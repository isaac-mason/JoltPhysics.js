// SPDX-FileCopyrightText: 2022 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include "JoltJS.h"

#include "Jolt/Physics/Body/BodyFilter.h"
#include "Jolt/Physics/Body/BodyLock.h"
#include "Jolt/Physics/Body/BodyLockInterface.h"
#include "Jolt/Physics/Collision/ShapeCast.h"
#include "Jolt/Physics/Collision/CollidePointResult.h"
#include "Jolt/Physics/Collision/PhysicsMaterialSimple.h"
#include "Jolt/Physics/Collision/GroupFilterTable.h"
#include "Jolt/Physics/Body/MassProperties.h"
#include "Jolt/Physics/Character/Character.h"
#include "Jolt/Physics/Vehicle/VehicleConstraint.h"
#include "Jolt/Physics/Vehicle/WheeledVehicleController.h"
#include "Jolt/Physics/Vehicle/MotorcycleController.h"
#include "Jolt/Physics/Vehicle/TrackedVehicleController.h"
#include "Jolt/Physics/Vehicle/VehicleCollisionTester.h"
#include "Jolt/Core/Color.h"

#include <emscripten/bind.h>
#include <sstream>

using namespace emscripten;

// Objects JS references but never owns, whose dtor is inaccessible (Body is destroyed
// only via BodyInterface), get a no-op raw_destructor: jolt_class_<T> compiles and
// JS .delete() is a harmless no-op.
namespace emscripten { namespace internal {
template<> inline void raw_destructor<JPH::Body>(JPH::Body *) {}
}} // namespace emscripten::internal

// Body IDs cross as a plain JS number: bound signatures use uint32 + these helpers,
// never BodyID (embind won't marshal a custom type as a bare number).
static inline JPH::BodyID toBodyID(JPH::uint32 v) { return JPH::BodyID(v); }
static inline JPH::uint32 fromBodyID(const JPH::BodyID &id) { return id.GetIndexAndSequenceNumber(); }
static inline JPH::SubShapeID toSubShapeID(JPH::uint32 v) { JPH::SubShapeID id; id.SetValue(v); return id; }

// Teach embind about Jolt's intrusive ref-counting: it has no built-in support for
// Ref<T> / RefConst<T>, so these trait specializations let it hold them as smart pointers.
// With them, refcounted types (deriving RefTarget) register via .smart_ptr<Ref<T>>("TRef")
// and embind drives AddRef/Release automatically — never bind those raw. Such types' ctors
// also return Ref<T> (not a raw type-list ctor) so a GC'd JS handle drops one ref (the
// object survives while C++ still holds a RefConst) instead of leaking as a raw handle
// embind never finalizes.
namespace emscripten {
template<typename T>
struct smart_ptr_trait<JPH::Ref<T>> {
    typedef JPH::Ref<T> pointer_type;
    typedef T element_type;
    static sharing_policy get_sharing_policy() { return sharing_policy::INTRUSIVE; }
    static T* get(const JPH::Ref<T>& p) { return p.GetPtr(); }
    static JPH::Ref<T> share(T* p) { return JPH::Ref<T>(p); }
    static pointer_type* construct_null() { return new pointer_type(); }
};
template<typename T>
struct smart_ptr_trait<JPH::RefConst<T>> {
    typedef JPH::RefConst<T> pointer_type;
    typedef const T element_type;
    static sharing_policy get_sharing_policy() { return sharing_policy::INTRUSIVE; }
    static const T* get(const JPH::RefConst<T>& p) { return p.GetPtr(); }
    static JPH::RefConst<T> share(const T* p) { return JPH::RefConst<T>(p); }
    static pointer_type* construct_null() { return new pointer_type(); }
};
} // namespace emscripten

// Everything below is our internal binding machinery — helpers, the jolt_class_ wrapper,
// the out_function DSL (nested outfn), the JS-subclassable callback wrappers and the
// buffered-read classes. Grouped in namespace `jsbind` so it doesn't crowd the global
// namespace alongside the `using namespace emscripten/JPH` soup; pulled into scope with
// `using namespace jsbind;` inside EMSCRIPTEN_BINDINGS.
namespace jsbind {

// Minimal JSON string escaping for the _*Meta serializers.
static std::string jStr(const std::string& s) {
    std::string o; o.reserve(s.size() + 2); o += '"';
    for (char c : s) {
        if      (c == '"')  o += "\\\"";
        else if (c == '\\') o += "\\\\";
        else if (c < 0x20)  { o += "\\u00";
                               o += "0123456789abcdef"[((unsigned char)c >> 4) & 0xf];
                               o += "0123456789abcdef"[ (unsigned char)c       & 0xf]; }
        else o += c;
    }
    o += '"'; return o;
}

// Shared scratch for the out-param read paradigm (facade.js copies from here).
// Static data => stable address, unaffected by heap growth; single-threaded JS
// reads it synchronously right after each write, so one slot is safe.
static float sOutScratch[32];

static inline void WriteVec3(JPH::Vec3Arg v, uintptr_t out) {
    auto* o = reinterpret_cast<float*>(out);
    o[0]=(float)v.GetX(); o[1]=(float)v.GetY(); o[2]=(float)v.GetZ();
}
static inline void WriteVec2(const JPH::Vector<2> &v, uintptr_t out) {
    auto* o = reinterpret_cast<float*>(out);
    o[0] = (float)v[0]; o[1] = (float)v[1];
}
static inline void WriteQuat(JPH::Quat q, uintptr_t out) {
    auto* o = reinterpret_cast<float*>(out);
    o[0]=(float)q.GetX(); o[1]=(float)q.GetY(); o[2]=(float)q.GetZ(); o[3]=(float)q.GetW();
}
template<typename M>
static inline void WriteMat4(const M& m, uintptr_t out) {
    auto* o = reinterpret_cast<float*>(out);
    for (int c = 0; c < 4; c++) {
        JPH::Vec4 col = m.GetColumn4(c);
        o[c*4+0]=(float)col.GetX(); o[c*4+1]=(float)col.GetY();
        o[c*4+2]=(float)col.GetZ(); o[c*4+3]=(float)col.GetW();
    }
}
static inline void WriteAABox(const JPH::AABox& b, uintptr_t out) {
    auto* o = reinterpret_cast<float*>(out);
    o[0]=(float)b.mMin.GetX(); o[1]=(float)b.mMin.GetY(); o[2]=(float)b.mMin.GetZ();
    o[3]=(float)b.mMax.GetX(); o[4]=(float)b.mMax.GetY(); o[5]=(float)b.mMax.GetZ();
}
// value_array element i (column-major) of a Mat44: operator()(row, col) = (i%4, i/4)
#define MAT_ELEM(i) element(+[](const JPH::Mat44 &m) { return m(i % 4, i / 4); }, \
    +[](JPH::Mat44 &m, float v) { m(i % 4, i / 4) = v; })

// Binding-site metadata consumed by postprocess-tsd.mjs. Filled during EMSCRIPTEN_BINDINGS via the
// jolt_class_ DSL (.out_function / .constructor / .function) and serialized by the _*Meta() getters:
// out-param value types, constructor param names + optional `val` types, and `val`-return types.
struct OutEntry  { std::string cls, method; std::vector<int> sizes; std::vector<std::string> tsTypes; int trailing; };
struct CtorParam { std::string name, tsType; };   // tsType empty => embind infers the type
struct CtorEntry { std::string cls; std::vector<CtorParam> params; };
struct RetEntry  { std::string cls, method, tsType; };
static std::vector<OutEntry>  sOutRegistry;
static std::vector<CtorEntry> sCtorRegistry;
static std::vector<RetEntry>  sRetRegistry;

// Split a ctor param spec on TOP-LEVEL commas (nesting-aware: <> {} [] ()), then split each token
// on its first ':' into {name, tsType}. tsType is empty when unannotated (embind infers the type).
static std::vector<CtorParam> splitParams(const char* s) {
    auto trim = [](std::string t) -> std::string {
        size_t a = t.find_first_not_of(' ');
        return a == std::string::npos ? std::string() : t.substr(a, t.find_last_not_of(' ') - a + 1);
    };
    std::vector<CtorParam> out;
    std::string cur;
    int depth = 0;
    auto flush = [&]() {
        std::string tok = trim(cur); cur.clear();
        if (tok.empty()) return;
        size_t colon = tok.find(':');
        if (colon == std::string::npos) out.push_back({tok, ""});
        else out.push_back({trim(tok.substr(0, colon)), trim(tok.substr(colon + 1))});
    };
    for (; *s; ++s) {
        char ch = *s;
        if (ch == '<' || ch == '{' || ch == '[' || ch == '(') { ++depth; cur += ch; }
        else if (ch == '>' || ch == '}' || ch == ']' || ch == ')') { --depth; cur += ch; }
        else if (ch == ',' && depth == 0) flush();
        else cur += ch;
    }
    flush();
    return out;
}

// ---- out_function descriptor DSL ----
// Referenced qualified at call sites: out_desc::Vec3 (an out param of that value type),
// out_desc::Pass (a forwarded input arg), out_desc::ret::Vec3 (return-type override). Each
// *Tag carries a value type's float count + TS type name — add a value type = add a Tag.
namespace out_desc {

struct Float2Tag{ static constexpr const char* tsType = "Float2"; static constexpr int size = 2;  };
struct Vec3Tag  { static constexpr const char* tsType = "Vec3";  static constexpr int size = 3;  };
struct QuatTag  { static constexpr const char* tsType = "Quat";  static constexpr int size = 4;  };
struct Vec4Tag  { static constexpr const char* tsType = "Vec4";  static constexpr int size = 4;  };
struct AABoxTag { static constexpr const char* tsType = "AABox"; static constexpr int size = 6;  };
struct Mat44Tag { static constexpr const char* tsType = "Mat44"; static constexpr int size = 16; };

template<typename Tag> struct out_t {};
struct pass_t {};
inline constexpr pass_t Pass{};   // a forwarded (non-out) input arg

// out-param descriptors — bare value-type names
inline constexpr out_t<Float2Tag> Float2 {};
inline constexpr out_t<Vec3Tag>  Vec3  {};
inline constexpr out_t<QuatTag>  Quat  {};
inline constexpr out_t<Vec4Tag>  Vec4  {};
inline constexpr out_t<AABoxTag> AABox {};
inline constexpr out_t<Mat44Tag> Mat44 {};

// ---- function-pointer arity (works with +[] non-capturing lambdas) ----
template<typename F> struct FnArity;
template<typename R, typename... A>
struct FnArity<R(*)(A...)> : std::integral_constant<int, (int)sizeof...(A)> {};

// ---- descriptor pack: count outs/trailing, collect sizes and TS type names ----
// Descriptor order: out_t<Tag>... then pass_t(Pass)..., one per JS-facing signature slot.
template<typename... Ds> struct DescInfo {
    static constexpr int nOuts = 0, trailing = 0, totalSize = 0;
    static void sizes(std::vector<int>&) {}
    static void tsTypes(std::vector<std::string>&) {}
};
template<typename Tag, typename... Rest> struct DescInfo<out_t<Tag>, Rest...> {
    static constexpr int nOuts     = 1 + DescInfo<Rest...>::nOuts;
    static constexpr int trailing  = DescInfo<Rest...>::trailing;
    static constexpr int totalSize = Tag::size + DescInfo<Rest...>::totalSize;
    static void sizes(std::vector<int>& v)           { v.push_back(Tag::size);    DescInfo<Rest...>::sizes(v); }
    static void tsTypes(std::vector<std::string>& v)  { v.push_back(Tag::tsType); DescInfo<Rest...>::tsTypes(v); }
};
template<typename... Rest> struct DescInfo<pass_t, Rest...> {
    static constexpr int nOuts     = DescInfo<Rest...>::nOuts;
    static constexpr int trailing  = 1 + DescInfo<Rest...>::trailing;
    static constexpr int totalSize = DescInfo<Rest...>::totalSize;
    static void sizes(std::vector<int>& v)           { DescInfo<Rest...>::sizes(v); }
    static void tsTypes(std::vector<std::string>& v)  { DescInfo<Rest...>::tsTypes(v); }
};

}  // namespace out_desc

// ---- thin wrapper over emscripten::class_<T[, Base]> ----
template<typename T, typename Base = emscripten::internal::NoBaseClass>
struct jolt_class_ {
    emscripten::class_<T, Base> c;
    std::string name_;

    explicit jolt_class_(const char* name) : c(name), name_(name) {}

    // Regular function passthrough. Optional TS-style return type after the sig, e.g.
    //   .function("GetBodies: number[]", fn) / .function("GetHeights(x, y): Float32Array", fn)
    // recorded in sRetRegistry so postprocess-tsd can retype the `any` emit-tsd gives a
    // val-returning lambda. Split on the first ':' (embind sigs never contain one).
    template<typename Fn, typename... Opts>
    jolt_class_& function(const char* sig, Fn fn, Opts&&... opts) {
        if (const char* colon = strchr(sig, ':')) {
            std::string embSig(sig, colon);
            while (!embSig.empty() && embSig.back() == ' ') embSig.pop_back();
            std::string method = embSig.substr(0, embSig.find('('));
            while (!method.empty() && method.back() == ' ') method.pop_back();
            const char* ret = colon + 1;
            while (*ret == ' ') ++ret;
            c.function(embSig.c_str(), fn, std::forward<Opts>(opts)...);
            sRetRegistry.push_back({name_, method, std::string(ret)});
        } else {
            c.function(sig, fn, std::forward<Opts>(opts)...);
        }
        return *this;
    }

    // .out_function("Method(out, a)", out_desc::Vec3, out_desc::Pass, +[lambda])
    // Descriptors declare the JS-facing shape; lambda is the Into implementation.
    template<typename... Args>
    jolt_class_& out_function(const char* sig, Args... args) {
        auto tup = std::make_tuple(args...);
        constexpr std::size_t N = sizeof...(Args);
        return out_fn_dispatch_(sig, std::get<N-1>(tup), tup, std::make_index_sequence<N-1>{});
    }

    template<typename Fn, typename Tup, std::size_t... I>
    jolt_class_& out_fn_dispatch_(const char* sig, Fn fn, const Tup& tup, std::index_sequence<I...>) {
        return out_fn_core_(sig, fn, std::get<I>(tup)...);
    }

    template<typename Fn, typename... Descs>
    jolt_class_& out_fn_core_(const char* sig, Fn fn, Descs...) {
        using DI = out_desc::DescInfo<Descs...>;
        static_assert(out_desc::FnArity<Fn>::value == 1 + DI::nOuts + DI::trailing,
            "out_function: lambda arity != 1 + outs + trailing — check descriptors");
        static_assert(DI::totalSize <= 32,
            "out_function: combined out sizes exceed sOutScratch[32] — bump sOutScratch or split");
        const char* paren = strchr(sig, '(');
        // Strip trailing whitespace from the method name (guards against "Foo (out)" typos).
        std::string method = paren ? std::string(sig, paren) : std::string(sig);
        while (!method.empty() && method.back() == ' ') method.pop_back();
        std::string intoSig = method + "Into" + (paren ? paren : "()");
        c.function(intoSig.c_str(), fn);
        std::vector<int> szVec; DI::sizes(szVec);
        std::vector<std::string> tsVec; DI::tsTypes(tsVec);
        sOutRegistry.push_back({name_, method, szVec, tsVec, DI::trailing});
        return *this;
    }

    // Passthroughs for jolt_class_<T> methods used by out-param classes
    template<typename P>
    jolt_class_& smart_ptr(const char* n) { c.template smart_ptr<P>(n); return *this; }

    // Named constructors: register param names into sCtorRegistry.
    // Use for any constructor that emits _0/_1/... in --emit-tsd output.

    // type-list constructor with param names: .constructor<Vec3>("halfExtent")
    template<typename... Args>
    jolt_class_& constructor(const char* params) {
        c.template constructor<Args...>();
        sCtorRegistry.push_back({name_, splitParams(params)});
        return *this;
    }

    // factory lambda with param names: .constructor("x, y", +[](int x, int y){...})
    template<typename F>
    jolt_class_& constructor(const char* params, F fn) {
        c.constructor(fn);
        sCtorRegistry.push_back({name_, splitParams(params)});
        return *this;
    }

    // factory lambda + options (e.g. allow_raw_pointers()) with param names
    template<typename F, typename... Opts>
    jolt_class_& constructor(const char* params, F fn, Opts&&... opts) {
        c.constructor(fn, std::forward<Opts>(opts)...);
        sCtorRegistry.push_back({name_, splitParams(params)});
        return *this;
    }

    template<typename... Args>
    jolt_class_& constructor() { c.template constructor<Args...>(); return *this; }

    template<typename F>
    jolt_class_& constructor(F fn) { c.constructor(fn); return *this; }

    template<typename F, typename... Opts>
    jolt_class_& constructor(F fn, Opts&&... opts) { c.constructor(fn, std::forward<Opts>(opts)...); return *this; }

    template<typename Sub>
    jolt_class_& allow_subclass(const char* n, const char* rn) {
        c.template allow_subclass<Sub>(n, rn); return *this;
    }
    template<typename Sub>
    jolt_class_& allow_subclass(const char* n) {
        c.template allow_subclass<Sub>(n); return *this;
    }

    template<typename G, typename S>
    jolt_class_& property(const char* n, G g, S s) { c.property(n, g, s); return *this; }

    template<typename G>
    jolt_class_& property(const char* n, G g) { c.property(n, g); return *this; }

    template<typename Fn, typename... Opts>
    jolt_class_& class_function(const char* sig, Fn fn, Opts&&... opts) {
        c.class_function(sig, fn, std::forward<Opts>(opts)...); return *this;
    }
};
// Jolt's Array<T> (== std::vector<T, STLAllocator<T>>) doesn't match embind's
// std::allocator-only register_vector, so containers are hand-registered here.
// Value-type elements are returned by copy.
template<typename T>
static void registerJoltArray(const char *inName) {
    using namespace emscripten;
    class_<JPH::Array<T>>(inName)
        .template constructor<>()
        .function("size", +[](const JPH::Array<T> &a) { return (JPH::uint32)a.size(); })
        .function("empty", +[](const JPH::Array<T> &a) { return a.empty(); })
        .function("at(index)", +[](const JPH::Array<T> &a, JPH::uint32 i) { return a[i]; })
        .function("push_back(value)", +[](JPH::Array<T> &a, T v) { a.push_back(v); })
        .function("reserve(count)", +[](JPH::Array<T> &a, JPH::uint32 n) { a.reserve(n); })
        .function("clear", +[](JPH::Array<T> &a) { a.clear(); });
}

// Register a closest-hit / all-hit convenience collector for a query CollectorType.
// The collector upcasts to CollectorType (the query param); results read via GetHit /
// size()+at(index) returning non-owning handles into the collector.
template<typename CollectorType>
static void registerClosestHit(const char *inName) {
    using namespace emscripten;
    using C = JPH::ClosestHitCollisionCollector<CollectorType>;
    class_<C, base<CollectorType>>(inName)
        .template constructor<>()
        .function("HadHit", &C::HadHit)
        .function("Reset", &C::Reset)
        .function("GetHit", +[](C &c) { return &c.mHit; }, allow_raw_pointers());
}
template<typename CollectorType>
static void registerAllHit(const char *inName) {
    using namespace emscripten;
    using C = JPH::AllHitCollisionCollector<CollectorType>;
    class_<C, base<CollectorType>>(inName)
        .template constructor<>()
        .function("HadHit", &C::HadHit)
        .function("Reset", &C::Reset)
        .function("Sort", &C::Sort)
        .function("size", +[](const C &c) { return (JPH::uint32)c.mHits.size(); })
        .function("at(index)", +[](C &c, JPH::uint32 i) { return &c.mHits[i]; }, allow_raw_pointers());
}

// JS-subclassable callbacks use wrapper + call<>(): class args forward as non-owning
// pointer handles, value types (RVec3Arg -> Vec3) by value, and mutable& args
// (ContactSettings&) as pointers so JS writes propagate.
// handleOf: non-owning val handle to a C++ object. const_cast because embind can't invoke
// methods on a const raw-pointer handle (JS only reads).
template<typename T> static val handleOf(const T &x) { return val(const_cast<T *>(&x), allow_raw_pointers()); }

struct ContactListenerWrapper : public wrapper<ContactListener> {
    // All four callbacks are OPTIONAL: probe once at construction so the per-callback path
    // is a bool test; unimplemented ones keep Jolt's default. This is the per-contact JS
    // path — use ContactListenerBuffer for the zero-crossing bulk path.
    bool mHasValidate = false, mHasAdded = false, mHasPersisted = false, mHasRemoved = false;
    template<typename... Args>
    ContactListenerWrapper(val &&v, Args &&...args) : wrapper(val(v), std::forward<Args>(args)...) {
        mHasValidate  = !v["OnContactValidate"].isUndefined();
        mHasAdded     = !v["OnContactAdded"].isUndefined();
        mHasPersisted = !v["OnContactPersisted"].isUndefined();
        mHasRemoved   = !v["OnContactRemoved"].isUndefined();
    }
    ValidateResult OnContactValidate(const Body &b1, const Body &b2, RVec3Arg inBaseOffset, const CollideShapeResult &r) override {
        if (!mHasValidate) return ValidateResult::AcceptAllContactsForThisBodyPair;
        return call<ValidateResult>("OnContactValidate", handleOf(b1), handleOf(b2), Vec3(inBaseOffset), handleOf(r));
    }
    void OnContactAdded(const Body &b1, const Body &b2, const ContactManifold &m, ContactSettings &s) override {
        if (mHasAdded) call<void>("OnContactAdded", handleOf(b1), handleOf(b2), handleOf(m), handleOf(s));
    }
    void OnContactPersisted(const Body &b1, const Body &b2, const ContactManifold &m, ContactSettings &s) override {
        if (mHasPersisted) call<void>("OnContactPersisted", handleOf(b1), handleOf(b2), handleOf(m), handleOf(s));
    }
    void OnContactRemoved(const SubShapeIDPair &p) override {
        if (mHasRemoved) call<void>("OnContactRemoved",
            (uint32)p.GetBody1ID().GetIndexAndSequenceNumber(), (uint32)p.GetSubShapeID1().GetValue(),
            (uint32)p.GetBody2ID().GetIndexAndSequenceNumber(), (uint32)p.GetSubShapeID2().GetValue());
    }
};

// Contact listener for the MULTI-THREADED build. Jolt invokes contact callbacks from
// worker threads, where a JS callback captured on the main thread (the allow_subclass
// `val`) isn't valid. Instead this dispatches to a THREAD-LOCAL global function that
// each worker installs (globalThis.__joltOnContactAdded / __joltOnContactPersisted),
// passing raw pointers the worker reads/writes via the getContact*/rotateVectorByBody/
// setContact* helpers. The listener object is created + installed on the main thread and
// shared; only the per-thread globals differ. (Single-threaded builds can use it too —
// just install the globals on the main thread.)
struct ThreadedContactListener : public ContactListener {
    static void dispatch(const char *name, const Body &b1, const Body &b2, ContactSettings &s) {
        val g = val::global(name);
        if (g.typeOf().as<std::string>() == "function")
            g((uintptr_t)&b1, (uintptr_t)&b2, (uintptr_t)&s);
    }
    void OnContactAdded(const Body &b1, const Body &b2, const ContactManifold &, ContactSettings &s) override {
        dispatch("__joltOnContactAdded", b1, b2, s);
    }
    void OnContactPersisted(const Body &b1, const Body &b2, const ContactManifold &, ContactSettings &s) override {
        dispatch("__joltOnContactPersisted", b1, b2, s);
    }
};

// Body activation listener (bodies waking / going to sleep). Both callbacks are
// pure-virtual in Jolt, so the JS impl must provide both. Low-frequency events.
struct BodyActivationListenerWrapper : public wrapper<BodyActivationListener> {
    EMSCRIPTEN_WRAPPER(BodyActivationListenerWrapper);
    void OnBodyActivated(const BodyID &id, uint64 userData) override {
        return call<void>("OnBodyActivated", (uint32)id.GetIndexAndSequenceNumber(), userData);
    }
    void OnBodyDeactivated(const BodyID &id, uint64 userData) override {
        return call<void>("OnBodyDeactivated", (uint32)id.GetIndexAndSequenceNumber(), userData);
    }
};

// CharacterVirtual contact events. Same optional-callback pattern as
// ContactListenerWrapper: probe once which methods the JS impl provides. Vec3/RVec3
// args marshal as plain arrays; the character + settings pass as non-owning handles
// so JS can read the character and tweak ioSettings (mCanPushCharacter, ...). The
// io-velocity callbacks can't mutate a Vec3& from JS (Vec3 is a value array), so JS
// RETURNS the new velocity instead. The character-vs-character variants aren't surfaced.
struct CharacterContactListenerWrapper : public wrapper<CharacterContactListener> {
    bool mHasValidate = false, mHasAdded = false, mHasPersisted = false, mHasRemoved = false;
    bool mHasAdjustBodyVelocity = false, mHasContactSolve = false;
    bool mHasCharValidate = false, mHasCharAdded = false, mHasCharPersisted = false,
         mHasCharRemoved = false, mHasCharContactSolve = false;
    template<typename... Args>
    CharacterContactListenerWrapper(val &&v, Args &&...args) : wrapper(val(v), std::forward<Args>(args)...) {
        mHasValidate  = !v["OnContactValidate"].isUndefined();
        mHasAdded     = !v["OnContactAdded"].isUndefined();
        mHasPersisted = !v["OnContactPersisted"].isUndefined();
        mHasRemoved   = !v["OnContactRemoved"].isUndefined();
        mHasAdjustBodyVelocity = !v["OnAdjustBodyVelocity"].isUndefined();
        mHasContactSolve       = !v["OnContactSolve"].isUndefined();
        mHasCharValidate     = !v["OnCharacterContactValidate"].isUndefined();
        mHasCharAdded        = !v["OnCharacterContactAdded"].isUndefined();
        mHasCharPersisted    = !v["OnCharacterContactPersisted"].isUndefined();
        mHasCharRemoved      = !v["OnCharacterContactRemoved"].isUndefined();
        mHasCharContactSolve = !v["OnCharacterContactSolve"].isUndefined();
    }
    // Character-vs-character variants: the second party is another CharacterVirtual (non-owning
    // handle). SubShapeID / CharacterID marshal as uint32.
    bool OnCharacterContactValidate(const CharacterVirtual *c, const CharacterVirtual *other,
                                    const SubShapeID &s2) override {
        if (!mHasCharValidate) return true;
        return call<bool>("OnCharacterContactValidate", handleOf(*c), handleOf(*other), (uint32)s2.GetValue());
    }
    void OnCharacterContactAdded(const CharacterVirtual *c, const CharacterVirtual *other,
                                 const SubShapeID &s2, RVec3Arg pos, Vec3Arg normal,
                                 CharacterContactSettings &io) override {
        if (mHasCharAdded) call<void>("OnCharacterContactAdded", handleOf(*c), handleOf(*other),
            (uint32)s2.GetValue(), Vec3(pos), normal, handleOf(io));
    }
    void OnCharacterContactPersisted(const CharacterVirtual *c, const CharacterVirtual *other,
                                     const SubShapeID &s2, RVec3Arg pos, Vec3Arg normal,
                                     CharacterContactSettings &io) override {
        if (mHasCharPersisted) call<void>("OnCharacterContactPersisted", handleOf(*c), handleOf(*other),
            (uint32)s2.GetValue(), Vec3(pos), normal, handleOf(io));
    }
    // The "removed" variant takes the OTHER character's CharacterID by value (it may already be
    // deleted). Marshal as uint32.
    void OnCharacterContactRemoved(const CharacterVirtual *c, const CharacterID &otherID,
                                   const SubShapeID &s2) override {
        if (mHasCharRemoved) call<void>("OnCharacterContactRemoved", handleOf(*c),
            (uint32)otherID.GetValue(), (uint32)s2.GetValue());
    }
    void OnCharacterContactSolve(const CharacterVirtual *c, const CharacterVirtual *other,
                                 const SubShapeID &s2, RVec3Arg pos, Vec3Arg normal,
                                 Vec3Arg contactVel, const PhysicsMaterial *,
                                 Vec3Arg charVel, Vec3 &ioNewCharVel) override {
        if (!mHasCharContactSolve) return;
        val r = call<val>("OnCharacterContactSolve", handleOf(*c), handleOf(*other),
            (uint32)s2.GetValue(), Vec3(pos), Vec3(normal), Vec3(contactVel), Vec3(charVel),
            Vec3(ioNewCharVel));
        if (!r.isUndefined() && !r.isNull())
            ioNewCharVel = Vec3(r[0].as<float>(), r[1].as<float>(), r[2].as<float>());
    }
    // OnAdjustBodyVelocity(character, body2, linearVelocity, angularVelocity) -> optionally
    // return { linear?: [x,y,z], angular?: [x,y,z] } to override (e.g. a conveyor belt).
    void OnAdjustBodyVelocity(const CharacterVirtual *c, const Body &b2, Vec3 &ioLin, Vec3 &ioAng) override {
        if (!mHasAdjustBodyVelocity) return;
        val r = call<val>("OnAdjustBodyVelocity", handleOf(*c), handleOf(b2), Vec3(ioLin), Vec3(ioAng));
        if (r.isUndefined() || r.isNull()) return;
        val lin = r["linear"];
        if (!lin.isUndefined() && !lin.isNull()) ioLin = Vec3(lin[0].as<float>(), lin[1].as<float>(), lin[2].as<float>());
        val ang = r["angular"];
        if (!ang.isUndefined() && !ang.isNull()) ioAng = Vec3(ang[0].as<float>(), ang[1].as<float>(), ang[2].as<float>());
    }
    // OnContactSolve(character, bodyID2, subShapeID2, contactPosition, contactNormal,
    //   contactVelocity, characterVelocity, newCharacterVelocity) -> optionally return a
    // [x,y,z] to override the new character velocity (e.g. anti-sliding on gentle slopes).
    void OnContactSolve(const CharacterVirtual *c, const BodyID &b2, const SubShapeID &s2,
                        RVec3Arg pos, Vec3Arg normal, Vec3Arg contactVel, const PhysicsMaterial *,
                        Vec3Arg charVel, Vec3 &ioNewCharVel) override {
        if (!mHasContactSolve) return;
        val r = call<val>("OnContactSolve", handleOf(*c),
            (uint32)b2.GetIndexAndSequenceNumber(), (uint32)s2.GetValue(),
            Vec3(pos), Vec3(normal), Vec3(contactVel), Vec3(charVel), Vec3(ioNewCharVel));
        if (!r.isUndefined() && !r.isNull())
            ioNewCharVel = Vec3(r[0].as<float>(), r[1].as<float>(), r[2].as<float>());
    }
    bool OnContactValidate(const CharacterVirtual *c, const BodyID &b2, const SubShapeID &s2) override {
        if (!mHasValidate) return true;
        return call<bool>("OnContactValidate", handleOf(*c),
            (uint32)b2.GetIndexAndSequenceNumber(), (uint32)s2.GetValue());
    }
    void OnContactAdded(const CharacterVirtual *c, const BodyID &b2, const SubShapeID &s2,
                        RVec3Arg pos, Vec3Arg normal, CharacterContactSettings &io) override {
        if (mHasAdded) call<void>("OnContactAdded", handleOf(*c),
            (uint32)b2.GetIndexAndSequenceNumber(), (uint32)s2.GetValue(), Vec3(pos), normal, handleOf(io));
    }
    void OnContactPersisted(const CharacterVirtual *c, const BodyID &b2, const SubShapeID &s2,
                            RVec3Arg pos, Vec3Arg normal, CharacterContactSettings &io) override {
        if (mHasPersisted) call<void>("OnContactPersisted", handleOf(*c),
            (uint32)b2.GetIndexAndSequenceNumber(), (uint32)s2.GetValue(), Vec3(pos), normal, handleOf(io));
    }
    void OnContactRemoved(const CharacterVirtual *c, const BodyID &b2, const SubShapeID &s2) override {
        if (mHasRemoved) call<void>("OnContactRemoved", handleOf(*c),
            (uint32)b2.GetIndexAndSequenceNumber(), (uint32)s2.GetValue());
    }
};

// JS-subclassable character-vs-character collision interface. Both virtuals are const and hand
// the character + a collector to JS (all non-owning handles). Most users prefer the built-in
// CharacterVsCharacterCollisionSimple; this is for custom broad-phase logic in JS.
struct CharacterVsCharacterCollisionWrapper : public wrapper<CharacterVsCharacterCollision> {
    bool mHasCollide = false, mHasCast = false;
    template<typename... Args>
    CharacterVsCharacterCollisionWrapper(val &&v, Args &&...args) : wrapper(val(v), std::forward<Args>(args)...) {
        mHasCollide = !v["CollideCharacter"].isUndefined();
        mHasCast    = !v["CastCharacter"].isUndefined();
    }
    void CollideCharacter(const CharacterVirtual *c, RMat44Arg comTransform,
                          const CollideShapeSettings &settings, RVec3Arg baseOffset,
                          CollideShapeCollector &collector) const override {
        if (!mHasCollide) return;
        RMat44 t = comTransform;
        const_cast<CharacterVsCharacterCollisionWrapper *>(this)->call<void>(
            "CollideCharacter", handleOf(*c), handleOf(t), handleOf(settings),
            Vec3(baseOffset), handleOf(collector));
    }
    void CastCharacter(const CharacterVirtual *c, RMat44Arg comTransform, Vec3Arg direction,
                       const ShapeCastSettings &settings, RVec3Arg baseOffset,
                       CastShapeCollector &collector) const override {
        if (!mHasCast) return;
        RMat44 t = comTransform;
        const_cast<CharacterVsCharacterCollisionWrapper *>(this)->call<void>(
            "CastCharacter", handleOf(*c), handleOf(t), Vec3(direction),
            handleOf(settings), Vec3(baseOffset), handleOf(collector));
    }
};

// JS-subclassable soft-body contact listener. Both callbacks are OPTIONAL (Jolt provides
// non-pure-virtual defaults), so probe once at construction. OnSoftBodyContactValidate returns a
// SoftBodyValidateResult (int enum); ioSettings is a mutable non-owning handle.
struct SoftBodyContactListenerWrapper : public wrapper<SoftBodyContactListener> {
    bool mHasValidate = false, mHasAdded = false;
    template<typename... Args>
    SoftBodyContactListenerWrapper(val &&v, Args &&...args) : wrapper(val(v), std::forward<Args>(args)...) {
        mHasValidate = !v["OnSoftBodyContactValidate"].isUndefined();
        mHasAdded    = !v["OnSoftBodyContactAdded"].isUndefined();
    }
    SoftBodyValidateResult OnSoftBodyContactValidate(const Body &inSoftBody, const Body &inOtherBody, SoftBodyContactSettings &ioSettings) override {
        if (!mHasValidate) return SoftBodyValidateResult::AcceptContact;
        return call<SoftBodyValidateResult>("OnSoftBodyContactValidate", handleOf(inSoftBody), handleOf(inOtherBody), handleOf(ioSettings));
    }
    void OnSoftBodyContactAdded(const Body &inSoftBody, const SoftBodyManifold &inManifold) override {
        if (mHasAdded) call<void>("OnSoftBodyContactAdded", handleOf(inSoftBody), handleOf(inManifold));
    }
};

// JS-subclassable path for PathConstraint (e.g. a circle or an SVG-derived curve).
// GetPointOnPath has four Vec3& out-params which JS can't mutate, so the JS impl returns
// { position, tangent, normal, binormal } (plain arrays) and the wrapper writes them back.
struct PathConstraintPathWrapper : public wrapper<PathConstraintPath> {
    EMSCRIPTEN_WRAPPER(PathConstraintPathWrapper);
    float GetPathMaxFraction() const override { return call<float>("GetPathMaxFraction"); }
    float GetClosestPoint(Vec3Arg inPosition, float inFractionHint) const override {
        return call<float>("GetClosestPoint", Vec3(inPosition), inFractionHint);
    }
    void GetPointOnPath(float inFraction, Vec3 &outPosition, Vec3 &outTangent, Vec3 &outNormal, Vec3 &outBinormal) const override {
        val r = call<val>("GetPointOnPath", inFraction);
        auto rd = [](val a) { return Vec3(a[0].as<float>(), a[1].as<float>(), a[2].as<float>()); };
        outPosition = rd(r["position"]); outTangent = rd(r["tangent"]);
        outNormal = rd(r["normal"]); outBinormal = rd(r["binormal"]);
    }
};

// JS-subclassable pre-step hook (apply custom forces, etc.). Fires once per Update
// before the simulation step. The system is passed as a non-owning handle.
struct PhysicsStepListenerWrapper : public wrapper<PhysicsStepListener> {
    EMSCRIPTEN_WRAPPER(PhysicsStepListenerWrapper);
    // primitives only (call<> can't marshal a raw pointer arg); a JS listener
    // captures its PhysicsSystem in closure, so the signature is OnStep(dt, first, last).
    void OnStep(const PhysicsStepListenerContext &inContext) override {
        return call<void>("OnStep", inContext.mDeltaTime, inContext.mIsFirstStep, inContext.mIsLastStep);
    }
};

// JS-subclassable vehicle friction/step callbacks. Subclasses VehicleConstraintCallbacksEm
// (JoltJS.h): SetVehicleConstraint() wires the four Jolt std::function slots to these virtuals.
// GetCombinedFriction is invoked twice per wheel (longitudinal then lateral) — a JS function
// can't mutate the two float& refs in one call, so it returns the new value. SubShapeID -> uint32.
struct VehicleConstraintCallbacksWrapper : public wrapper<VehicleConstraintCallbacksEm> {
    EMSCRIPTEN_WRAPPER(VehicleConstraintCallbacksWrapper);
    float GetCombinedFriction(unsigned int inWheelIndex, ETireFrictionDirection inDir, float inTireFriction, const Body &inBody2, const SubShapeID &inSub2) override {
        return call<float>("GetCombinedFriction", inWheelIndex, inDir, inTireFriction, handleOf(inBody2), (uint32)inSub2.GetValue());
    }
    void OnPreStepCallback(VehicleConstraint &v, const PhysicsStepListenerContext &c) override    { call<void>("OnPreStepCallback",    handleOf(v), handleOf(c)); }
    void OnPostCollideCallback(VehicleConstraint &v, const PhysicsStepListenerContext &c) override { call<void>("OnPostCollideCallback", handleOf(v), handleOf(c)); }
    void OnPostStepCallback(VehicleConstraint &v, const PhysicsStepListenerContext &c) override   { call<void>("OnPostStepCallback",   handleOf(v), handleOf(c)); }
};

// JS-subclassable wheeled-controller tire-max-impulse callback. Subclasses
// WheeledVehicleControllerCallbacksEm (JoltJS.h): outResult is pre-filled with the default
// (friction * suspensionImpulse) then handed to JS as a mutable handle to overwrite.
struct WheeledVehicleControllerCallbacksWrapper : public wrapper<WheeledVehicleControllerCallbacksEm> {
    EMSCRIPTEN_WRAPPER(WheeledVehicleControllerCallbacksWrapper);
    void OnTireMaxImpulseCallback(uint inWheelIndex, TireMaxImpulseCallbackResult *outResult, float inSuspensionImpulse, float inLongitudinalFriction, float inLateralFriction, float inLongitudinalSlip, float inLateralSlip, float inDeltaTime) override {
        call<void>("OnTireMaxImpulseCallback", inWheelIndex, handleOf(*outResult), inSuspensionImpulse, inLongitudinalFriction, inLateralFriction, inLongitudinalSlip, inLateralSlip, inDeltaTime);
    }
};

// JS-subclassable broad-phase layer interface. BroadPhaseLayer isn't marshallable as a return,
// so JS implements { GetNumBroadPhaseLayers(), GetBPLayer(objectLayer) -> uint16 }. Both are
// pure-virtual in Jolt so a JS impl MUST provide them. NOTE: main-thread only (captures a JS
// val); use the C++ Table/Mask impls for the multi-threaded build.
struct BroadPhaseLayerInterfaceWrapper : public wrapper<BroadPhaseLayerInterface> {
    EMSCRIPTEN_WRAPPER(BroadPhaseLayerInterfaceWrapper);
    uint GetNumBroadPhaseLayers() const override {
        return const_cast<BroadPhaseLayerInterfaceWrapper *>(this)->call<uint>("GetNumBroadPhaseLayers");
    }
    BroadPhaseLayer GetBroadPhaseLayer(ObjectLayer inLayer) const override {
        uint16 v = const_cast<BroadPhaseLayerInterfaceWrapper *>(this)->call<uint16>("GetBPLayer", (uint32)inLayer);
        return BroadPhaseLayer((BroadPhaseLayer::Type)v);
    }
#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    const char *GetBroadPhaseLayerName(BroadPhaseLayer inLayer) const override { return "JSBroadPhaseLayer"; }
#endif
};
// JS-subclassable object-vs-broadphase filter. Single virtual ShouldCollide(objLayer, bpLayer).
struct ObjectVsBroadPhaseLayerFilterWrapper : public wrapper<ObjectVsBroadPhaseLayerFilter> {
    bool mHas = false;
    template<typename... Args>
    ObjectVsBroadPhaseLayerFilterWrapper(val &&v, Args &&...args) : wrapper(val(v), std::forward<Args>(args)...) { mHas = !v["ShouldCollide"].isUndefined(); }
    bool ShouldCollide(ObjectLayer inLayer1, BroadPhaseLayer inLayer2) const override {
        if (!mHas) return true;
        return const_cast<ObjectVsBroadPhaseLayerFilterWrapper *>(this)->call<bool>(
            "ShouldCollide", (uint32)inLayer1, (uint32)(BroadPhaseLayer::Type)inLayer2);
    }
};
// JS-subclassable object-layer pair filter. Single virtual ShouldCollide(objLayer1, objLayer2).
struct ObjectLayerPairFilterWrapper : public wrapper<ObjectLayerPairFilter> {
    bool mHas = false;
    template<typename... Args>
    ObjectLayerPairFilterWrapper(val &&v, Args &&...args) : wrapper(val(v), std::forward<Args>(args)...) { mHas = !v["ShouldCollide"].isUndefined(); }
    bool ShouldCollide(ObjectLayer inLayer1, ObjectLayer inLayer2) const override {
        if (!mHas) return true;
        return const_cast<ObjectLayerPairFilterWrapper *>(this)->call<bool>("ShouldCollide", (uint32)inLayer1, (uint32)inLayer2);
    }
};
// JS-subclassable save/restore filter for PhysicsSystem::SaveState/RestoreState. All four
// callbacks optional (Jolt defaults to accept). BodyID/Constraint -> uint32 id / handle.
struct StateRecorderFilterWrapper : public wrapper<StateRecorderFilter> {
    bool mHasBody = false, mHasConstraint = false, mHasSaveContact = false, mHasRestoreContact = false;
    template<typename... Args>
    StateRecorderFilterWrapper(val &&v, Args &&...args) : wrapper(val(v), std::forward<Args>(args)...) {
        mHasBody           = !v["ShouldSaveBody"].isUndefined();
        mHasConstraint     = !v["ShouldSaveConstraint"].isUndefined();
        mHasSaveContact    = !v["ShouldSaveContact"].isUndefined();
        mHasRestoreContact = !v["ShouldRestoreContact"].isUndefined();
    }
    bool ShouldSaveBody(const Body &inBody) const override {
        if (!mHasBody) return true;
        return const_cast<StateRecorderFilterWrapper *>(this)->call<bool>("ShouldSaveBody", handleOf(inBody));
    }
    bool ShouldSaveConstraint(const Constraint &inConstraint) const override {
        if (!mHasConstraint) return true;
        return const_cast<StateRecorderFilterWrapper *>(this)->call<bool>("ShouldSaveConstraint", handleOf(inConstraint));
    }
    bool ShouldSaveContact(const BodyID &inBody1, const BodyID &inBody2) const override {
        if (!mHasSaveContact) return true;
        return const_cast<StateRecorderFilterWrapper *>(this)->call<bool>("ShouldSaveContact",
            (uint32)inBody1.GetIndexAndSequenceNumber(), (uint32)inBody2.GetIndexAndSequenceNumber());
    }
    bool ShouldRestoreContact(const BodyID &inBody1, const BodyID &inBody2) const override {
        if (!mHasRestoreContact) return true;
        return const_cast<StateRecorderFilterWrapper *>(this)->call<bool>("ShouldRestoreContact",
            (uint32)inBody1.GetIndexAndSequenceNumber(), (uint32)inBody2.GetIndexAndSequenceNumber());
    }
};

// JS-subclassable query filters — `.implement({ ShouldCollide(...) { ... } })`. Each callback is
// probed at construction; unimplemented ones fall back to Jolt's default (accept). ShouldCollide
// is a const virtual, so call<> goes through a const_cast. NOTE: these fire per broad/narrow-phase
// candidate pair — a heavy JS predicate is costly; prefer IgnoreSingleBody/MultipleBodiesFilter or
// the layer filters when the rule is expressible that way.
struct BodyFilterWrapper : public wrapper<BodyFilter> {
    bool mHasShouldCollide = false, mHasShouldCollideLocked = false;
    template<typename... Args>
    BodyFilterWrapper(val &&v, Args &&...args) : wrapper(val(v), std::forward<Args>(args)...) {
        mHasShouldCollide       = !v["ShouldCollide"].isUndefined();
        mHasShouldCollideLocked = !v["ShouldCollideLocked"].isUndefined();
    }
    bool ShouldCollide(const BodyID &id) const override {
        if (!mHasShouldCollide) return true;
        return const_cast<BodyFilterWrapper *>(this)->call<bool>("ShouldCollide", (uint32)id.GetIndexAndSequenceNumber());
    }
    bool ShouldCollideLocked(const Body &b) const override {
        if (!mHasShouldCollideLocked) return true;
        return const_cast<BodyFilterWrapper *>(this)->call<bool>("ShouldCollideLocked", handleOf(b));
    }
};
struct ObjectLayerFilterWrapper : public wrapper<ObjectLayerFilter> {
    bool mHas = false;
    template<typename... Args>
    ObjectLayerFilterWrapper(val &&v, Args &&...args) : wrapper(val(v), std::forward<Args>(args)...) { mHas = !v["ShouldCollide"].isUndefined(); }
    bool ShouldCollide(ObjectLayer layer) const override {
        if (!mHas) return true;
        return const_cast<ObjectLayerFilterWrapper *>(this)->call<bool>("ShouldCollide", (uint32)layer);
    }
};
struct BroadPhaseLayerFilterWrapper : public wrapper<BroadPhaseLayerFilter> {
    bool mHas = false;
    template<typename... Args>
    BroadPhaseLayerFilterWrapper(val &&v, Args &&...args) : wrapper(val(v), std::forward<Args>(args)...) { mHas = !v["ShouldCollide"].isUndefined(); }
    bool ShouldCollide(BroadPhaseLayer layer) const override {
        if (!mHas) return true;
        return const_cast<BroadPhaseLayerFilterWrapper *>(this)->call<bool>("ShouldCollide", (uint32)(BroadPhaseLayer::Type)layer);
    }
};
// ShapeFilter has two ShouldCollide overloads (single-shape and pair); JS can't overload by arity,
// so the pair variant is surfaced as ShouldCollidePair.
struct ShapeFilterWrapper : public wrapper<ShapeFilter> {
    bool mHasSingle = false, mHasPair = false;
    template<typename... Args>
    ShapeFilterWrapper(val &&v, Args &&...args) : wrapper(val(v), std::forward<Args>(args)...) {
        mHasSingle = !v["ShouldCollide"].isUndefined();
        mHasPair   = !v["ShouldCollidePair"].isUndefined();
    }
    bool ShouldCollide(const Shape *inShape2, const SubShapeID &inSub2) const override {
        if (!mHasSingle) return true;
        return const_cast<ShapeFilterWrapper *>(this)->call<bool>("ShouldCollide", handleOf(*inShape2), (uint32)inSub2.GetValue());
    }
    bool ShouldCollide(const Shape *inShape1, const SubShapeID &inSub1, const Shape *inShape2, const SubShapeID &inSub2) const override {
        if (!mHasPair) return true;
        return const_cast<ShapeFilterWrapper *>(this)->call<bool>("ShouldCollidePair", handleOf(*inShape1), (uint32)inSub1.GetValue(), handleOf(*inShape2), (uint32)inSub2.GetValue());
    }
};

// ---- JS-subclassable query collectors --------------------------------------
// AddHit is pure-virtual in Jolt so a JS subclass must define it; Reset/OnBody optional.
struct CastRayCollectorWrapper : public wrapper<CastRayCollector> {
    bool mHasReset = false, mHasOnBody = false, mHasAddHit = false;
    template<typename... Args>
    CastRayCollectorWrapper(val &&v, Args &&...args) : wrapper(val(v), std::forward<Args>(args)...) {
        mHasReset  = !v["Reset"].isUndefined();
        mHasOnBody = !v["OnBody"].isUndefined();
        mHasAddHit = !v["AddHit"].isUndefined();
    }
    void Reset() override {
        CastRayCollector::Reset();
        if (mHasReset) call<void>("Reset");
    }
    void OnBody(const Body &inBody) override {
        if (mHasOnBody) call<void>("OnBody", handleOf(inBody));
    }
    void AddHit(const RayCastResult &inResult) override {
        if (mHasAddHit) call<void>("AddHit", handleOf(inResult));
    }
};
struct CollidePointCollectorWrapper : public wrapper<CollidePointCollector> {
    bool mHasReset = false, mHasOnBody = false, mHasAddHit = false;
    template<typename... Args>
    CollidePointCollectorWrapper(val &&v, Args &&...args) : wrapper(val(v), std::forward<Args>(args)...) {
        mHasReset  = !v["Reset"].isUndefined();
        mHasOnBody = !v["OnBody"].isUndefined();
        mHasAddHit = !v["AddHit"].isUndefined();
    }
    void Reset() override {
        CollidePointCollector::Reset();
        if (mHasReset) call<void>("Reset");
    }
    void OnBody(const Body &inBody) override {
        if (mHasOnBody) call<void>("OnBody", handleOf(inBody));
    }
    void AddHit(const CollidePointResult &inResult) override {
        if (mHasAddHit) call<void>("AddHit", handleOf(inResult));
    }
};
struct CollideShapeCollectorWrapper : public wrapper<CollideShapeCollector> {
    bool mHasReset = false, mHasOnBody = false, mHasAddHit = false;
    template<typename... Args>
    CollideShapeCollectorWrapper(val &&v, Args &&...args) : wrapper(val(v), std::forward<Args>(args)...) {
        mHasReset  = !v["Reset"].isUndefined();
        mHasOnBody = !v["OnBody"].isUndefined();
        mHasAddHit = !v["AddHit"].isUndefined();
    }
    void Reset() override {
        CollideShapeCollector::Reset();
        if (mHasReset) call<void>("Reset");
    }
    void OnBody(const Body &inBody) override {
        if (mHasOnBody) call<void>("OnBody", handleOf(inBody));
    }
    void AddHit(const CollideShapeResult &inResult) override {
        if (mHasAddHit) call<void>("AddHit", handleOf(inResult));
    }
};
struct CastShapeCollectorWrapper : public wrapper<CastShapeCollector> {
    bool mHasReset = false, mHasOnBody = false, mHasAddHit = false;
    template<typename... Args>
    CastShapeCollectorWrapper(val &&v, Args &&...args) : wrapper(val(v), std::forward<Args>(args)...) {
        mHasReset  = !v["Reset"].isUndefined();
        mHasOnBody = !v["OnBody"].isUndefined();
        mHasAddHit = !v["AddHit"].isUndefined();
    }
    void Reset() override {
        CastShapeCollector::Reset();
        if (mHasReset) call<void>("Reset");
    }
    void OnBody(const Body &inBody) override {
        if (mHasOnBody) call<void>("OnBody", handleOf(inBody));
    }
    void AddHit(const ShapeCastResult &inResult) override {
        if (mHasAddHit) call<void>("AddHit", handleOf(inResult));
    }
};
struct TransformedShapeCollectorWrapper : public wrapper<TransformedShapeCollector> {
    bool mHasReset = false, mHasOnBody = false, mHasAddHit = false;
    template<typename... Args>
    TransformedShapeCollectorWrapper(val &&v, Args &&...args) : wrapper(val(v), std::forward<Args>(args)...) {
        mHasReset  = !v["Reset"].isUndefined();
        mHasOnBody = !v["OnBody"].isUndefined();
        mHasAddHit = !v["AddHit"].isUndefined();
    }
    void Reset() override {
        TransformedShapeCollector::Reset();
        if (mHasReset) call<void>("Reset");
    }
    void OnBody(const Body &inBody) override {
        if (mHasOnBody) call<void>("OnBody", handleOf(inBody));
    }
    void AddHit(const TransformedShape &inResult) override {
        if (mHasAddHit) call<void>("AddHit", handleOf(inResult));
    }
};
struct RayCastBodyCollectorWrapper : public wrapper<RayCastBodyCollector> {
    bool mHasReset = false, mHasOnBody = false, mHasAddHit = false;
    template<typename... Args>
    RayCastBodyCollectorWrapper(val &&v, Args &&...args) : wrapper(val(v), std::forward<Args>(args)...) {
        mHasReset  = !v["Reset"].isUndefined();
        mHasOnBody = !v["OnBody"].isUndefined();
        mHasAddHit = !v["AddHit"].isUndefined();
    }
    void Reset() override {
        RayCastBodyCollector::Reset();
        if (mHasReset) call<void>("Reset");
    }
    void OnBody(const Body &inBody) override {
        if (mHasOnBody) call<void>("OnBody", handleOf(inBody));
    }
    void AddHit(const BroadPhaseCastResult &inResult) override {
        if (mHasAddHit) call<void>("AddHit", handleOf(inResult));
    }
};
struct CastShapeBodyCollectorWrapper : public wrapper<CastShapeBodyCollector> {
    bool mHasReset = false, mHasOnBody = false, mHasAddHit = false;
    template<typename... Args>
    CastShapeBodyCollectorWrapper(val &&v, Args &&...args) : wrapper(val(v), std::forward<Args>(args)...) {
        mHasReset  = !v["Reset"].isUndefined();
        mHasOnBody = !v["OnBody"].isUndefined();
        mHasAddHit = !v["AddHit"].isUndefined();
    }
    void Reset() override {
        CastShapeBodyCollector::Reset();
        if (mHasReset) call<void>("Reset");
    }
    void OnBody(const Body &inBody) override {
        if (mHasOnBody) call<void>("OnBody", handleOf(inBody));
    }
    void AddHit(const BroadPhaseCastResult &inResult) override {
        if (mHasAddHit) call<void>("AddHit", handleOf(inResult));
    }
};
struct CollideShapeBodyCollectorWrapper : public wrapper<CollideShapeBodyCollector> {
    bool mHasReset = false, mHasOnBody = false, mHasAddHit = false;
    template<typename... Args>
    CollideShapeBodyCollectorWrapper(val &&v, Args &&...args) : wrapper(val(v), std::forward<Args>(args)...) {
        mHasReset  = !v["Reset"].isUndefined();
        mHasOnBody = !v["OnBody"].isUndefined();
        mHasAddHit = !v["AddHit"].isUndefined();
    }
    void Reset() override {
        CollideShapeBodyCollector::Reset();
        if (mHasReset) call<void>("Reset");
    }
    void OnBody(const Body &inBody) override {
        if (mHasOnBody) call<void>("OnBody", handleOf(inBody));
    }
    void AddHit(const BodyID &inResult) override {
        if (mHasAddHit) call<void>("AddHit", (uint32)inResult.GetIndexAndSequenceNumber());
    }
};
struct SimShapeFilterWrapper : public wrapper<SimShapeFilter> {
    bool mHas = false;
    template<typename... Args>
    SimShapeFilterWrapper(val &&v, Args &&...args) : wrapper(val(v), std::forward<Args>(args)...) {
        mHas = !v["ShouldCollide"].isUndefined();
    }
    bool ShouldCollide(const Body &inBody1, const Shape *inShape1, const SubShapeID &inSub1,
                       const Body &inBody2, const Shape *inShape2, const SubShapeID &inSub2) const override {
        if (!mHas) return true;
        return const_cast<SimShapeFilterWrapper *>(this)->call<bool>("ShouldCollide",
            handleOf(inBody1), handleOf(*inShape1), (uint32)inSub1.GetValue(),
            handleOf(inBody2), handleOf(*inShape2), (uint32)inSub2.GetValue());
    }
};

// JS-subclassable collision group filter (idl GroupFilterJS).
struct GroupFilterWrapper : public wrapper<GroupFilter> {
    EMSCRIPTEN_WRAPPER(GroupFilterWrapper);
    bool CanCollide(const CollisionGroup &inGroup1, const CollisionGroup &inGroup2) const override {
        return const_cast<GroupFilterWrapper *>(this)->call<bool>("CanCollide", inGroup1, inGroup2);
    }
};

#ifdef JPH_DEBUG_RENDERER
// JS-subclassable debug renderer. Receives draw primitives from Jolt's DebugRendererEm.
// RVec3*/Color* args are dereferenced to value types (Vec3 value_array / Color value_array)
// so JS sees them as plain arrays. Geometry buffer pointers are passed as uintptr_t so JS
// can read raw triangle/vertex data via HEAP with DebugRendererVertexTraits offsets.
struct DebugRendererWrapper : public wrapper<DebugRendererEm> {
    EMSCRIPTEN_WRAPPER(DebugRendererWrapper);
    void DrawLine(const RVec3 *inFrom, const RVec3 *inTo, const Color *inColor) override {
        call<void>("DrawLine", Vec3(*inFrom), Vec3(*inTo), *inColor);
    }
    void DrawTriangle(const RVec3 *inV1, const RVec3 *inV2, const RVec3 *inV3, const Color *inColor, ECastShadow inCastShadow) override {
        call<void>("DrawTriangle", Vec3(*inV1), Vec3(*inV2), Vec3(*inV3), *inColor, inCastShadow);
    }
    void DrawText3D(const RVec3 *inPosition, const void *inString, uint32 inStringLen, const Color *inColor, float inHeight) override {
        call<void>("DrawText3D", Vec3(*inPosition), std::string(static_cast<const char*>(inString), inStringLen), *inColor, inHeight);
    }
    uint32 CreateTriangleBatchID(const void *inTriangles, int inTriangleCount) override {
        return call<uint32>("CreateTriangleBatchID", (uintptr_t)inTriangles, inTriangleCount);
    }
    uint32 CreateTriangleBatchIDWithIndex(const void *inVertices, int inVertexCount, const void *inIndices, int inIndexCount) override {
        return call<uint32>("CreateTriangleBatchIDWithIndex", (uintptr_t)inVertices, inVertexCount, (uintptr_t)inIndices, inIndexCount);
    }
    void DrawGeometryWithID(const RMat44 *inModelMatrix, const AABox *inWorldSpaceBounds, float inLODScaleSq, Color *inModelColor, const uint32 inGeometryID, ECullMode inCullMode, ECastShadow inCastShadow, EDrawMode inDrawMode) override {
        // Mat44/AABox/Color are value_array types — pass by value (copies), not via handleOf
        call<void>("DrawGeometryWithID", *inModelMatrix, *inWorldSpaceBounds, inLODScaleSq, *inModelColor, inGeometryID, inCullMode, inCastShadow, inDrawMode);
    }
};
#endif

// Buffering contact listener: appends contacts to flat wasm-heap tiers DURING the
// step (pure C++, no JS crossing), for bulk zero-alloc reads after Step (post.js).
// Added and persisted contacts are stored in separate tiers so JS can loop over
// only the category it cares about without an isNew flag. Vectors keep capacity
// across Clear() — zero alloc after warm-up. Single-threaded only (Jolt fires
// listeners from job threads in the MT build; would need a mutex there).
//   addedI32/persistedI32 stride 6: body1, body2, subShape1, subShape2, ptStart, ptCount
//   addedF32/persistedF32 stride 4: worldNormal(xyz), penetrationDepth
//   pointsF32             stride 6: worldPointOn1(xyz), worldPointOn2(xyz)  [shared]
//   removedI32            stride 4: body1, subShape1, body2, subShape2
class ContactListenerBuffer : public ContactListener {
public:
    std::vector<int32_t> mAddedI32, mPersistedI32, mRemovedI32;
    std::vector<float>   mAddedF32, mPersistedF32, mPointsF32;
    int mAddedCount = 0, mPersistedCount = 0, mRemovedCount = 0;

    void Clear() {
        mAddedI32.clear();     mAddedF32.clear();     mAddedCount = 0;
        mPersistedI32.clear(); mPersistedF32.clear(); mPersistedCount = 0;
        mPointsF32.clear();
        mRemovedI32.clear(); mRemovedCount = 0;
    }
    void OnContactAdded(const Body &b1, const Body &b2, const ContactManifold &m, ContactSettings &) override {
        Pack(mAddedI32, mAddedF32, mAddedCount, b1, b2, m);
    }
    void OnContactPersisted(const Body &b1, const Body &b2, const ContactManifold &m, ContactSettings &) override {
        Pack(mPersistedI32, mPersistedF32, mPersistedCount, b1, b2, m);
    }
    void OnContactRemoved(const SubShapeIDPair &p) override {
        mRemovedI32.insert(mRemovedI32.end(), {
            (int32_t)p.GetBody1ID().GetIndexAndSequenceNumber(), (int32_t)p.GetSubShapeID1().GetValue(),
            (int32_t)p.GetBody2ID().GetIndexAndSequenceNumber(), (int32_t)p.GetSubShapeID2().GetValue() });
        mRemovedCount++;
    }
    int GetAddedCount()     const { return mAddedCount; }
    int GetPersistedCount() const { return mPersistedCount; }
    int GetRemovedCount()   const { return mRemovedCount; }
    uintptr_t AddedI32Ptr()     const { return (uintptr_t)mAddedI32.data(); }
    uintptr_t AddedF32Ptr()     const { return (uintptr_t)mAddedF32.data(); }
    uintptr_t PersistedI32Ptr() const { return (uintptr_t)mPersistedI32.data(); }
    uintptr_t PersistedF32Ptr() const { return (uintptr_t)mPersistedF32.data(); }
    uintptr_t PointsF32Ptr()    const { return (uintptr_t)mPointsF32.data(); }
    uintptr_t RemovedI32Ptr()   const { return (uintptr_t)mRemovedI32.data(); }

private:
    void Pack(std::vector<int32_t> &i32, std::vector<float> &f32, int &count,
              const Body &b1, const Body &b2, const ContactManifold &m) {
        int32_t ptStart = (int32_t)(mPointsF32.size() / 6);
        int32_t ptCount = (int32_t)m.mRelativeContactPointsOn1.size();
        i32.insert(i32.end(), {
            (int32_t)b1.GetID().GetIndexAndSequenceNumber(),
            (int32_t)b2.GetID().GetIndexAndSequenceNumber(),
            (int32_t)m.mSubShapeID1.GetValue(), (int32_t)m.mSubShapeID2.GetValue(),
            ptStart, ptCount });
        Vec3 n = m.mWorldSpaceNormal;
        f32.insert(f32.end(), { n.GetX(), n.GetY(), n.GetZ(), m.mPenetrationDepth });
        for (int32_t i = 0; i < ptCount; i++) {
            RVec3 p1 = m.GetWorldSpaceContactPointOn1(i), p2 = m.GetWorldSpaceContactPointOn2(i);
            mPointsF32.insert(mPointsF32.end(), {
                (float)p1.GetX(), (float)p1.GetY(), (float)p1.GetZ(),
                (float)p2.GetX(), (float)p2.GetY(), (float)p2.GetZ() });
        }
        count++;
    }
};

// Post-step body state snapshot: filled by Refresh() after Step() for all active
// (non-sleeping) rigid bodies — both dynamic and kinematic. Safe to call with the
// lock-free interface because Step() has fully returned before JS calls Refresh().
//   bodiesF32 stride 14: id(u32 bits in float[0]), px,py,pz, rx,ry,rz,rw, lvx,lvy,lvz, avx,avy,avz
class ActiveBodyBuffer {
public:
    std::vector<float> mBodies;
    int mBodyCount = 0;

    void Refresh(PhysicsSystem *sys) {
        BodyIDVector ids;
        sys->GetActiveBodies(EBodyType::RigidBody, ids);
        mBodyCount = (int)ids.size();
        mBodies.resize(mBodyCount * 14);
        const BodyInterface &bi = sys->GetBodyInterfaceNoLock();
        for (int i = 0; i < mBodyCount; i++) {
            const BodyID &id = ids[i];
            float *d = mBodies.data() + i * 14;
            uint32_t rawId = id.GetIndexAndSequenceNumber();
            std::memcpy(d, &rawId, 4);
            RVec3 pos = bi.GetPosition(id);
            Quat  rot = bi.GetRotation(id);
            Vec3  lv  = bi.GetLinearVelocity(id);
            Vec3  av  = bi.GetAngularVelocity(id);
            d[1]=(float)pos.GetX(); d[2]=(float)pos.GetY(); d[3]=(float)pos.GetZ();
            d[4]=rot.GetX(); d[5]=rot.GetY(); d[6]=rot.GetZ(); d[7]=rot.GetW();
            d[8]=lv.GetX();  d[9]=lv.GetY();  d[10]=lv.GetZ();
            d[11]=av.GetX(); d[12]=av.GetY(); d[13]=av.GetZ();
        }
    }
    int GetBodyCount()      const { return mBodyCount; }
    uintptr_t BodiesF32Ptr() const { return (uintptr_t)mBodies.data(); }
};

}  // namespace jsbind

EMSCRIPTEN_BINDINGS(jolt) {
    using namespace jsbind;   // internal binding machinery (jolt_class_, WriteVec3, ...); the
                              // out_function descriptor DSL is referenced qualified as out_desc::
    emscripten::function("_getOutScratch", +[]() -> uintptr_t { return (uintptr_t)sOutScratch; });

    // ---- math value types: mathcat-style plain arrays [x, y, z, ...] ----
    // (RVec3 == Vec3 in single precision). value_array marshals to/from a JS array;
    // matches mathcat's Vec3=[x,y,z], Quat=[x,y,z,w], Vec4/Float2/Float3, Box3.
    value_array<Vec3>("Vec3")
        .element(&Vec3::GetX, &Vec3::SetX)
        .element(&Vec3::GetY, &Vec3::SetY)
        .element(&Vec3::GetZ, &Vec3::SetZ);
    value_array<JPH::Vector<2>>("Vector2")   // [x, y] — plain array, math done JS-side
        .element(+[](const JPH::Vector<2> &v) { return v[0]; }, +[](JPH::Vector<2> &v, float x) { v[0] = x; })
        .element(+[](const JPH::Vector<2> &v) { return v[1]; }, +[](JPH::Vector<2> &v, float y) { v[1] = y; });
    value_array<Quat>("Quat")   // [x, y, z, w] — same order as Jolt
        .element(&Quat::GetX, &Quat::SetX)
        .element(&Quat::GetY, &Quat::SetY)
        .element(&Quat::GetZ, &Quat::SetZ)
        .element(&Quat::GetW, &Quat::SetW);
    value_array<Vec4>("Vec4")
        .element(&Vec4::GetX, &Vec4::SetX)
        .element(&Vec4::GetY, &Vec4::SetY)
        .element(&Vec4::GetZ, &Vec4::SetZ)
        .element(&Vec4::GetW, &Vec4::SetW);
    value_array<Float3>("Float3")
        .element(&Float3::x).element(&Float3::y).element(&Float3::z);
    value_array<Float2>("Float2")
        .element(&Float2::x).element(&Float2::y);
    // AABox -> mathcat Box3 = [minX, minY, minZ, maxX, maxY, maxZ] (flat 6-array)
    value_array<AABox>("AABox")
        .element(+[](const AABox &b) { return b.mMin.GetX(); }, +[](AABox &b, float v) { b.mMin.SetX(v); })
        .element(+[](const AABox &b) { return b.mMin.GetY(); }, +[](AABox &b, float v) { b.mMin.SetY(v); })
        .element(+[](const AABox &b) { return b.mMin.GetZ(); }, +[](AABox &b, float v) { b.mMin.SetZ(v); })
        .element(+[](const AABox &b) { return b.mMax.GetX(); }, +[](AABox &b, float v) { b.mMax.SetX(v); })
        .element(+[](const AABox &b) { return b.mMax.GetY(); }, +[](AABox &b, float v) { b.mMax.SetY(v); })
        .element(+[](const AABox &b) { return b.mMax.GetZ(); }, +[](AABox &b, float v) { b.mMax.SetZ(v); });
    // Mat44 -> mathcat Mat4 = 16 column-major floats
    value_array<Mat44>("Mat44")
        .MAT_ELEM(0).MAT_ELEM(1).MAT_ELEM(2).MAT_ELEM(3)
        .MAT_ELEM(4).MAT_ELEM(5).MAT_ELEM(6).MAT_ELEM(7)
        .MAT_ELEM(8).MAT_ELEM(9).MAT_ELEM(10).MAT_ELEM(11)
        .MAT_ELEM(12).MAT_ELEM(13).MAT_ELEM(14).MAT_ELEM(15);
    value_array<Color>("Color")   // [r, g, b, a] (0-255)
        .element(&Color::r).element(&Color::g).element(&Color::b).element(&Color::a);
    // BodyID -> plain number (marshalled via BindingType<BodyID> specialization above)

    // ---- enums (all 27, faithful C++ names: Jolt.EBodyType.RigidBody etc.) ----
    enum_<EBodyType>("EBodyType")
        .value("RigidBody", EBodyType::RigidBody)
        .value("SoftBody", EBodyType::SoftBody);
    enum_<EMotionType>("EMotionType")
        .value("Static", EMotionType::Static)
        .value("Kinematic", EMotionType::Kinematic)
        .value("Dynamic", EMotionType::Dynamic);
    enum_<EMotionQuality>("EMotionQuality")
        .value("Discrete", EMotionQuality::Discrete)
        .value("LinearCast", EMotionQuality::LinearCast);
    enum_<EActivation>("EActivation")
        .value("Activate", EActivation::Activate)
        .value("DontActivate", EActivation::DontActivate);
    enum_<EShapeType>("EShapeType")
        .value("Convex", EShapeType::Convex)
        .value("Compound", EShapeType::Compound)
        .value("Decorated", EShapeType::Decorated)
        .value("Mesh", EShapeType::Mesh)
        .value("HeightField", EShapeType::HeightField)
        .value("Plane", EShapeType::Plane)
        .value("Empty", EShapeType::Empty);
    enum_<EShapeSubType>("EShapeSubType")
        .value("Sphere", EShapeSubType::Sphere)
        .value("Box", EShapeSubType::Box)
        .value("Capsule", EShapeSubType::Capsule)
        .value("TaperedCapsule", EShapeSubType::TaperedCapsule)
        .value("Cylinder", EShapeSubType::Cylinder)
        .value("TaperedCylinder", EShapeSubType::TaperedCylinder)
        .value("ConvexHull", EShapeSubType::ConvexHull)
        .value("StaticCompound", EShapeSubType::StaticCompound)
        .value("MutableCompound", EShapeSubType::MutableCompound)
        .value("RotatedTranslated", EShapeSubType::RotatedTranslated)
        .value("Scaled", EShapeSubType::Scaled)
        .value("OffsetCenterOfMass", EShapeSubType::OffsetCenterOfMass)
        .value("Mesh", EShapeSubType::Mesh)
        .value("HeightField", EShapeSubType::HeightField)
        .value("Plane", EShapeSubType::Plane)
        .value("Empty", EShapeSubType::Empty);
    enum_<EConstraintSpace>("EConstraintSpace")
        .value("LocalToBodyCOM", EConstraintSpace::LocalToBodyCOM)
        .value("WorldSpace", EConstraintSpace::WorldSpace);
    enum_<ESpringMode>("ESpringMode")
        .value("FrequencyAndDamping", ESpringMode::FrequencyAndDamping)
        .value("StiffnessAndDamping", ESpringMode::StiffnessAndDamping);
    enum_<EOverrideMassProperties>("EOverrideMassProperties")
        .value("CalculateMassAndInertia", EOverrideMassProperties::CalculateMassAndInertia)
        .value("CalculateInertia", EOverrideMassProperties::CalculateInertia)
        .value("MassAndInertiaProvided", EOverrideMassProperties::MassAndInertiaProvided);
    enum_<EAllowedDOFs>("EAllowedDOFs")
        .value("TranslationX", EAllowedDOFs::TranslationX)
        .value("TranslationY", EAllowedDOFs::TranslationY)
        .value("TranslationZ", EAllowedDOFs::TranslationZ)
        .value("RotationX", EAllowedDOFs::RotationX)
        .value("RotationY", EAllowedDOFs::RotationY)
        .value("RotationZ", EAllowedDOFs::RotationZ)
        .value("Plane2D", EAllowedDOFs::Plane2D)
        .value("All", EAllowedDOFs::All);
    enum_<EStateRecorderState>("EStateRecorderState")
        .value("None", EStateRecorderState::None)
        .value("Global", EStateRecorderState::Global)
        .value("Bodies", EStateRecorderState::Bodies)
        .value("Contacts", EStateRecorderState::Contacts)
        .value("Constraints", EStateRecorderState::Constraints)
        .value("All", EStateRecorderState::All);
    enum_<EBackFaceMode>("EBackFaceMode")
        .value("IgnoreBackFaces", EBackFaceMode::IgnoreBackFaces)
        .value("CollideWithBackFaces", EBackFaceMode::CollideWithBackFaces);
    enum_<CharacterBase::EGroundState>("EGroundState")
        .value("OnGround", CharacterBase::EGroundState::OnGround)
        .value("OnSteepGround", CharacterBase::EGroundState::OnSteepGround)
        .value("NotSupported", CharacterBase::EGroundState::NotSupported)
        .value("InAir", CharacterBase::EGroundState::InAir);
    enum_<ValidateResult>("ValidateResult")
        .value("AcceptAllContactsForThisBodyPair", ValidateResult::AcceptAllContactsForThisBodyPair)
        .value("AcceptContact", ValidateResult::AcceptContact)
        .value("RejectContact", ValidateResult::RejectContact)
        .value("RejectAllContactsForThisBodyPair", ValidateResult::RejectAllContactsForThisBodyPair);
    enum_<SoftBodyValidateResult>("SoftBodyValidateResult")
        .value("AcceptContact", SoftBodyValidateResult::AcceptContact)
        .value("RejectContact", SoftBodyValidateResult::RejectContact);
    enum_<EActiveEdgeMode>("EActiveEdgeMode")
        .value("CollideOnlyWithActive", EActiveEdgeMode::CollideOnlyWithActive)
        .value("CollideWithAll", EActiveEdgeMode::CollideWithAll);
    enum_<ECollectFacesMode>("ECollectFacesMode")
        .value("CollectFaces", ECollectFacesMode::CollectFaces)
        .value("NoFaces", ECollectFacesMode::NoFaces);
    enum_<SixDOFConstraintSettings::EAxis>("SixDOFConstraintSettings_EAxis")
        .value("TranslationX", SixDOFConstraintSettings::EAxis::TranslationX)
        .value("TranslationY", SixDOFConstraintSettings::EAxis::TranslationY)
        .value("TranslationZ", SixDOFConstraintSettings::EAxis::TranslationZ)
        .value("RotationX", SixDOFConstraintSettings::EAxis::RotationX)
        .value("RotationY", SixDOFConstraintSettings::EAxis::RotationY)
        .value("RotationZ", SixDOFConstraintSettings::EAxis::RotationZ);
    enum_<EConstraintType>("EConstraintType")
        .value("Constraint", EConstraintType::Constraint)
        .value("TwoBodyConstraint", EConstraintType::TwoBodyConstraint);
    enum_<EConstraintSubType>("EConstraintSubType")
        .value("Fixed", EConstraintSubType::Fixed)
        .value("Point", EConstraintSubType::Point)
        .value("Hinge", EConstraintSubType::Hinge)
        .value("Slider", EConstraintSubType::Slider)
        .value("Distance", EConstraintSubType::Distance)
        .value("Cone", EConstraintSubType::Cone)
        .value("SwingTwist", EConstraintSubType::SwingTwist)
        .value("SixDOF", EConstraintSubType::SixDOF)
        .value("Path", EConstraintSubType::Path)
        .value("Vehicle", EConstraintSubType::Vehicle)
        .value("RackAndPinion", EConstraintSubType::RackAndPinion)
        .value("Gear", EConstraintSubType::Gear)
        .value("Pulley", EConstraintSubType::Pulley);
    enum_<EMotorState>("EMotorState")
        .value("Off", EMotorState::Off)
        .value("Velocity", EMotorState::Velocity)
        .value("Position", EMotorState::Position);
    enum_<ETransmissionMode>("ETransmissionMode")
        .value("Auto", ETransmissionMode::Auto)
        .value("Manual", ETransmissionMode::Manual);
    enum_<ESwingType>("ESwingType")
        .value("Cone", ESwingType::Cone)
        .value("Pyramid", ESwingType::Pyramid);
    enum_<EPathRotationConstraintType>("EPathRotationConstraintType")
        .value("Free", EPathRotationConstraintType::Free)
        .value("ConstrainAroundTangent", EPathRotationConstraintType::ConstrainAroundTangent)
        .value("ConstrainAroundNormal", EPathRotationConstraintType::ConstrainAroundNormal)
        .value("ConstrainAroundBinormal", EPathRotationConstraintType::ConstrainAroundBinormal)
        .value("ConstrainToPath", EPathRotationConstraintType::ConstrainToPath)
        .value("FullyConstrained", EPathRotationConstraintType::FullyConstrained);
    enum_<SoftBodySharedSettings::EBendType>("EBendType")
        .value("None", SoftBodySharedSettings::EBendType::None)
        .value("Distance", SoftBodySharedSettings::EBendType::Distance)
        .value("Dihedral", SoftBodySharedSettings::EBendType::Dihedral);
    enum_<SoftBodySharedSettings::ELRAType>("ELRAType")   // long range attachment
        .value("None", SoftBodySharedSettings::ELRAType::None)
        .value("EuclideanDistance", SoftBodySharedSettings::ELRAType::EuclideanDistance)
        .value("GeodesicDistance", SoftBodySharedSettings::ELRAType::GeodesicDistance);
    enum_<MeshShapeSettings::EBuildQuality>("MeshShapeSettings_EBuildQuality")
        .value("FavorRuntimePerformance", MeshShapeSettings::EBuildQuality::FavorRuntimePerformance)
        .value("FavorBuildSpeed", MeshShapeSettings::EBuildQuality::FavorBuildSpeed);

    // ---- shape hierarchy (refcounted via Ref<T>) ----
    jolt_class_<Shape>("Shape")
        .smart_ptr<Ref<Shape>>("ShapeRef")
        // read-only refcount introspection (never bind AddRef/Release — the Ref<T> owns the count)
        .function("GetRefCount", +[](const Shape &s) { return s.GetRefCount(); })
        // Introspection. Shape is polymorphic, so embind auto-downcasts GetShape()/GetInnerShape()
        // returns to the concrete subclass (BoxShape/ScaledShape/...) — read its accessors directly,
        // no cast; GetType()/GetSubType() drive the dispatch. (No castObject: WebIDL needed it
        // because its binder never downcast; embind does, so it would be a no-op.)
        .function("GetType", &Shape::GetType)
        .function("GetSubType", &Shape::GetSubType)
        .out_function("GetCenterOfMass(out)", out_desc::Vec3, +[](const Shape &s, uintptr_t out) { WriteVec3(s.GetCenterOfMass(), out); })
        // Mass + scaling. GetMassProperties returns a copy (read .mMass / .mInertia); IsValidScale/
        // MakeScaleValid/ScaleShape take a Vec3 [x,y,z]; ScaleShape returns a ShapeResult (.Get()).
        .function("GetMassProperties", +[](const Shape &s) { return s.GetMassProperties(); })
        .function("GetVolume", &Shape::GetVolume)
        .function("IsValidScale(scale)", +[](const Shape &s, Vec3 scale) { return s.IsValidScale(scale); })
        .function("MakeScaleValid(scale)", +[](const Shape &s, Vec3 scale) -> Vec3 { return s.MakeScaleValid(scale); })
        .function("ScaleShape(scale)", +[](const Shape &s, Vec3 scale) { return s.ScaleShape(scale); })
        .function("MustBeStatic", &Shape::MustBeStatic)
        .out_function("GetLocalBounds(out)", out_desc::AABox,
            +[](const Shape &s, uintptr_t out) { WriteAABox(s.GetLocalBounds(), out); })
        .out_function("GetWorldSpaceBounds(out, comTransform, scale)", out_desc::AABox, out_desc::Pass, out_desc::Pass,
            +[](const Shape &s, uintptr_t out, Mat44 comTransform, Vec3 scale) {
                WriteAABox(s.GetWorldSpaceBounds(comTransform, scale), out); })
        .function("GetUserData", +[](const Shape &s) { return (uint32)s.GetUserData(); })
        .function("SetUserData(userData)", +[](Shape &s, uint32 userData) { s.SetUserData(userData); })
        .function("GetSubShapeIDBitsRecursive", &Shape::GetSubShapeIDBitsRecursive)
        .function("GetInnerRadius", &Shape::GetInnerRadius)
        .function("GetLeafShape(subShapeID)",
            +[](const Shape &s, uint32 subShapeID) {
                SubShapeID remainder;
                return const_cast<Shape *>(s.GetLeafShape(toSubShapeID(subShapeID), remainder)); }, allow_raw_pointers())
        .function("GetMaterial(subShapeID)",
            +[](const Shape &s, uint32 subShapeID) {
                return const_cast<PhysicsMaterial *>(s.GetMaterial(toSubShapeID(subShapeID))); }, allow_raw_pointers())
        .out_function("GetSurfaceNormal(out, subShapeID, localSurfacePosition)", out_desc::Vec3, out_desc::Pass, out_desc::Pass,
            +[](const Shape &s, uintptr_t out, uint32 subShapeID, Vec3 localSurfacePosition) {
                WriteVec3(s.GetSurfaceNormal(toSubShapeID(subShapeID), localSurfacePosition), out); })
        .function("GetSubShapeUserData(subShapeID)",
            +[](const Shape &s, uint32 subShapeID) { return (uint32)s.GetSubShapeUserData(toSubShapeID(subShapeID)); })
        .function("GetStats: { sizeBytes: number; numTriangles: number }", +[](const Shape &s) -> val {
                Shape::Stats st = s.GetStats();
                val o = val::object();
                o.set("sizeBytes", (double)st.mSizeBytes);
                o.set("numTriangles", (uint32)st.mNumTriangles);
                return o; })
        .function("GetSubShapeTransformedShape(subShapeID, positionCOM, rotation, scale)",
            +[](const Shape &s, uint32 subShapeID, Vec3 posCOM, Quat rot, Vec3 scale) {
                SubShapeID rem;
                return s.GetSubShapeTransformedShape(toSubShapeID(subShapeID), posCOM, rot, scale, rem); })
        // All triangles as a flat Float32Array — 9 floats/triangle (x,y,z * 3), COM-relative
        // local space. GetTrianglesStart asserts on non-leaf shapes, so gather leaves via
        // CollectTransformedShapes first, then extract per leaf.
        .function("GetTriangles: Float32Array", +[](const Shape &s) -> val {
            std::vector<float> verts;
            constexpr int kMax = Shape::cGetTrianglesMinTrianglesRequested;
            Float3 buf[kMax * 3];
            AllHitCollisionCollector<TransformedShapeCollector> collector;
            s.CollectTransformedShapes(AABox::sBiggest(), Vec3::sZero(), Quat::sIdentity(),
                Vec3::sReplicate(1.0f), SubShapeIDCreator(), collector, ShapeFilter());
            for (const TransformedShape &ts : collector.mHits) {
                Shape::GetTrianglesContext ctx;
                ts.GetTrianglesStart(ctx, AABox::sBiggest(), RVec3::sZero());
                for (;;) {
                    int n = ts.GetTrianglesNext(ctx, kMax, buf);
                    if (n == 0) break;
                    for (int i = 0; i < n * 3; i++) { verts.push_back(buf[i].x); verts.push_back(buf[i].y); verts.push_back(buf[i].z); }
                }
            }
            val out = val::global("Float32Array").new_(verts.size());
            out.call<void>("set", val(typed_memory_view(verts.size(), verts.data())));
            return out;
        });
    jolt_class_<ConvexShape, base<Shape>>("ConvexShape")
        .smart_ptr<Ref<ConvexShape>>("ConvexShapeRef")
        .function("GetDensity", &ConvexShape::GetDensity)
        .function("SetDensity(density)", &ConvexShape::SetDensity)
        .function("SetMaterial(material)",
            +[](ConvexShape &s, const PhysicsMaterial *m) { s.SetMaterial(m); }, allow_raw_pointers());
        // No no-arg GetMaterial here: a derived overload whose arity differs from the inherited
        // Shape::GetMaterial(subShapeID) breaks embind's overload table. Use the inherited one.
    jolt_class_<BoxShape, base<ConvexShape>>("BoxShape")
        .smart_ptr<Ref<BoxShape>>("BoxShapeRef")
        .constructor("halfExtent", +[](Vec3 halfExtent) -> Ref<BoxShape> { return new BoxShape(halfExtent); })
        .constructor("halfExtent, convexRadius", +[](Vec3 halfExtent, float convexRadius) -> Ref<BoxShape> { return new BoxShape(halfExtent, convexRadius); })
        .out_function("GetHalfExtent(out)", out_desc::Vec3, +[](const BoxShape &s, uintptr_t out) { WriteVec3(s.GetHalfExtent(), out); });
    jolt_class_<SphereShape, base<ConvexShape>>("SphereShape")
        .smart_ptr<Ref<SphereShape>>("SphereShapeRef")
        .constructor("radius", +[](float radius) -> Ref<SphereShape> { return new SphereShape(radius); })
        .function("GetRadius", &SphereShape::GetRadius);

    // ---- ShapeSettings -> Create() -> ShapeResult -> Get() (the idiomatic path) ----
    // Mirrors Jolt C++/docs: `BoxShapeSettings(he).Create().Get()`. ShapeResult is
    // Result<Ref<Shape>>; Get() returns the Ref<Shape> (RTTI-downcast to the concrete
    // shape in JS). Errors are surfaced via HasError()/GetError() exactly as in C++.
    using ShapeResult = ShapeSettings::ShapeResult;
    jolt_class_<ShapeResult>("ShapeResult")
        .function("IsValid", &ShapeResult::IsValid)
        .function("HasError", &ShapeResult::HasError)
        .function("GetError", +[](const ShapeResult &r) { return r.GetError(); })   // -> JS string
        .function("Get", +[](const ShapeResult &r) { return r.Get(); });            // -> Ref<Shape>

    jolt_class_<ShapeSettings>("ShapeSettings")
        .smart_ptr<Ref<ShapeSettings>>("ShapeSettingsRef")
        .function("Create", &ShapeSettings::Create);
    jolt_class_<ConvexShapeSettings, base<ShapeSettings>>("ConvexShapeSettings")
        .smart_ptr<Ref<ConvexShapeSettings>>("ConvexShapeSettingsRef")
        .property("mDensity", &ConvexShapeSettings::mDensity)
        .function("SetMaterial(material)",
            +[](ConvexShapeSettings &s, const PhysicsMaterial *m) { s.mMaterial = m; }, allow_raw_pointers());
    jolt_class_<BoxShapeSettings, base<ConvexShapeSettings>>("BoxShapeSettings")
        .smart_ptr<Ref<BoxShapeSettings>>("BoxShapeSettingsRef")
        .constructor("halfExtent", +[](Vec3 halfExtent) -> Ref<BoxShapeSettings> { return new BoxShapeSettings(halfExtent); })
        .constructor("halfExtent, convexRadius", +[](Vec3 halfExtent, float convexRadius) -> Ref<BoxShapeSettings> { return new BoxShapeSettings(halfExtent, convexRadius); })
        .property("mHalfExtent", &BoxShapeSettings::mHalfExtent)
        .property("mConvexRadius", &BoxShapeSettings::mConvexRadius);
    jolt_class_<SphereShapeSettings, base<ConvexShapeSettings>>("SphereShapeSettings")
        .smart_ptr<Ref<SphereShapeSettings>>("SphereShapeSettingsRef")
        .constructor("radius", +[](float radius) -> Ref<SphereShapeSettings> { return new SphereShapeSettings(radius); })
        .property("mRadius", &SphereShapeSettings::mRadius);

    // -- Capsule --
    jolt_class_<CapsuleShape, base<ConvexShape>>("CapsuleShape")
        .smart_ptr<Ref<CapsuleShape>>("CapsuleShapeRef")
        .constructor("halfHeightOfCylinder, radius", +[](float halfHeight, float radius) -> Ref<CapsuleShape> { return new CapsuleShape(halfHeight, radius); })
        .function("GetRadius", &CapsuleShape::GetRadius)
        .function("GetHalfHeightOfCylinder", &CapsuleShape::GetHalfHeightOfCylinder);
    jolt_class_<CapsuleShapeSettings, base<ConvexShapeSettings>>("CapsuleShapeSettings")
        .smart_ptr<Ref<CapsuleShapeSettings>>("CapsuleShapeSettingsRef")
        .constructor("halfHeightOfCylinder, radius", +[](float halfHeight, float radius) -> Ref<CapsuleShapeSettings> { return new CapsuleShapeSettings(halfHeight, radius); })
        .property("mRadius", &CapsuleShapeSettings::mRadius)
        .property("mHalfHeightOfCylinder", &CapsuleShapeSettings::mHalfHeightOfCylinder);

    // -- TaperedCapsule (settings-only, no direct value ctor) --
    jolt_class_<TaperedCapsuleShape, base<ConvexShape>>("TaperedCapsuleShape")
        .smart_ptr<Ref<TaperedCapsuleShape>>("TaperedCapsuleShapeRef")
        .function("GetTopRadius", &TaperedCapsuleShape::GetTopRadius)
        .function("GetBottomRadius", &TaperedCapsuleShape::GetBottomRadius)
        .function("GetHalfHeight", &TaperedCapsuleShape::GetHalfHeight);
    jolt_class_<TaperedCapsuleShapeSettings, base<ConvexShapeSettings>>("TaperedCapsuleShapeSettings")
        .smart_ptr<Ref<TaperedCapsuleShapeSettings>>("TaperedCapsuleShapeSettingsRef")
        .constructor("halfHeightOfTaperedCylinder, topRadius, bottomRadius", +[](float halfHeight, float topRadius, float bottomRadius) -> Ref<TaperedCapsuleShapeSettings> { return new TaperedCapsuleShapeSettings(halfHeight, topRadius, bottomRadius); })
        .property("mHalfHeightOfTaperedCylinder", &TaperedCapsuleShapeSettings::mHalfHeightOfTaperedCylinder)
        .property("mTopRadius", &TaperedCapsuleShapeSettings::mTopRadius)
        .property("mBottomRadius", &TaperedCapsuleShapeSettings::mBottomRadius);

    // -- Cylinder --
    jolt_class_<CylinderShape, base<ConvexShape>>("CylinderShape")
        .smart_ptr<Ref<CylinderShape>>("CylinderShapeRef")
        .constructor("halfHeight, radius", +[](float halfHeight, float radius) -> Ref<CylinderShape> { return new CylinderShape(halfHeight, radius); })
        .function("GetHalfHeight", &CylinderShape::GetHalfHeight)
        .function("GetRadius", &CylinderShape::GetRadius);
    jolt_class_<CylinderShapeSettings, base<ConvexShapeSettings>>("CylinderShapeSettings")
        .smart_ptr<Ref<CylinderShapeSettings>>("CylinderShapeSettingsRef")
        .constructor("halfHeight, radius", +[](float halfHeight, float radius) -> Ref<CylinderShapeSettings> { return new CylinderShapeSettings(halfHeight, radius); })
        .property("mHalfHeight", &CylinderShapeSettings::mHalfHeight)
        .property("mRadius", &CylinderShapeSettings::mRadius)
        .property("mConvexRadius", &CylinderShapeSettings::mConvexRadius);

    // -- TaperedCylinder (settings-only) --
    jolt_class_<TaperedCylinderShape, base<ConvexShape>>("TaperedCylinderShape")
        .smart_ptr<Ref<TaperedCylinderShape>>("TaperedCylinderShapeRef")
        .function("GetTopRadius", &TaperedCylinderShape::GetTopRadius)
        .function("GetBottomRadius", &TaperedCylinderShape::GetBottomRadius)
        .function("GetConvexRadius", &TaperedCylinderShape::GetConvexRadius)
        .function("GetHalfHeight", &TaperedCylinderShape::GetHalfHeight);
    jolt_class_<TaperedCylinderShapeSettings, base<ConvexShapeSettings>>("TaperedCylinderShapeSettings")
        .smart_ptr<Ref<TaperedCylinderShapeSettings>>("TaperedCylinderShapeSettingsRef")
        .constructor("halfHeight, topRadius, bottomRadius", +[](float halfHeight, float topRadius, float bottomRadius) -> Ref<TaperedCylinderShapeSettings> { return new TaperedCylinderShapeSettings(halfHeight, topRadius, bottomRadius); })
        .property("mHalfHeight", &TaperedCylinderShapeSettings::mHalfHeight)
        .property("mTopRadius", &TaperedCylinderShapeSettings::mTopRadius)
        .property("mBottomRadius", &TaperedCylinderShapeSettings::mBottomRadius)
        .property("mConvexRadius", &TaperedCylinderShapeSettings::mConvexRadius);

    // -- ConvexHull (settings-only, built from Array<Vec3>) --
    jolt_class_<ConvexHullShape, base<ConvexShape>>("ConvexHullShape")
        .smart_ptr<Ref<ConvexHullShape>>("ConvexHullShapeRef")
        .function("GetNumPoints", &ConvexHullShape::GetNumPoints)
        .out_function("GetPoint(out, index)", out_desc::Vec3, out_desc::Pass, +[](const ConvexHullShape &s, uintptr_t out, uint idx) {
            WriteVec3(s.GetPoint(idx), out); });
    jolt_class_<ConvexHullShapeSettings, base<ConvexShapeSettings>>("ConvexHullShapeSettings")
        .smart_ptr<Ref<ConvexHullShapeSettings>>("ConvexHullShapeSettingsRef")
        .constructor("points", +[](const Array<Vec3> &inPoints) -> Ref<ConvexHullShapeSettings> {
            return new ConvexHullShapeSettings(inPoints); })
        .property("mMaxConvexRadius", &ConvexHullShapeSettings::mMaxConvexRadius)
        .property("mHullTolerance", &ConvexHullShapeSettings::mHullTolerance);

    // -- Compound base --
    jolt_class_<CompoundShape::SubShape>("CompoundShapeSubShape")
        .out_function("GetPositionCOM(out)", out_desc::Vec3,
            +[](const CompoundShape::SubShape &s, uintptr_t out) { WriteVec3(s.GetPositionCOM(), out); })
        .out_function("GetRotation(out)", out_desc::Quat,
            +[](const CompoundShape::SubShape &s, uintptr_t out) { WriteQuat(s.GetRotation(), out); })
        .function("GetShape", +[](const CompoundShape::SubShape &s) { return const_cast<Shape *>(s.mShape.GetPtr()); }, allow_raw_pointers())
        .property("mUserData", &CompoundShape::SubShape::mUserData);
    jolt_class_<CompoundShape, base<Shape>>("CompoundShape")
        .smart_ptr<Ref<CompoundShape>>("CompoundShapeRef")
        .function("GetNumSubShapes", &CompoundShape::GetNumSubShapes)
        .out_function("GetCenterOfMass(out)", out_desc::Vec3, +[](const CompoundShape &s, uintptr_t out) { WriteVec3(s.GetCenterOfMass(), out); })
        .function("GetSubShape(index)",
            +[](const CompoundShape &s, uint index) { return const_cast<CompoundShape::SubShape *>(&s.GetSubShape(index)); }, allow_raw_pointers())
        .function("GetCompoundUserData(index)", &CompoundShape::GetCompoundUserData)
        .function("SetCompoundUserData(index, userData)", &CompoundShape::SetCompoundUserData);
    jolt_class_<CompoundShapeSettings, base<ShapeSettings>>("CompoundShapeSettings")   // abstract
        .smart_ptr<Ref<CompoundShapeSettings>>("CompoundShapeSettingsRef")
        .function("AddShape(position, rotation, shape)",
            +[](CompoundShapeSettings &s, Vec3 pos, Quat rot, const Shape *shape) { s.AddShape(pos, rot, shape); },
            allow_raw_pointers())
        .function("AddShapeWithUserData(position, rotation, shape, userData)",
            +[](CompoundShapeSettings &s, Vec3 pos, Quat rot, const Shape *shape, uint32 userData) {
                s.AddShape(pos, rot, shape, userData); }, allow_raw_pointers());
    jolt_class_<StaticCompoundShape, base<CompoundShape>>("StaticCompoundShape")
        .smart_ptr<Ref<StaticCompoundShape>>("StaticCompoundShapeRef");
    jolt_class_<StaticCompoundShapeSettings, base<CompoundShapeSettings>>("StaticCompoundShapeSettings")
        .smart_ptr<Ref<StaticCompoundShapeSettings>>("StaticCompoundShapeSettingsRef")
        .constructor(+[]() -> Ref<StaticCompoundShapeSettings> { return new StaticCompoundShapeSettings(); });
    jolt_class_<MutableCompoundShape, base<CompoundShape>>("MutableCompoundShape")
        .smart_ptr<Ref<MutableCompoundShape>>("MutableCompoundShapeRef")
        .function("AddShape(position, rotation, shape)",
            +[](MutableCompoundShape &s, Vec3 pos, Quat rot, const Shape *shape) { return s.AddShape(pos, rot, shape); },
            allow_raw_pointers())
        .function("AddShapeWithUserData(position, rotation, shape, userData)",
            +[](MutableCompoundShape &s, Vec3 pos, Quat rot, const Shape *shape, uint32 userData) {
                return s.AddShape(pos, rot, shape, userData); }, allow_raw_pointers())
        .function("RemoveShape(index)", &MutableCompoundShape::RemoveShape)
        .function("AdjustCenterOfMass", &MutableCompoundShape::AdjustCenterOfMass)
        .function("ModifyShape(index, position, rotation)",
            +[](MutableCompoundShape &s, uint index, Vec3 pos, Quat rot) { s.ModifyShape(index, pos, rot); })
        .function("ModifyShapeShape(index, position, rotation, shape)",
            +[](MutableCompoundShape &s, uint index, Vec3 pos, Quat rot, const Shape *shape) {
                s.ModifyShape(index, pos, rot, shape); }, allow_raw_pointers())
        .function("ModifyShapes(startIndex, numShapes, positions, rotations)",
            +[](MutableCompoundShape &s, uint startIndex, uint numShapes, val positions, val rotations) {
                std::vector<float> pf = convertJSArrayToNumberVector<float>(positions);
                std::vector<float> rf = convertJSArrayToNumberVector<float>(rotations);
                std::vector<Vec3> pos; pos.reserve(numShapes);
                std::vector<Quat> rot; rot.reserve(numShapes);
                for (uint i = 0; i < numShapes; i++) pos.emplace_back(pf[i*3+0], pf[i*3+1], pf[i*3+2]);
                for (uint i = 0; i < numShapes; i++) rot.push_back(Quat(rf[i*4+0], rf[i*4+1], rf[i*4+2], rf[i*4+3]));
                s.ModifyShapes(startIndex, numShapes, pos.data(), rot.data()); });
    jolt_class_<MutableCompoundShapeSettings, base<CompoundShapeSettings>>("MutableCompoundShapeSettings")
        .smart_ptr<Ref<MutableCompoundShapeSettings>>("MutableCompoundShapeSettingsRef")
        .constructor(+[]() -> Ref<MutableCompoundShapeSettings> { return new MutableCompoundShapeSettings(); });

    // -- Decorated base --
    jolt_class_<DecoratedShape, base<Shape>>("DecoratedShape")   // abstract
        .smart_ptr<Ref<DecoratedShape>>("DecoratedShapeRef")
        .function("GetInnerShape", +[](const DecoratedShape &s) { return const_cast<Shape *>(s.GetInnerShape()); }, allow_raw_pointers());
    jolt_class_<DecoratedShapeSettings, base<ShapeSettings>>("DecoratedShapeSettings")   // abstract
        .smart_ptr<Ref<DecoratedShapeSettings>>("DecoratedShapeSettingsRef");

    // -- RotatedTranslated --
    jolt_class_<RotatedTranslatedShape, base<DecoratedShape>>("RotatedTranslatedShape")
        .smart_ptr<Ref<RotatedTranslatedShape>>("RotatedTranslatedShapeRef")
        .constructor("position, rotation, shape", +[](Vec3 pos, Quat rot, const Shape *shape) -> Ref<RotatedTranslatedShape> {
            return new RotatedTranslatedShape(pos, rot, shape); }, allow_raw_pointers())
        .out_function("GetPosition(out)", out_desc::Vec3, +[](const RotatedTranslatedShape &s, uintptr_t out) { WriteVec3(s.GetPosition(), out); })
        .out_function("GetRotation(out)", out_desc::Quat, +[](const RotatedTranslatedShape &s, uintptr_t out) { WriteQuat(s.GetRotation(), out); });
    jolt_class_<RotatedTranslatedShapeSettings, base<DecoratedShapeSettings>>("RotatedTranslatedShapeSettings")
        .smart_ptr<Ref<RotatedTranslatedShapeSettings>>("RotatedTranslatedShapeSettingsRef")
        .constructor("position, rotation, shape", +[](Vec3 pos, Quat rot, const Shape *shape) -> Ref<RotatedTranslatedShapeSettings> {
            return new RotatedTranslatedShapeSettings(pos, rot, shape); }, allow_raw_pointers())
        .property("mPosition", &RotatedTranslatedShapeSettings::mPosition)
        .property("mRotation", &RotatedTranslatedShapeSettings::mRotation);

    // -- Scaled --
    jolt_class_<ScaledShape, base<DecoratedShape>>("ScaledShape")
        .smart_ptr<Ref<ScaledShape>>("ScaledShapeRef")
        .constructor("shape, scale", +[](const Shape *shape, Vec3 scale) -> Ref<ScaledShape> {
            return new ScaledShape(shape, scale); }, allow_raw_pointers())
        .out_function("GetScale(out)", out_desc::Vec3, +[](const ScaledShape &s, uintptr_t out) { WriteVec3(s.GetScale(), out); });
    jolt_class_<ScaledShapeSettings, base<DecoratedShapeSettings>>("ScaledShapeSettings")
        .smart_ptr<Ref<ScaledShapeSettings>>("ScaledShapeSettingsRef")
        .constructor("shape, scale", +[](const Shape *shape, Vec3 scale) -> Ref<ScaledShapeSettings> {
            return new ScaledShapeSettings(shape, scale); }, allow_raw_pointers())
        .property("mScale", &ScaledShapeSettings::mScale);

    // -- OffsetCenterOfMass (note: shape-ctor arg order is reversed vs settings) --
    jolt_class_<OffsetCenterOfMassShape, base<DecoratedShape>>("OffsetCenterOfMassShape")
        .smart_ptr<Ref<OffsetCenterOfMassShape>>("OffsetCenterOfMassShapeRef")
        .constructor("shape, offset", +[](const Shape *shape, Vec3 offset) -> Ref<OffsetCenterOfMassShape> {
            return new OffsetCenterOfMassShape(shape, offset); }, allow_raw_pointers())
        .out_function("GetOffset(out)", out_desc::Vec3, +[](const OffsetCenterOfMassShape &s, uintptr_t out) { WriteVec3(s.GetOffset(), out); });
    jolt_class_<OffsetCenterOfMassShapeSettings, base<DecoratedShapeSettings>>("OffsetCenterOfMassShapeSettings")
        .smart_ptr<Ref<OffsetCenterOfMassShapeSettings>>("OffsetCenterOfMassShapeSettingsRef")
        .constructor("offset, shape", +[](Vec3 offset, const Shape *shape) -> Ref<OffsetCenterOfMassShapeSettings> {
            return new OffsetCenterOfMassShapeSettings(offset, shape); }, allow_raw_pointers())
        .property("mOffset", &OffsetCenterOfMassShapeSettings::mOffset);

    // -- Empty --
    jolt_class_<EmptyShape, base<Shape>>("EmptyShape")
        .smart_ptr<Ref<EmptyShape>>("EmptyShapeRef")
        .constructor(+[]() -> Ref<EmptyShape> { return new EmptyShape(); })
        .constructor("centerOfMass", +[](Vec3 centerOfMass) -> Ref<EmptyShape> { return new EmptyShape(centerOfMass); });
    jolt_class_<EmptyShapeSettings, base<ShapeSettings>>("EmptyShapeSettings")
        .smart_ptr<Ref<EmptyShapeSettings>>("EmptyShapeSettingsRef")
        .constructor(+[]() -> Ref<EmptyShapeSettings> { return new EmptyShapeSettings(); })
        .constructor("centerOfMass", +[](Vec3 centerOfMass) -> Ref<EmptyShapeSettings> { return new EmptyShapeSettings(centerOfMass); })
        .property("mCenterOfMass", &EmptyShapeSettings::mCenterOfMass);

    // -- Plane (helper type for PlaneShape) --
    jolt_class_<Plane>("Plane")
        .constructor<>()
        .class_function("sFromPointAndNormal(point, normal)",
            +[](Vec3 point, Vec3 normal) { return Plane::sFromPointAndNormal(point, normal); })
        .out_function("GetNormal(out)", out_desc::Vec3, +[](const Plane &s, uintptr_t out) { WriteVec3(s.GetNormal(), out); })
        .function("GetConstant", &Plane::GetConstant)
        .constructor("normal, constant", +[](Vec3 n, float c) { return Plane(n, c); })
        .function("SetNormal(normal)", +[](Plane &p, Vec3 n) { p.SetNormal(n); })
        .function("SetConstant(constant)", &Plane::SetConstant)
        .function("SignedDistance(point)", +[](const Plane &p, Vec3 pt) { return p.SignedDistance(pt); })
        .out_function("ProjectPointOnPlane(out, point)", out_desc::Vec3, out_desc::Pass,
            +[](const Plane &p, uintptr_t out, Vec3 pt) { WriteVec3(p.ProjectPointOnPlane(pt), out); })
        .function("Offset(distance)", +[](const Plane &p, float d) { return Plane(p.Offset(d)); })
        .function("Scaled(scale)", +[](const Plane &p, Vec3 s) { return Plane(p.Scaled(s)); })
        .function("GetTransformed(transform)", +[](const Plane &p, Mat44 m) { return Plane(p.GetTransformed(m)); })
        .class_function("sFromPointsCCW(v1, v2, v3)",
            +[](Vec3 a, Vec3 b, Vec3 c) { return Plane(Plane::sFromPointsCCW(a, b, c)); });

    // -- Mesh (bulk input via flat typed arrays: vertices [x,y,z,...], indices [i0,i1,i2,...]) --
    jolt_class_<MeshShape, base<Shape>>("MeshShape")
        .smart_ptr<Ref<MeshShape>>("MeshShapeRef");
    jolt_class_<MeshShapeSettings, base<ShapeSettings>>("MeshShapeSettings")
        .smart_ptr<Ref<MeshShapeSettings>>("MeshShapeSettingsRef")
        .constructor("vertices: Float32Array | number[], indices: Uint32Array | number[]", +[](val inVertices, val inIndices) -> Ref<MeshShapeSettings> {
            std::vector<float> v = convertJSArrayToNumberVector<float>(inVertices);
            std::vector<uint32> idx = convertJSArrayToNumberVector<uint32>(inIndices);
            VertexList verts; verts.reserve(v.size() / 3);
            for (size_t k = 0; k + 2 < v.size(); k += 3) verts.push_back(Float3(v[k], v[k + 1], v[k + 2]));
            IndexedTriangleList tris; tris.reserve(idx.size() / 3);
            for (size_t k = 0; k + 2 < idx.size(); k += 3) tris.push_back(IndexedTriangle(idx[k], idx[k + 1], idx[k + 2], 0));
            return new MeshShapeSettings(verts, tris);
        })
        .property("mMaxTrianglesPerLeaf", &MeshShapeSettings::mMaxTrianglesPerLeaf);

    // -- HeightField (bulk input: flat samples array of size sampleCount*sampleCount) --
    jolt_class_<HeightFieldShape, base<Shape>>("HeightFieldShape")
        .smart_ptr<Ref<HeightFieldShape>>("HeightFieldShapeRef")
        .function("GetSampleCount", &HeightFieldShape::GetSampleCount)
        .out_function("GetPosition(out, x, y)",
            out_desc::Vec3, out_desc::Pass, out_desc::Pass, +[](const HeightFieldShape &s, uintptr_t out, uint x, uint y) {
                WriteVec3(s.GetPosition(x, y), out); })
        .function("IsNoCollision(x, y)", &HeightFieldShape::IsNoCollision)
        // Read back a sizeX*sizeY block of raw height samples (x-major) as a Float32Array.
        // Values can be Jolt.cNoCollisionValue (holes). Stride == sizeX (tightly packed).
        .function("GetHeights(x, y, sizeX, sizeY): Float32Array",
            +[](const HeightFieldShape &s, uint x, uint y, uint sizeX, uint sizeY) -> val {
                std::vector<float> h(sizeX * sizeY);
                s.GetHeights(x, y, sizeX, sizeY, h.data(), sizeX);
                val out = val::global("Float32Array").new_(h.size());
                out.call<void>("set", val(typed_memory_view(h.size(), h.data())));
                return out;
            })
        // Write a sizeX*sizeY block of height samples back into the field (needs the temp
        // allocator from the JoltInterface). Follow with BodyInterface.NotifyShapeChanged.
        .function("SetHeights(x, y, sizeX, sizeY, heights, jolt)",
            +[](HeightFieldShape &s, uint x, uint y, uint sizeX, uint sizeY, val heights, JoltInterface &jolt) {
                std::vector<float> h = convertJSArrayToNumberVector<float>(heights);
                s.SetHeights(x, y, sizeX, sizeY, h.data(), sizeX, *jolt.GetTempAllocator());
            }, allow_raw_pointers());
    jolt_class_<HeightFieldShapeSettings, base<ShapeSettings>>("HeightFieldShapeSettings")
        .smart_ptr<Ref<HeightFieldShapeSettings>>("HeightFieldShapeSettingsRef")
        .constructor("samples: Float32Array | number[], offset, scale, sampleCount", +[](val inSamples, Vec3 inOffset, Vec3 inScale, uint32 inSampleCount) -> Ref<HeightFieldShapeSettings> {
            std::vector<float> s = convertJSArrayToNumberVector<float>(inSamples);
            return new HeightFieldShapeSettings(s.data(), inOffset, inScale, inSampleCount);
        })
        .property("mOffset", &HeightFieldShapeSettings::mOffset)
        .property("mScale", &HeightFieldShapeSettings::mScale)
        .property("mBlockSize", &HeightFieldShapeSettings::mBlockSize)
        .property("mBitsPerSample", &HeightFieldShapeSettings::mBitsPerSample);
    // Magic sample value marking a hole (no collision) in a height field.
    constant("cNoCollisionValue", (float)HeightFieldShapeConstants::cNoCollisionValue);

    // -- Plane shape --
    jolt_class_<PlaneShape, base<Shape>>("PlaneShape")
        .smart_ptr<Ref<PlaneShape>>("PlaneShapeRef")
        .constructor("plane", +[](const Plane &inPlane) -> Ref<PlaneShape> { return new PlaneShape(inPlane); })
        .function("GetPlane", +[](const PlaneShape &s) { return s.GetPlane(); })
        .function("GetHalfExtent", &PlaneShape::GetHalfExtent);
    jolt_class_<PlaneShapeSettings, base<ShapeSettings>>("PlaneShapeSettings")
        .smart_ptr<Ref<PlaneShapeSettings>>("PlaneShapeSettingsRef")
        .constructor("plane", +[](const Plane &inPlane) -> Ref<PlaneShapeSettings> { return new PlaneShapeSettings(inPlane); })
        .property("mHalfExtent", &PlaneShapeSettings::mHalfExtent);

    // -- TransformedShape: world-space shape snapshot for picking (see GetTransformedShape) --
    jolt_class_<TransformedShape>("TransformedShape")
        // Ergonomic single-hit ray cast. origin/direction are plain [x,y,z]; the ray's reach
        // is the LENGTH of direction (Jolt convention). Returns null on a miss, else
        // { fraction, point:[x,y,z] } where point = origin + fraction * direction.
        .function("CastRay(origin, direction): { fraction: number; point: Vec3 } | null",
            +[](const TransformedShape &ts, RVec3 origin, Vec3 direction) -> val {
                RRayCast ray(origin, direction);
                RayCastResult hit;               // mFraction resets to >1 (miss sentinel)
                if (!ts.CastRay(ray, hit)) return val::null();
                RVec3 p = ray.GetPointOnRay(hit.mFraction);
                val out = val::object();
                out.set("fraction", hit.mFraction);
                val pt = val::array();
                pt.call<void>("push", p.GetX()); pt.call<void>("push", p.GetY()); pt.call<void>("push", p.GetZ());
                out.set("point", pt);
                return out;
            })
        .function("CastRayCollide(ray, settings, collector, shapeFilter)",
            +[](const TransformedShape &ts, const RRayCast &ray, const RayCastSettings &settings,
                CastRayCollector &collector, const ShapeFilter &shapeFilter) {
                ts.CastRay(ray, settings, collector, shapeFilter); }, allow_raw_pointers())
        .function("CollidePoint(point, collector, shapeFilter)",
            +[](const TransformedShape &ts, RVec3 point, CollidePointCollector &collector,
                const ShapeFilter &shapeFilter) {
                ts.CollidePoint(point, collector, shapeFilter); }, allow_raw_pointers())
        .function("CollideShape(shape, shapeScale, comTransform, settings, baseOffset, collector, shapeFilter)",
            +[](const TransformedShape &ts, const Shape *shape, Vec3 shapeScale, RMat44 comTransform,
                const CollideShapeSettings &settings, RVec3 baseOffset, CollideShapeCollector &collector,
                const ShapeFilter &shapeFilter) {
                ts.CollideShape(shape, shapeScale, comTransform, settings, baseOffset, collector, shapeFilter); }, allow_raw_pointers())
        .function("CastShape(shapeCast, settings, baseOffset, collector, shapeFilter)",
            +[](const TransformedShape &ts, const RShapeCast &shapeCast, const ShapeCastSettings &settings,
                RVec3 baseOffset, CastShapeCollector &collector, const ShapeFilter &shapeFilter) {
                ts.CastShape(shapeCast, settings, baseOffset, collector, shapeFilter); }, allow_raw_pointers())
        .out_function("GetShapeScale(out)", out_desc::Vec3,
            +[](const TransformedShape &ts, uintptr_t out) { WriteVec3(ts.GetShapeScale(), out); })
        .function("SetShapeScale(scale)", +[](TransformedShape &ts, Vec3 scale) { ts.SetShapeScale(scale); })
        .out_function("GetCenterOfMassTransform(out)", out_desc::Mat44,
            +[](const TransformedShape &ts, uintptr_t out) { RMat44 m = ts.GetCenterOfMassTransform(); WriteMat4(m, out); })
        .out_function("GetInverseCenterOfMassTransform(out)", out_desc::Mat44,
            +[](const TransformedShape &ts, uintptr_t out) { RMat44 m = ts.GetInverseCenterOfMassTransform(); WriteMat4(m, out); })
        .out_function("GetWorldTransform(out)", out_desc::Mat44,
            +[](const TransformedShape &ts, uintptr_t out) { RMat44 m = ts.GetWorldTransform(); WriteMat4(m, out); })
        .function("SetWorldTransform(position, rotation, scale)",
            +[](TransformedShape &ts, RVec3 pos, Quat rot, Vec3 scale) { ts.SetWorldTransform(pos, rot, scale); })
        .function("SetWorldTransformMat(transform)",
            +[](TransformedShape &ts, RMat44 t) { ts.SetWorldTransform(t); })
        .out_function("GetWorldSpaceBounds(out)", out_desc::AABox,
            +[](const TransformedShape &ts, uintptr_t out) { WriteAABox(ts.GetWorldSpaceBounds(), out); })
        .out_function("GetWorldSpaceSurfaceNormal(out, subShapeID, position)", out_desc::Vec3, out_desc::Pass, out_desc::Pass,
            +[](const TransformedShape &ts, uintptr_t out, uint32 subShapeID, RVec3 position) {
                WriteVec3(ts.GetWorldSpaceSurfaceNormal(toSubShapeID(subShapeID), position), out); })
        .function("GetSupportingFace(subShapeID, direction, baseOffset): Float32Array",
            +[](const TransformedShape &ts, uint32 subShapeID, Vec3 direction, RVec3 baseOffset) -> val {
                Shape::SupportingFace face;
                ts.GetSupportingFace(toSubShapeID(subShapeID), direction, baseOffset, face);
                std::vector<float> v; v.reserve(face.size() * 3);
                for (const Vec3 &p : face) { v.push_back(p.GetX()); v.push_back(p.GetY()); v.push_back(p.GetZ()); }
                val out = val::global("Float32Array").new_(v.size());
                out.call<void>("set", val(typed_memory_view(v.size(), v.data())));
                return out; })
        .function("GetMaterial(subShapeID)",
            +[](const TransformedShape &ts, uint32 subShapeID) {
                return const_cast<PhysicsMaterial *>(ts.GetMaterial(toSubShapeID(subShapeID))); }, allow_raw_pointers())
        .function("GetSubShapeUserData(subShapeID)",
            +[](const TransformedShape &ts, uint32 subShapeID) { return (uint32)ts.GetSubShapeUserData(toSubShapeID(subShapeID)); })
        .function("GetBodyID", +[](const TransformedShape &ts) { return fromBodyID(ts.mBodyID); })
        .function("GetShape", +[](const TransformedShape &ts) { return const_cast<Shape *>(ts.mShape.GetPtr()); }, allow_raw_pointers())
        .function("SetShape(shape)", +[](TransformedShape &ts, const Shape *s) { ts.mShape = s; }, allow_raw_pointers())
        .out_function("GetShapePositionCOM(out)", out_desc::Vec3,
            +[](const TransformedShape &ts, uintptr_t out) { WriteVec3(ts.mShapePositionCOM, out); })
        .function("SetShapePositionCOM(pos)", +[](TransformedShape &ts, RVec3 pos) { ts.mShapePositionCOM = pos; })
        .out_function("GetShapeRotation(out)", out_desc::Quat,
            +[](const TransformedShape &ts, uintptr_t out) { WriteQuat(ts.mShapeRotation, out); })
        .function("SetShapeRotation(rot)", +[](TransformedShape &ts, Quat rot) { ts.mShapeRotation = rot; });

    // ---- SoftBody: ergonomic shared-settings builder + creation settings ----
    // The old WebIDL API exposed the raw Vertex/Face/Edge/Volume structs plus Array
    // push_back + set_mVertex; here we take plain numbers/arrays and hide the structs.
    // (EBendType enum registered above with the other enums.)
    jolt_class_<SoftBodySharedSettings>("SoftBodySharedSettings")
        .smart_ptr<Ref<SoftBodySharedSettings>>("SoftBodySharedSettingsRef")
        .constructor(+[]() -> Ref<SoftBodySharedSettings> { return new SoftBodySharedSettings(); })
        .function("AddVertex(position, invMass)", +[](SoftBodySharedSettings &s, Vec3 pos, float invMass) {
            SoftBodySharedSettings::Vertex v; v.mPosition = Float3(pos.GetX(), pos.GetY(), pos.GetZ()); v.mInvMass = invMass; s.mVertices.push_back(v); })
        .function("AddFace(vertex0, vertex1, vertex2)", +[](SoftBodySharedSettings &s, uint32 v0, uint32 v1, uint32 v2) { s.AddFace(SoftBodySharedSettings::Face(v0, v1, v2)); })
        .function("AddEdgeConstraint(vertex0, vertex1, compliance)", +[](SoftBodySharedSettings &s, uint32 v0, uint32 v1, float compliance) { s.mEdgeConstraints.push_back(SoftBodySharedSettings::Edge(v0, v1, compliance)); })
        .function("AddVolumeConstraint(vertex0, vertex1, vertex2, vertex3, compliance)", +[](SoftBodySharedSettings &s, uint32 v0, uint32 v1, uint32 v2, uint32 v3, float compliance) {
            SoftBodySharedSettings::Volume vol; vol.mVertex[0] = v0; vol.mVertex[1] = v1; vol.mVertex[2] = v2; vol.mVertex[3] = v3; vol.mCompliance = compliance; s.mVolumeConstraints.push_back(vol); })
        .function("CalculateEdgeLengths", &SoftBodySharedSettings::CalculateEdgeLengths)
        .function("CalculateVolumeConstraintVolumes", &SoftBodySharedSettings::CalculateVolumeConstraintVolumes)
        // Build edge/shear/bend (and optional long-range-attachment) constraints from the
        // faces using a single vertex-attribute profile applied to every vertex (compliance
        // = inverse stiffness). Pass Jolt.ELRAType.None + 1.0 when LRA isn't needed.
        .function("CreateConstraints(compliance, shearCompliance, bendCompliance, bendType, lraType, lraMaxDistanceMultiplier)", +[](SoftBodySharedSettings &s, float compliance, float shearCompliance, float bendCompliance, SoftBodySharedSettings::EBendType bendType, SoftBodySharedSettings::ELRAType lraType, float lraMaxDistanceMultiplier) {
            SoftBodySharedSettings::VertexAttributes attr;
            attr.mCompliance = compliance; attr.mShearCompliance = shearCompliance; attr.mBendCompliance = bendCompliance;
            attr.mLRAType = lraType; attr.mLRAMaxDistanceMultiplier = lraMaxDistanceMultiplier;
            s.CreateConstraints(&attr, 1, bendType); })
        .function("Optimize", +[](SoftBodySharedSettings &s) { s.Optimize(); })
        // Seed every vertex with an initial velocity (e.g. to launch a soft body sideways).
        .function("SetAllVertexVelocities(velocity)", +[](SoftBodySharedSettings &s, Vec3 v) {
            Float3 f(v.GetX(), v.GetY(), v.GetZ());
            for (auto &vert : s.mVertices) vert.mVelocity = f; })
        // Static triangle indices (3 uint32 per face) for building a render mesh.
        .function("GetFaceIndices: Uint32Array", +[](const SoftBodySharedSettings &s) -> val {
            std::vector<uint32> idx; idx.reserve(s.mFaces.size() * 3);
            for (const auto &f : s.mFaces) { idx.push_back(f.mVertex[0]); idx.push_back(f.mVertex[1]); idx.push_back(f.mVertex[2]); }
            val a = val::global("Uint32Array").new_(idx.size());
            a.call<void>("set", val(typed_memory_view(idx.size(), idx.data())));
            return a; })
        // ---- struct-typed builder entry points (complement the scalar AddVertex/AddFace helpers) ----
        .function("AddFaceStruct(face)", +[](SoftBodySharedSettings &s, const SoftBodySharedSettings::Face &f) { s.AddFace(f); }, allow_raw_pointers())
        .function("AddVertexStruct(vertex)", +[](SoftBodySharedSettings &s, const SoftBodySharedSettings::Vertex &v) { s.mVertices.push_back(v); }, allow_raw_pointers())
        .function("AddEdgeConstraintStruct(edge)", +[](SoftBodySharedSettings &s, const SoftBodySharedSettings::Edge &e) { s.mEdgeConstraints.push_back(e); }, allow_raw_pointers())
        .function("AddDihedralBendConstraint(bend)", +[](SoftBodySharedSettings &s, const SoftBodySharedSettings::DihedralBend &d) { s.mDihedralBendConstraints.push_back(d); }, allow_raw_pointers())
        .function("AddVolumeConstraintStruct(volume)", +[](SoftBodySharedSettings &s, const SoftBodySharedSettings::Volume &v) { s.mVolumeConstraints.push_back(v); }, allow_raw_pointers())
        .function("AddRod(rod)", +[](SoftBodySharedSettings &s, const SoftBodySharedSettings::RodStretchShear &r) { s.mRodStretchShearConstraints.push_back(r); }, allow_raw_pointers())
        .function("AddRodBendTwist(rod)", +[](SoftBodySharedSettings &s, const SoftBodySharedSettings::RodBendTwist &r) { s.mRodBendTwistConstraints.push_back(r); }, allow_raw_pointers())
        .function("AddLRAConstraint(lra)", +[](SoftBodySharedSettings &s, const SoftBodySharedSettings::LRA &l) { s.mLRAConstraints.push_back(l); }, allow_raw_pointers())
        .function("AddSkinnedConstraint(skinned)", +[](SoftBodySharedSettings &s, const SoftBodySharedSettings::Skinned &sk) { s.mSkinnedConstraints.push_back(sk); }, allow_raw_pointers())
        .function("AddInvBindMatrix(invBind)", +[](SoftBodySharedSettings &s, const SoftBodySharedSettings::InvBind &b) { s.mInvBindMatrices.push_back(b); }, allow_raw_pointers())
        // ---- calculation / setup passthroughs ----
        .function("CalculateBendConstraintConstants", &SoftBodySharedSettings::CalculateBendConstraintConstants)
        .function("CalculateSkinnedConstraintNormals", &SoftBodySharedSettings::CalculateSkinnedConstraintNormals)
        .function("CalculateRodProperties", &SoftBodySharedSettings::CalculateRodProperties)
        .function("CalculateLRALengths(maxDistanceMultiplier)", +[](SoftBodySharedSettings &s, float m) { s.CalculateLRALengths(m); })
        // Full CreateConstraints taking a JS array of VertexAttributes.
        .function("CreateConstraintsFromAttributes(attributesArray, bendType, angleTolerance)",
            +[](SoftBodySharedSettings &s, val attributesArray, SoftBodySharedSettings::EBendType bendType, float angleTolerance) {
                const unsigned n = attributesArray["length"].as<unsigned>();
                std::vector<SoftBodySharedSettings::VertexAttributes> attrs(n);
                for (unsigned i = 0; i < n; ++i) attrs[i] = attributesArray[i].as<SoftBodySharedSettings::VertexAttributes>();
                s.CreateConstraints(attrs.data(), n, bendType, angleTolerance); })
        .function("Clone", +[](const SoftBodySharedSettings &s) -> Ref<SoftBodySharedSettings> { return s.Clone(); })
        // ---- collection accessors (count + index handle) ----
        .function("GetNumVertices", +[](const SoftBodySharedSettings &s) { return (uint32)s.mVertices.size(); })
        .function("GetVertexStruct(index)", +[](SoftBodySharedSettings &s, uint32 i) { return &s.mVertices[i]; }, allow_raw_pointers())
        .function("GetNumFaces", +[](const SoftBodySharedSettings &s) { return (uint32)s.mFaces.size(); })
        .function("GetFace(index)", +[](SoftBodySharedSettings &s, uint32 i) { return &s.mFaces[i]; }, allow_raw_pointers())
        .function("GetNumEdgeConstraints", +[](const SoftBodySharedSettings &s) { return (uint32)s.mEdgeConstraints.size(); })
        .function("GetEdgeConstraint(index)", +[](SoftBodySharedSettings &s, uint32 i) { return &s.mEdgeConstraints[i]; }, allow_raw_pointers())
        .function("GetNumVolumeConstraints", +[](const SoftBodySharedSettings &s) { return (uint32)s.mVolumeConstraints.size(); })
        .function("GetVolumeConstraint(index)", +[](SoftBodySharedSettings &s, uint32 i) { return &s.mVolumeConstraints[i]; }, allow_raw_pointers())
        .function("GetNumDihedralBendConstraints", +[](const SoftBodySharedSettings &s) { return (uint32)s.mDihedralBendConstraints.size(); })
        .function("GetDihedralBendConstraint(index)", +[](SoftBodySharedSettings &s, uint32 i) { return &s.mDihedralBendConstraints[i]; }, allow_raw_pointers())
        .function("GetNumSkinnedConstraints", +[](const SoftBodySharedSettings &s) { return (uint32)s.mSkinnedConstraints.size(); })
        .function("GetSkinnedConstraint(index)", +[](SoftBodySharedSettings &s, uint32 i) { return &s.mSkinnedConstraints[i]; }, allow_raw_pointers())
        .function("GetNumInvBindMatrices", +[](const SoftBodySharedSettings &s) { return (uint32)s.mInvBindMatrices.size(); })
        .function("GetInvBindMatrix(index)", +[](SoftBodySharedSettings &s, uint32 i) { return &s.mInvBindMatrices[i]; }, allow_raw_pointers())
        .function("GetNumLRAConstraints", +[](const SoftBodySharedSettings &s) { return (uint32)s.mLRAConstraints.size(); })
        .function("GetLRAConstraint(index)", +[](SoftBodySharedSettings &s, uint32 i) { return &s.mLRAConstraints[i]; }, allow_raw_pointers())
        .function("GetNumRodStretchShearConstraints", +[](const SoftBodySharedSettings &s) { return (uint32)s.mRodStretchShearConstraints.size(); })
        .function("GetRodStretchShearConstraint(index)", +[](SoftBodySharedSettings &s, uint32 i) { return &s.mRodStretchShearConstraints[i]; }, allow_raw_pointers())
        .function("GetNumRodBendTwistConstraints", +[](const SoftBodySharedSettings &s) { return (uint32)s.mRodBendTwistConstraints.size(); })
        .function("GetRodBendTwistConstraint(index)", +[](SoftBodySharedSettings &s, uint32 i) { return &s.mRodBendTwistConstraints[i]; }, allow_raw_pointers())
        .function("GetNumMaterials", +[](const SoftBodySharedSettings &s) { return (uint32)s.mMaterials.size(); })
        .function("GetMaterial(index)", +[](const SoftBodySharedSettings &s, uint32 i) {
            return const_cast<PhysicsMaterial *>(s.mMaterials[i].GetPtr()); }, allow_raw_pointers())
        .function("SetMaterial(index, material)", +[](SoftBodySharedSettings &s, uint32 i, PhysicsMaterial *m) { s.mMaterials[i] = m; }, allow_raw_pointers())
        .function("AddMaterial(material)", +[](SoftBodySharedSettings &s, PhysicsMaterial *m) { s.mMaterials.push_back(m); return (uint32)(s.mMaterials.size() - 1); }, allow_raw_pointers());
    jolt_class_<SoftBodyCreationSettings>("SoftBodyCreationSettings")
        .constructor<>()
        .constructor("settings, position, rotation, objectLayer", +[](const SoftBodySharedSettings *settings, RVec3 pos, Quat rot, ObjectLayer layer) -> SoftBodyCreationSettings * {
            return new SoftBodyCreationSettings(settings, pos, rot, layer); }, allow_raw_pointers())
        .property("mPosition", &SoftBodyCreationSettings::mPosition)
        .property("mRotation", &SoftBodyCreationSettings::mRotation)
        .property("mObjectLayer", &SoftBodyCreationSettings::mObjectLayer)
        .property("mPressure", &SoftBodyCreationSettings::mPressure)
        .property("mNumIterations", &SoftBodyCreationSettings::mNumIterations)
        .property("mLinearDamping", &SoftBodyCreationSettings::mLinearDamping)
        .property("mVertexRadius", &SoftBodyCreationSettings::mVertexRadius)
        .property("mFriction", &SoftBodyCreationSettings::mFriction)
        .property("mRestitution", &SoftBodyCreationSettings::mRestitution)
        .property("mGravityFactor", &SoftBodyCreationSettings::mGravityFactor)
        // Set false for a soft body attached to the static world (COM won't be updated).
        .property("mUpdatePosition", &SoftBodyCreationSettings::mUpdatePosition)
        .property("mUserData", &SoftBodyCreationSettings::mUserData)
        .property("mMaxLinearVelocity", &SoftBodyCreationSettings::mMaxLinearVelocity)
        .property("mMakeRotationIdentity", &SoftBodyCreationSettings::mMakeRotationIdentity)
        .property("mAllowSleeping", &SoftBodyCreationSettings::mAllowSleeping)
        .property("mFacesDoubleSided", &SoftBodyCreationSettings::mFacesDoubleSided)
        .function("GetCollisionGroup", +[](SoftBodyCreationSettings &s) { return &s.mCollisionGroup; }, allow_raw_pointers())
        // mSettings: RefConst<SoftBodySharedSettings>. Getter const_casts so JS can call methods.
        .function("GetSettings", +[](const SoftBodyCreationSettings &s) {
            return const_cast<SoftBodySharedSettings *>(s.mSettings.GetPtr()); }, allow_raw_pointers())
        .function("SetSettings(settings)", +[](SoftBodyCreationSettings &s, const SoftBodySharedSettings *settings) { s.mSettings = settings; }, allow_raw_pointers());

    // ==== soft body runtime ==================================================
    // Runtime particle. At runtime only mInvMass / mVelocity should be modified (header note).
    jolt_class_<SoftBodyVertex>("SoftBodyVertex")
        .constructor<>()
        .out_function("GetPosition(out)",         out_desc::Vec3, +[](const SoftBodyVertex &v, uintptr_t out) { WriteVec3(v.mPosition, out); })
        .out_function("GetPreviousPosition(out)", out_desc::Vec3, +[](const SoftBodyVertex &v, uintptr_t out) { WriteVec3(v.mPreviousPosition, out); })
        .out_function("GetVelocity(out)",         out_desc::Vec3, +[](const SoftBodyVertex &v, uintptr_t out) { WriteVec3(v.mVelocity, out); })
        .function("SetPosition(v)",         +[](SoftBodyVertex &v, Vec3 p) { v.mPosition = p; })
        .function("SetVelocity(v)",         +[](SoftBodyVertex &v, Vec3 p) { v.mVelocity = p; })
        .function("GetInvMass",             +[](const SoftBodyVertex &v) { return v.mInvMass; })
        .function("SetInvMass(invMass)",    +[](SoftBodyVertex &v, float m) { v.mInvMass = m; })
        .function("GetCollidingShapeIndex", +[](const SoftBodyVertex &v) { return v.mCollidingShapeIndex; })
        .function("GetLargestPenetration", +[](const SoftBodyVertex &v) { return v.mLargestPenetration; })
        .function("HasContact",            +[](const SoftBodyVertex &v) { return v.mHasContact; });

    jolt_class_<SoftBodyContactSettings>("SoftBodyContactSettings")
        .constructor<>()
        .property("mInvMassScale1",    &SoftBodyContactSettings::mInvMassScale1)
        .property("mInvMassScale2",    &SoftBodyContactSettings::mInvMassScale2)
        .property("mInvInertiaScale2", &SoftBodyContactSettings::mInvInertiaScale2)
        .property("mIsSensor",         &SoftBodyContactSettings::mIsSensor);

    // Contact query interface passed into OnSoftBodyContactAdded (non-owning handle only).
    jolt_class_<SoftBodyManifold>("SoftBodyManifold")
        .function("HasContact(vertex)",           +[](const SoftBodyManifold &m, const SoftBodyVertex &v) { return m.HasContact(v); }, allow_raw_pointers())
        .out_function("GetLocalContactPoint(out, vertex)", out_desc::Vec3, out_desc::Pass,
            +[](const SoftBodyManifold &m, uintptr_t out, const SoftBodyVertex &v) { WriteVec3(m.GetLocalContactPoint(v), out); })
        .out_function("GetContactNormal(out, vertex)", out_desc::Vec3, out_desc::Pass,
            +[](const SoftBodyManifold &m, uintptr_t out, const SoftBodyVertex &v) { WriteVec3(m.GetContactNormal(v), out); })
        .function("GetContactBodyID(vertex)",     +[](const SoftBodyManifold &m, const SoftBodyVertex &v) { return (uint32)m.GetContactBodyID(v).GetIndexAndSequenceNumber(); }, allow_raw_pointers())
        .function("GetNumSensorContacts",         +[](const SoftBodyManifold &m) { return (uint32)m.GetNumSensorContacts(); })
        .function("GetSensorContactBodyID(index)",+[](const SoftBodyManifold &m, uint32 i) { return (uint32)m.GetSensorContactBodyID(i).GetIndexAndSequenceNumber(); })
        .function("GetNumVertices",               +[](const SoftBodyManifold &m) { return (uint32)m.GetVertices().size(); })
        .function("GetVertex(index)",             +[](const SoftBodyManifold &m, uint32 i) { return const_cast<SoftBodyVertex *>(&m.GetVertices()[i]); }, allow_raw_pointers());

    jolt_class_<SoftBodyShape, base<Shape>>("SoftBodyShape")
        .smart_ptr<Ref<SoftBodyShape>>("SoftBodyShapeRef")
        .function("GetSubShapeIDBits", +[](const SoftBodyShape &s) { return (uint32)s.GetSubShapeIDBits(); })
        .function("GetFaceIndex(subShapeID)", +[](const SoftBodyShape &s, uint32 subShapeID) {
            return (uint32)s.GetFaceIndex(toSubShapeID(subShapeID)); })
        .out_function("GetLocalBounds(out)", out_desc::AABox, +[](const SoftBodyShape &s, uintptr_t out) { WriteAABox(s.GetLocalBounds(), out); })
        .function("GetVolume", +[](const SoftBodyShape &s) { return s.GetVolume(); });

    jolt_class_<SoftBodyMotionProperties, base<MotionProperties>>("SoftBodyMotionProperties")
        .function("GetSettings", +[](const SoftBodyMotionProperties &s) {
            return const_cast<SoftBodySharedSettings *>(s.GetSettings()); }, allow_raw_pointers())
        .function("GetNumVertices", +[](const SoftBodyMotionProperties &s) { return (uint32)s.GetVertices().size(); })
        .function("GetVertex(index)", +[](SoftBodyMotionProperties &s, uint32 i) { return &s.GetVertex(i); }, allow_raw_pointers())
        .function("GetNumFaces", +[](const SoftBodyMotionProperties &s) { return (uint32)s.GetFaces().size(); })
        .function("GetFace(index)", +[](const SoftBodyMotionProperties &s, uint32 i) {
            return const_cast<SoftBodySharedSettings::Face *>(&s.GetFace(i)); }, allow_raw_pointers())
        .function("GetNumMaterials", +[](const SoftBodyMotionProperties &s) { return (uint32)s.GetMaterials().size(); })
        .function("GetMaterial(index)", +[](const SoftBodyMotionProperties &s, uint32 i) {
            return const_cast<PhysicsMaterial *>(s.GetMaterials()[i].GetPtr()); }, allow_raw_pointers())
        .function("GetNumIterations", +[](const SoftBodyMotionProperties &s) { return (uint32)s.GetNumIterations(); })
        .function("SetNumIterations(n)", +[](SoftBodyMotionProperties &s, uint32 n) { s.SetNumIterations(n); })
        .function("GetPressure", &SoftBodyMotionProperties::GetPressure)
        .function("SetPressure(pressure)", &SoftBodyMotionProperties::SetPressure)
        .function("GetEnableSkinConstraints", &SoftBodyMotionProperties::GetEnableSkinConstraints)
        .function("SetEnableSkinConstraints(enable)", &SoftBodyMotionProperties::SetEnableSkinConstraints)
        .function("GetSkinnedMaxDistanceMultiplier", &SoftBodyMotionProperties::GetSkinnedMaxDistanceMultiplier)
        .function("SetSkinnedMaxDistanceMultiplier(m)", &SoftBodyMotionProperties::SetSkinnedMaxDistanceMultiplier)
        .function("GetVolume", &SoftBodyMotionProperties::GetVolume)
        .out_function("GetLocalBounds(out)", out_desc::AABox, +[](const SoftBodyMotionProperties &s, uintptr_t out) { WriteAABox(s.GetLocalBounds(), out); })
        .function("CustomUpdate(deltaTime, softBody, system)",
            +[](SoftBodyMotionProperties &s, float dt, Body &b, PhysicsSystem &sys) { s.CustomUpdate(dt, b, sys); }, allow_raw_pointers())
        .function("SkinVertices(rootTransform, jointMatricesPtr, numJoints, hardSkinAll, jolt)",
            +[](SoftBodyMotionProperties &s, RMat44 rootTransform, uintptr_t jointMatricesPtr, uint32 numJoints, bool hardSkinAll, JoltInterface &jolt) {
                s.SkinVertices(rootTransform, reinterpret_cast<const Mat44 *>(jointMatricesPtr), numJoints, hardSkinAll, *jolt.GetTempAllocator());
            }, allow_raw_pointers());

    // -- nested builder / constraint structs (POD-ish; fixed arrays via indexed get/set) --
    jolt_class_<SoftBodySharedSettings::Vertex>("SoftBodySharedSettingsVertex")
        .constructor<>()
        .out_function("GetPosition(out)", out_desc::Vec3, +[](const SoftBodySharedSettings::Vertex &v, uintptr_t out) { WriteVec3(Vec3(v.mPosition.x, v.mPosition.y, v.mPosition.z), out); })
        .function("SetPosition(v)", +[](SoftBodySharedSettings::Vertex &v, Vec3 p) { v.mPosition = Float3(p.GetX(), p.GetY(), p.GetZ()); })
        .out_function("GetVelocity(out)", out_desc::Vec3, +[](const SoftBodySharedSettings::Vertex &v, uintptr_t out) { WriteVec3(Vec3(v.mVelocity.x, v.mVelocity.y, v.mVelocity.z), out); })
        .function("SetVelocity(v)", +[](SoftBodySharedSettings::Vertex &v, Vec3 p) { v.mVelocity = Float3(p.GetX(), p.GetY(), p.GetZ()); })
        .property("mInvMass", &SoftBodySharedSettings::Vertex::mInvMass);
    jolt_class_<SoftBodySharedSettings::Face>("SoftBodySharedSettingsFace")
        .constructor<>()
        .constructor("vertex0, vertex1, vertex2, materialIndex", +[](uint32 v0, uint32 v1, uint32 v2, uint32 mat) -> SoftBodySharedSettings::Face * {
            return new SoftBodySharedSettings::Face(v0, v1, v2, mat); }, allow_raw_pointers())
        .function("GetVertex(index)", +[](const SoftBodySharedSettings::Face &f, uint32 i) { return f.mVertex[i]; })
        .function("SetVertex(index, value)", +[](SoftBodySharedSettings::Face &f, uint32 i, uint32 val) { f.mVertex[i] = val; })
        .property("mMaterialIndex", &SoftBodySharedSettings::Face::mMaterialIndex);
    jolt_class_<SoftBodySharedSettings::Edge>("SoftBodySharedSettingsEdge")
        .constructor<>()
        .constructor("vertex0, vertex1, compliance", +[](uint32 v0, uint32 v1, float compliance) -> SoftBodySharedSettings::Edge * {
            return new SoftBodySharedSettings::Edge(v0, v1, compliance); }, allow_raw_pointers())
        .function("GetVertex(index)", +[](const SoftBodySharedSettings::Edge &e, uint32 i) { return e.mVertex[i]; })
        .function("SetVertex(index, value)", +[](SoftBodySharedSettings::Edge &e, uint32 i, uint32 val) { e.mVertex[i] = val; })
        .property("mRestLength", &SoftBodySharedSettings::Edge::mRestLength)
        .property("mCompliance", &SoftBodySharedSettings::Edge::mCompliance);
    jolt_class_<SoftBodySharedSettings::DihedralBend>("SoftBodySharedSettingsDihedralBend")
        .constructor<>()
        .constructor("vertex0, vertex1, vertex2, vertex3, compliance", +[](uint32 v0, uint32 v1, uint32 v2, uint32 v3, float compliance) -> SoftBodySharedSettings::DihedralBend * {
            return new SoftBodySharedSettings::DihedralBend(v0, v1, v2, v3, compliance); }, allow_raw_pointers())
        .function("GetVertex(index)", +[](const SoftBodySharedSettings::DihedralBend &d, uint32 i) { return d.mVertex[i]; })
        .function("SetVertex(index, value)", +[](SoftBodySharedSettings::DihedralBend &d, uint32 i, uint32 val) { d.mVertex[i] = val; })
        .property("mCompliance", &SoftBodySharedSettings::DihedralBend::mCompliance)
        .property("mInitialAngle", &SoftBodySharedSettings::DihedralBend::mInitialAngle);
    jolt_class_<SoftBodySharedSettings::Volume>("SoftBodySharedSettingsVolume")
        .constructor<>()
        .constructor("vertex0, vertex1, vertex2, vertex3, compliance", +[](uint32 v0, uint32 v1, uint32 v2, uint32 v3, float compliance) -> SoftBodySharedSettings::Volume * {
            return new SoftBodySharedSettings::Volume(v0, v1, v2, v3, compliance); }, allow_raw_pointers())
        .function("GetVertex(index)", +[](const SoftBodySharedSettings::Volume &v, uint32 i) { return v.mVertex[i]; })
        .function("SetVertex(index, value)", +[](SoftBodySharedSettings::Volume &v, uint32 i, uint32 val) { v.mVertex[i] = val; })
        .property("mSixRestVolume", &SoftBodySharedSettings::Volume::mSixRestVolume)
        .property("mCompliance", &SoftBodySharedSettings::Volume::mCompliance);
    jolt_class_<SoftBodySharedSettings::InvBind>("SoftBodySharedSettingsInvBind")
        .constructor<>()
        .constructor("jointIndex, invBind", +[](uint32 jointIndex, RMat44 invBind) -> SoftBodySharedSettings::InvBind * {
            return new SoftBodySharedSettings::InvBind(jointIndex, invBind); }, allow_raw_pointers())
        .property("mJointIndex", &SoftBodySharedSettings::InvBind::mJointIndex)
        .out_function("GetInvBind(out)", out_desc::Mat44, +[](const SoftBodySharedSettings::InvBind &b, uintptr_t out) { WriteMat4(b.mInvBind, out); })
        .function("SetInvBind(m)", +[](SoftBodySharedSettings::InvBind &b, Mat44 m) { b.mInvBind = m; });
    jolt_class_<SoftBodySharedSettings::SkinWeight>("SoftBodySharedSettingsSkinWeight")
        .constructor<>()
        .constructor("invBindIndex, weight", +[](uint32 invBindIndex, float weight) -> SoftBodySharedSettings::SkinWeight * {
            return new SoftBodySharedSettings::SkinWeight(invBindIndex, weight); }, allow_raw_pointers())
        .property("mInvBindIndex", &SoftBodySharedSettings::SkinWeight::mInvBindIndex)
        .property("mWeight", &SoftBodySharedSettings::SkinWeight::mWeight);
    jolt_class_<SoftBodySharedSettings::Skinned>("SoftBodySharedSettingsSkinned")
        .constructor<>()
        .constructor("vertex, maxDistance, backStopDistance, backStopRadius", +[](uint32 vertex, float maxDistance, float backStopDistance, float backStopRadius) -> SoftBodySharedSettings::Skinned * {
            return new SoftBodySharedSettings::Skinned(vertex, maxDistance, backStopDistance, backStopRadius); }, allow_raw_pointers())
        .property("mVertex", &SoftBodySharedSettings::Skinned::mVertex)
        .property("mMaxDistance", &SoftBodySharedSettings::Skinned::mMaxDistance)
        .property("mBackStopDistance", &SoftBodySharedSettings::Skinned::mBackStopDistance)
        .property("mBackStopRadius", &SoftBodySharedSettings::Skinned::mBackStopRadius)
        .function("GetWeight(index)", +[](SoftBodySharedSettings::Skinned &s, uint32 i) { return &s.mWeights[i]; }, allow_raw_pointers())
        .function("SetWeight(index, invBindIndex, weight)", +[](SoftBodySharedSettings::Skinned &s, uint32 i, uint32 invBindIndex, float weight) {
            s.mWeights[i] = SoftBodySharedSettings::SkinWeight(invBindIndex, weight); })
        .function("NormalizeWeights", &SoftBodySharedSettings::Skinned::NormalizeWeights);
    jolt_class_<SoftBodySharedSettings::LRA>("SoftBodySharedSettingsLRA")
        .constructor<>()
        .constructor("vertex0, vertex1, maxDistance", +[](uint32 v0, uint32 v1, float maxDistance) -> SoftBodySharedSettings::LRA * {
            return new SoftBodySharedSettings::LRA(v0, v1, maxDistance); }, allow_raw_pointers())
        .function("GetVertex(index)", +[](const SoftBodySharedSettings::LRA &l, uint32 i) { return l.mVertex[i]; })
        .function("SetVertex(index, value)", +[](SoftBodySharedSettings::LRA &l, uint32 i, uint32 val) { l.mVertex[i] = val; })
        .property("mMaxDistance", &SoftBodySharedSettings::LRA::mMaxDistance);
    jolt_class_<SoftBodySharedSettings::RodStretchShear>("SoftBodySharedSettingsRodStretchShear")
        .constructor<>()
        .constructor("vertex0, vertex1, compliance", +[](uint32 v0, uint32 v1, float compliance) -> SoftBodySharedSettings::RodStretchShear * {
            return new SoftBodySharedSettings::RodStretchShear(v0, v1, compliance); }, allow_raw_pointers())
        .function("GetVertex(index)", +[](const SoftBodySharedSettings::RodStretchShear &r, uint32 i) { return r.mVertex[i]; })
        .function("SetVertex(index, value)", +[](SoftBodySharedSettings::RodStretchShear &r, uint32 i, uint32 val) { r.mVertex[i] = val; })
        .property("mLength", &SoftBodySharedSettings::RodStretchShear::mLength)
        .property("mInvMass", &SoftBodySharedSettings::RodStretchShear::mInvMass)
        .property("mCompliance", &SoftBodySharedSettings::RodStretchShear::mCompliance)
        .out_function("GetBishop(out)", out_desc::Quat, +[](const SoftBodySharedSettings::RodStretchShear &r, uintptr_t out) { WriteQuat(r.mBishop, out); })
        .function("SetBishop(q)", +[](SoftBodySharedSettings::RodStretchShear &r, Quat q) { r.mBishop = q; });
    jolt_class_<SoftBodySharedSettings::RodBendTwist>("SoftBodySharedSettingsRodBendTwist")
        .constructor<>()
        .constructor("rod0, rod1, compliance", +[](uint32 rod0, uint32 rod1, float compliance) -> SoftBodySharedSettings::RodBendTwist * {
            return new SoftBodySharedSettings::RodBendTwist(rod0, rod1, compliance); }, allow_raw_pointers())
        .function("GetRod(index)", +[](const SoftBodySharedSettings::RodBendTwist &r, uint32 i) { return r.mRod[i]; })
        .function("SetRod(index, value)", +[](SoftBodySharedSettings::RodBendTwist &r, uint32 i, uint32 val) { r.mRod[i] = val; })
        .property("mCompliance", &SoftBodySharedSettings::RodBendTwist::mCompliance)
        .out_function("GetOmega0(out)", out_desc::Quat, +[](const SoftBodySharedSettings::RodBendTwist &r, uintptr_t out) { WriteQuat(r.mOmega0, out); })
        .function("SetOmega0(q)", +[](SoftBodySharedSettings::RodBendTwist &r, Quat q) { r.mOmega0 = q; });
    jolt_class_<SoftBodySharedSettings::VertexAttributes>("SoftBodySharedSettingsVertexAttributes")
        .constructor<>()
        .property("mCompliance", &SoftBodySharedSettings::VertexAttributes::mCompliance)
        .property("mShearCompliance", &SoftBodySharedSettings::VertexAttributes::mShearCompliance)
        .property("mBendCompliance", &SoftBodySharedSettings::VertexAttributes::mBendCompliance)
        .property("mLRAType", &SoftBodySharedSettings::VertexAttributes::mLRAType)
        .property("mLRAMaxDistanceMultiplier", &SoftBodySharedSettings::VertexAttributes::mLRAMaxDistanceMultiplier);

    jolt_class_<SoftBodyContactListener>("SoftBodyContactListener")
        .allow_subclass<SoftBodyContactListenerWrapper>("SoftBodyContactListenerWrapper");

    // ---- body creation settings ----
    jolt_class_<BodyCreationSettings>("BodyCreationSettings")
        .constructor<>()
        .constructor<const Shape *, RVec3, Quat, EMotionType, ObjectLayer>("shape, position, rotation, motionType, objectLayer")
        .property("mPosition", &BodyCreationSettings::mPosition)
        .property("mRotation", &BodyCreationSettings::mRotation)
        .property("mLinearVelocity", &BodyCreationSettings::mLinearVelocity)
        .property("mAngularVelocity", &BodyCreationSettings::mAngularVelocity)
        .property("mUserData", &BodyCreationSettings::mUserData)         // uint64 <-> BigInt
        .property("mObjectLayer", &BodyCreationSettings::mObjectLayer)
        .property("mMotionType", &BodyCreationSettings::mMotionType)
        .property("mAllowedDOFs", &BodyCreationSettings::mAllowedDOFs)
        .property("mAllowDynamicOrKinematic", &BodyCreationSettings::mAllowDynamicOrKinematic)
        .property("mIsSensor", &BodyCreationSettings::mIsSensor)
        .property("mCollideKinematicVsNonDynamic", &BodyCreationSettings::mCollideKinematicVsNonDynamic)
        .property("mUseManifoldReduction", &BodyCreationSettings::mUseManifoldReduction)
        .property("mApplyGyroscopicForce", &BodyCreationSettings::mApplyGyroscopicForce)
        .property("mMotionQuality", &BodyCreationSettings::mMotionQuality)
        .property("mEnhancedInternalEdgeRemoval", &BodyCreationSettings::mEnhancedInternalEdgeRemoval)
        .property("mAllowSleeping", &BodyCreationSettings::mAllowSleeping)
        .property("mFriction", &BodyCreationSettings::mFriction)
        .property("mRestitution", &BodyCreationSettings::mRestitution)
        .property("mLinearDamping", &BodyCreationSettings::mLinearDamping)
        .property("mAngularDamping", &BodyCreationSettings::mAngularDamping)
        .property("mMaxLinearVelocity", &BodyCreationSettings::mMaxLinearVelocity)
        .property("mMaxAngularVelocity", &BodyCreationSettings::mMaxAngularVelocity)
        .property("mGravityFactor", &BodyCreationSettings::mGravityFactor)
        .property("mNumVelocityStepsOverride", &BodyCreationSettings::mNumVelocityStepsOverride)
        .property("mNumPositionStepsOverride", &BodyCreationSettings::mNumPositionStepsOverride)
        .property("mOverrideMassProperties", &BodyCreationSettings::mOverrideMassProperties)
        .property("mInertiaMultiplier", &BodyCreationSettings::mInertiaMultiplier)
        // sub-objects: non-owning handles to mutate in place
        .function("GetCollisionGroup", +[](BodyCreationSettings &s) { return &s.mCollisionGroup; }, allow_raw_pointers())
        .function("GetMassPropertiesOverride", +[](BodyCreationSettings &s) { return &s.mMassPropertiesOverride; }, allow_raw_pointers())
        .function("GetShapeSettings", +[](const BodyCreationSettings &s) { return const_cast<ShapeSettings *>(s.GetShapeSettings()); }, allow_raw_pointers())
        .function("SetShapeSettings(shape)", +[](BodyCreationSettings &s, const ShapeSettings *shape) { s.SetShapeSettings(shape); }, allow_raw_pointers())
        .function("GetShape", +[](const BodyCreationSettings &s) { return const_cast<Shape *>(s.GetShape()); }, allow_raw_pointers())
        .function("SetShape(shape)", +[](BodyCreationSettings &s, const Shape *shape) { s.SetShape(shape); }, allow_raw_pointers())
        .function("HasMassProperties", &BodyCreationSettings::HasMassProperties)
        .function("GetMassProperties", +[](const BodyCreationSettings &s) { return s.GetMassProperties(); });

    // ---- BodyInterface (non-owning; obtained from PhysicsSystem) ----
    jolt_class_<BodyInterface>("BodyInterface")
        .function("CreateBody(settings)", &BodyInterface::CreateBody, allow_raw_pointers())
        .function("CreateSoftBody(settings)", &BodyInterface::CreateSoftBody, allow_raw_pointers())
        .function("CreateAndAddBody(settings, activationMode)",
            +[](BodyInterface &bi, const BodyCreationSettings &s, EActivation a) { return fromBodyID(bi.CreateAndAddBody(s, a)); })
        .function("AddBody(bodyID, activationMode)",
            +[](BodyInterface &bi, uint32 id, EActivation a) { bi.AddBody(toBodyID(id), a); })
        .function("RemoveBody(bodyID)", +[](BodyInterface &bi, uint32 id) { bi.RemoveBody(toBodyID(id)); })
        .function("DestroyBody(bodyID)", +[](BodyInterface &bi, uint32 id) { bi.DestroyBody(toBodyID(id)); })
        .function("ActivateBody(bodyID)", +[](BodyInterface &bi, uint32 id) { bi.ActivateBody(toBodyID(id)); })
        .function("DeactivateBody(bodyID)", +[](BodyInterface &bi, uint32 id) { bi.DeactivateBody(toBodyID(id)); })
        .function("IsActive(bodyID)", +[](const BodyInterface &bi, uint32 id) { return bi.IsActive(toBodyID(id)); })
        // value-type reads use the out-param paradigm (see facade.js): the raw
        // *Into writer takes a heap ptr; facade exposes GetX(out, ...) -> out.
        .out_function("GetPosition(out, bodyID)",
            out_desc::Vec3, out_desc::Pass, +[](const BodyInterface &bi, uintptr_t out, uint32 id) { WriteVec3(bi.GetPosition(toBodyID(id)), out); })
        .out_function("GetCenterOfMassPosition(out, bodyID)",
            out_desc::Vec3, out_desc::Pass, +[](const BodyInterface &bi, uintptr_t out, uint32 id) { WriteVec3(bi.GetCenterOfMassPosition(toBodyID(id)), out); })
        .out_function("GetRotation(out, bodyID)",
            out_desc::Quat, out_desc::Pass, +[](const BodyInterface &bi, uintptr_t out, uint32 id) { WriteQuat(bi.GetRotation(toBodyID(id)), out); })
        .out_function("GetLinearVelocity(out, bodyID)",
            out_desc::Vec3, out_desc::Pass, +[](const BodyInterface &bi, uintptr_t out, uint32 id) { WriteVec3(bi.GetLinearVelocity(toBodyID(id)), out); })
        .out_function("GetAngularVelocity(out, bodyID)",
            out_desc::Vec3, out_desc::Pass, +[](const BodyInterface &bi, uintptr_t out, uint32 id) { WriteVec3(bi.GetAngularVelocity(toBodyID(id)), out); })
        .out_function("GetWorldTransform(out, bodyID)",   // column-major 4x4 -> straight to a mat4
            out_desc::Mat44, out_desc::Pass, +[](const BodyInterface &bi, uintptr_t out, uint32 id) { RMat44 m = bi.GetWorldTransform(toBodyID(id)); WriteMat4(m, out); })
        .out_function("GetCenterOfMassTransform(out, bodyID)",
            out_desc::Mat44, out_desc::Pass, +[](const BodyInterface &bi, uintptr_t out, uint32 id) { RMat44 m = bi.GetCenterOfMassTransform(toBodyID(id)); WriteMat4(m, out); })
        .function("SetPosition(bodyID, position, activationMode)",
            +[](BodyInterface &bi, uint32 id, RVec3 p, EActivation a) { bi.SetPosition(toBodyID(id), p, a); })
        .function("SetRotation(bodyID, rotation, activationMode)",
            +[](BodyInterface &bi, uint32 id, Quat r, EActivation a) { bi.SetRotation(toBodyID(id), r, a); })
        .function("SetLinearVelocity(bodyID, linearVelocity)",
            +[](BodyInterface &bi, uint32 id, Vec3 v) { bi.SetLinearVelocity(toBodyID(id), v); })
        .function("SetAngularVelocity(bodyID, angularVelocity)",
            +[](BodyInterface &bi, uint32 id, Vec3 v) { bi.SetAngularVelocity(toBodyID(id), v); })
        .function("SetMotionType(bodyID, motionType, activationMode)",
            +[](BodyInterface &bi, uint32 id, EMotionType mt, EActivation a) { bi.SetMotionType(toBodyID(id), mt, a); })
        .function("AddForce(bodyID, force)",
            +[](BodyInterface &bi, uint32 id, Vec3 f) { bi.AddForce(toBodyID(id), f); })
        .function("AddForce(bodyID, force, point)",   // overload: force at a world point
            +[](BodyInterface &bi, uint32 id, Vec3 f, RVec3 p) { bi.AddForce(toBodyID(id), f, p); })
        .function("AddTorque(bodyID, torque)",
            +[](BodyInterface &bi, uint32 id, Vec3 t) { bi.AddTorque(toBodyID(id), t); })
        .function("AddForceAndTorque(bodyID, force, torque)",
            +[](BodyInterface &bi, uint32 id, Vec3 f, Vec3 t) { bi.AddForceAndTorque(toBodyID(id), f, t); })
        .function("AddImpulse(bodyID, impulse)",
            +[](BodyInterface &bi, uint32 id, Vec3 i) { bi.AddImpulse(toBodyID(id), i); })
        .function("AddImpulse(bodyID, impulse, point)",   // overload: impulse at a world point
            +[](BodyInterface &bi, uint32 id, Vec3 i, RVec3 p) { bi.AddImpulse(toBodyID(id), i, p); })
        .function("AddAngularImpulse(bodyID, angularImpulse)",
            +[](BodyInterface &bi, uint32 id, Vec3 i) { bi.AddAngularImpulse(toBodyID(id), i); })
        .function("AddLinearVelocity(bodyID, linearVelocity)",
            +[](BodyInterface &bi, uint32 id, Vec3 v) { bi.AddLinearVelocity(toBodyID(id), v); })
        .function("SetLinearAndAngularVelocity(bodyID, linearVelocity, angularVelocity)",
            +[](BodyInterface &bi, uint32 id, Vec3 lv, Vec3 av) { bi.SetLinearAndAngularVelocity(toBodyID(id), lv, av); })
        .function("SetPositionAndRotation(bodyID, position, rotation, activationMode)",
            +[](BodyInterface &bi, uint32 id, RVec3 p, Quat r, EActivation a) { bi.SetPositionAndRotation(toBodyID(id), p, r, a); })
        .function("SetPositionRotationAndVelocity(bodyID, position, rotation, linearVelocity, angularVelocity)",
            +[](BodyInterface &bi, uint32 id, RVec3 p, Quat r, Vec3 lv, Vec3 av) { bi.SetPositionRotationAndVelocity(toBodyID(id), p, r, lv, av); })
        .function("MoveKinematic(bodyID, targetPosition, targetRotation, deltaTime)",
            +[](BodyInterface &bi, uint32 id, RVec3 p, Quat r, float dt) { bi.MoveKinematic(toBodyID(id), p, r, dt); })
        .out_function("GetPointVelocity(out, bodyID, point)",
            out_desc::Vec3, out_desc::Pass, out_desc::Pass, +[](const BodyInterface &bi, uintptr_t out, uint32 id, RVec3 p) { WriteVec3(bi.GetPointVelocity(toBodyID(id), p), out); })
        // scalars / enums
        .function("IsAdded(bodyID)", +[](const BodyInterface &bi, uint32 id) { return bi.IsAdded(toBodyID(id)); })
        .function("GetBodyType(bodyID)", +[](const BodyInterface &bi, uint32 id) { return bi.GetBodyType(toBodyID(id)); })
        .function("GetMotionType(bodyID)", +[](const BodyInterface &bi, uint32 id) { return bi.GetMotionType(toBodyID(id)); })
        .function("GetMotionQuality(bodyID)", +[](const BodyInterface &bi, uint32 id) { return bi.GetMotionQuality(toBodyID(id)); })
        .function("GetObjectLayer(bodyID)", +[](const BodyInterface &bi, uint32 id) { return bi.GetObjectLayer(toBodyID(id)); })
        .function("GetFriction(bodyID)", +[](const BodyInterface &bi, uint32 id) { return bi.GetFriction(toBodyID(id)); })
        .function("GetRestitution(bodyID)", +[](const BodyInterface &bi, uint32 id) { return bi.GetRestitution(toBodyID(id)); })
        .function("GetGravityFactor(bodyID)", +[](const BodyInterface &bi, uint32 id) { return bi.GetGravityFactor(toBodyID(id)); })
        .function("GetUserData(bodyID)", +[](const BodyInterface &bi, uint32 id) { return bi.GetUserData(toBodyID(id)); })
        // setters
        .function("SetMotionQuality(bodyID, motionQuality)",
            +[](BodyInterface &bi, uint32 id, EMotionQuality q) { bi.SetMotionQuality(toBodyID(id), q); })
        .function("SetObjectLayer(bodyID, layer)",
            +[](BodyInterface &bi, uint32 id, ObjectLayer l) { bi.SetObjectLayer(toBodyID(id), l); })
        .function("SetFriction(bodyID, friction)",
            +[](BodyInterface &bi, uint32 id, float v) { bi.SetFriction(toBodyID(id), v); })
        .function("SetRestitution(bodyID, restitution)",
            +[](BodyInterface &bi, uint32 id, float v) { bi.SetRestitution(toBodyID(id), v); })
        .function("SetGravityFactor(bodyID, gravityFactor)",
            +[](BodyInterface &bi, uint32 id, float v) { bi.SetGravityFactor(toBodyID(id), v); })
        .function("SetUserData(bodyID, userData)",
            +[](BodyInterface &bi, uint32 id, uint64 v) { bi.SetUserData(toBodyID(id), v); })
        // shape (non-owning handle; body owns it)
        .function("GetShape(bodyID)",
            +[](const BodyInterface &bi, uint32 id) { return const_cast<Shape *>(bi.GetShape(toBodyID(id)).GetPtr()); }, allow_raw_pointers())
        .function("SetShape(bodyID, shape, updateMassProperties, activationMode)",
            +[](BodyInterface &bi, uint32 id, const Shape *s, bool m, EActivation a) { bi.SetShape(toBodyID(id), s, m, a); }, allow_raw_pointers())
        // Notify the broadphase that a body's shape was mutated in place (e.g. after
        // HeightFieldShape.SetHeights). Pass the body's current center-of-mass position.
        .function("NotifyShapeChanged(bodyID, prevCenterOfMass, updateMassProperties, activationMode)",
            +[](BodyInterface &bi, uint32 id, Vec3 prevCom, bool m, EActivation a) { bi.NotifyShapeChanged(toBodyID(id), prevCom, m, a); })
        // World-space transformed shape snapshot for a body (for CastRay picking etc).
        // Only valid while the body doesn't move — re-fetch after any movement.
        .function("GetTransformedShape(bodyID)",
            +[](const BodyInterface &bi, uint32 id) { return bi.GetTransformedShape(toBodyID(id)); })
        // creation variants
        .function("CreateSoftBodyWithID(bodyID, settings)",
            +[](BodyInterface &bi, uint32 id, const SoftBodyCreationSettings &s) { return bi.CreateSoftBodyWithID(toBodyID(id), s); }, allow_raw_pointers())
        .function("CreateBodyWithID(bodyID, settings)",
            +[](BodyInterface &bi, uint32 id, const BodyCreationSettings &s) { return bi.CreateBodyWithID(toBodyID(id), s); }, allow_raw_pointers())
        .function("CreateBodyWithoutID(settings)", &BodyInterface::CreateBodyWithoutID, allow_raw_pointers())
        .function("CreateSoftBodyWithoutID(settings)", &BodyInterface::CreateSoftBodyWithoutID, allow_raw_pointers())
        .function("DestroyBodyWithoutID(body)", &BodyInterface::DestroyBodyWithoutID, allow_raw_pointers())
        .function("CreateAndAddSoftBody(settings, activationMode)",
            +[](BodyInterface &bi, const SoftBodyCreationSettings &s, EActivation a) { return fromBodyID(bi.CreateAndAddSoftBody(s, a)); })
        .function("AssignBodyID(body)", +[](BodyInterface &bi, Body *b) { return bi.AssignBodyID(b); }, allow_raw_pointers())
        .function("AssignBodyIDWithID(body, bodyID)", +[](BodyInterface &bi, Body *b, uint32 id) { return bi.AssignBodyID(b, toBodyID(id)); }, allow_raw_pointers())
        .function("UnassignBodyID(bodyID)", +[](BodyInterface &bi, uint32 id) { return bi.UnassignBodyID(toBodyID(id)); }, allow_raw_pointers())
        // batch id-array APIs — raw heap pointer to a BodyID[] (Uint32Array on the JS side)
        .function("DestroyBodies(bodyIDsPtr, number)",
            +[](BodyInterface &bi, uintptr_t bodyIDs, int number) { bi.DestroyBodies(reinterpret_cast<const BodyID *>(bodyIDs), number); })
        .function("ActivateBodies(bodyIDsPtr, number)",
            +[](BodyInterface &bi, uintptr_t bodyIDs, int number) { bi.ActivateBodies(reinterpret_cast<const BodyID *>(bodyIDs), number); })
        .function("DeactivateBodies(bodyIDsPtr, number)",
            +[](BodyInterface &bi, uintptr_t bodyIDs, int number) { bi.DeactivateBodies(reinterpret_cast<const BodyID *>(bodyIDs), number); })
        .function("RemoveBodies(bodyIDsPtr, number)",
            +[](BodyInterface &bi, uintptr_t bodyIDs, int number) { bi.RemoveBodies(reinterpret_cast<BodyID *>(bodyIDs), number); })
        .function("AddBodiesPrepare(bodyIDsPtr, number)",
            +[](BodyInterface &bi, uintptr_t bodyIDs, int number) { return reinterpret_cast<uintptr_t>(bi.AddBodiesPrepare(reinterpret_cast<BodyID *>(bodyIDs), number)); })
        .function("AddBodiesFinalize(bodyIDsPtr, number, addState, activationMode)",
            +[](BodyInterface &bi, uintptr_t bodyIDs, int number, uintptr_t addState, EActivation a) { bi.AddBodiesFinalize(reinterpret_cast<BodyID *>(bodyIDs), number, reinterpret_cast<BodyInterface::AddState>(addState), a); })
        .function("AddBodiesAbort(bodyIDsPtr, number, addState)",
            +[](BodyInterface &bi, uintptr_t bodyIDs, int number, uintptr_t addState) { bi.AddBodiesAbort(reinterpret_cast<BodyID *>(bodyIDs), number, reinterpret_cast<BodyInterface::AddState>(addState)); })
        // constraints
        .function("CreateConstraint(settings, bodyID1, bodyID2)",
            +[](BodyInterface &bi, const TwoBodyConstraintSettings *s, uint32 id1, uint32 id2) { return bi.CreateConstraint(s, toBodyID(id1), toBodyID(id2)); }, allow_raw_pointers())
        .function("ActivateConstraint(constraint)",
            +[](BodyInterface &bi, const TwoBodyConstraint *c) { bi.ActivateConstraint(c); }, allow_raw_pointers())
        // position/rotation
        .function("SetPositionAndRotationWhenChanged(bodyID, position, rotation, activationMode)",
            +[](BodyInterface &bi, uint32 id, RVec3 p, Quat r, EActivation a) { bi.SetPositionAndRotationWhenChanged(toBodyID(id), p, r, a); })
        .out_function("GetPositionAndRotation(outPos, outRot, bodyID)",
            out_desc::Vec3, out_desc::Quat, out_desc::Pass,
            +[](const BodyInterface &bi, uintptr_t outPos, uintptr_t outRot, uint32 id) { RVec3 p; Quat r; bi.GetPositionAndRotation(toBodyID(id), p, r); WriteVec3(p, outPos); WriteQuat(r, outRot); })
        .out_function("GetLinearAndAngularVelocity(outLin, outAng, bodyID)",
            out_desc::Vec3, out_desc::Vec3, out_desc::Pass,
            +[](const BodyInterface &bi, uintptr_t outLin, uintptr_t outAng, uint32 id) { Vec3 lv, av; bi.GetLinearAndAngularVelocity(toBodyID(id), lv, av); WriteVec3(lv, outLin); WriteVec3(av, outAng); })
        .function("AddLinearAndAngularVelocity(bodyID, linearVelocity, angularVelocity)",
            +[](BodyInterface &bi, uint32 id, Vec3 lv, Vec3 av) { bi.AddLinearAndAngularVelocity(toBodyID(id), lv, av); })
        .function("ResetSleepTimer(bodyID)", +[](BodyInterface &bi, uint32 id) { bi.ResetSleepTimer(toBodyID(id)); })
        .function("SetMaxLinearVelocity(bodyID, linearVelocity)",
            +[](BodyInterface &bi, uint32 id, float v) { bi.SetMaxLinearVelocity(toBodyID(id), v); })
        .function("GetMaxLinearVelocity(bodyID)", +[](const BodyInterface &bi, uint32 id) { return bi.GetMaxLinearVelocity(toBodyID(id)); })
        .function("SetMaxAngularVelocity(bodyID, angularVelocity)",
            +[](BodyInterface &bi, uint32 id, float v) { bi.SetMaxAngularVelocity(toBodyID(id), v); })
        .function("GetMaxAngularVelocity(bodyID)", +[](const BodyInterface &bi, uint32 id) { return bi.GetMaxAngularVelocity(toBodyID(id)); })
        .function("SetUseManifoldReduction(bodyID, useReduction)",
            +[](BodyInterface &bi, uint32 id, bool v) { bi.SetUseManifoldReduction(toBodyID(id), v); })
        .function("GetUseManifoldReduction(bodyID)", +[](const BodyInterface &bi, uint32 id) { return bi.GetUseManifoldReduction(toBodyID(id)); })
        .function("SetIsSensor(bodyID, isSensor)",
            +[](BodyInterface &bi, uint32 id, bool v) { bi.SetIsSensor(toBodyID(id), v); })
        .function("IsSensor(bodyID)", +[](const BodyInterface &bi, uint32 id) { return bi.IsSensor(toBodyID(id)); })
        .function("SetCollisionGroup(bodyID, collisionGroup)",
            +[](BodyInterface &bi, uint32 id, const CollisionGroup &g) { bi.SetCollisionGroup(toBodyID(id), g); })
        .function("GetCollisionGroup(bodyID)",
            +[](const BodyInterface &bi, uint32 id) { return const_cast<CollisionGroup *>(&bi.GetCollisionGroup(toBodyID(id))); }, allow_raw_pointers())
        .out_function("GetInverseInertia(out, bodyID)",
            out_desc::Mat44, out_desc::Pass, +[](const BodyInterface &bi, uintptr_t out, uint32 id) { Mat44 m = bi.GetInverseInertia(toBodyID(id)); WriteMat4(m, out); })
        .function("GetMaterial(bodyID, subShapeID)",
            +[](const BodyInterface &bi, uint32 id, uint32 subShapeID) { return const_cast<PhysicsMaterial *>(bi.GetMaterial(toBodyID(id), toSubShapeID(subShapeID))); }, allow_raw_pointers())
        .function("ApplyBuoyancyImpulse(bodyID, surfacePosition, surfaceNormal, buoyancy, linearDrag, angularDrag, fluidVelocity, gravity, deltaTime)",
            +[](BodyInterface &bi, uint32 id, RVec3 sp, Vec3 sn, float buoyancy, float linearDrag, float angularDrag, Vec3 fluidVelocity, Vec3 gravity, float dt) {
                return bi.ApplyBuoyancyImpulse(toBodyID(id), sp, sn, buoyancy, linearDrag, angularDrag, fluidVelocity, gravity, dt); })
        .function("InvalidateContactCache(bodyID)", +[](BodyInterface &bi, uint32 id) { bi.InvalidateContactCache(toBodyID(id)); });

    // ---- Body (non-owning; handed to callbacks) ----
    jolt_class_<Body>("Body")
        .function("GetID", +[](const Body &b) { return fromBodyID(b.GetID()); })
        // value-type reads (out-param)
        .out_function("GetPosition(out)", out_desc::Vec3, +[](const Body &s, uintptr_t out) { WriteVec3(s.GetPosition(), out); })
        .out_function("GetRotation(out)", out_desc::Quat, +[](const Body &s, uintptr_t out) { WriteQuat(s.GetRotation(), out); })
        .out_function("GetCenterOfMassPosition(out)", out_desc::Vec3, +[](const Body &s, uintptr_t out) { WriteVec3(s.GetCenterOfMassPosition(), out); })
        .out_function("GetWorldTransform(out)", out_desc::Mat44, +[](const Body &s, uintptr_t out) { auto m = s.GetWorldTransform(); WriteMat4(m, out); })
        .out_function("GetCenterOfMassTransform(out)", out_desc::Mat44, +[](const Body &s, uintptr_t out) { auto m = s.GetCenterOfMassTransform(); WriteMat4(m, out); })
        .out_function("GetLinearVelocity(out)", out_desc::Vec3, +[](const Body &s, uintptr_t out) { WriteVec3(s.GetLinearVelocity(), out); })
        .out_function("GetAngularVelocity(out)", out_desc::Vec3, +[](const Body &s, uintptr_t out) { WriteVec3(s.GetAngularVelocity(), out); })
        .out_function("GetPointVelocity(out, point)",
            out_desc::Vec3, out_desc::Pass, +[](const Body &b, uintptr_t out, RVec3 p) { WriteVec3(b.GetPointVelocity(p), out); })
        .out_function("GetWorldSpaceBounds(out)",   // AABox -> [minX,minY,minZ,maxX,maxY,maxZ]
            out_desc::AABox, +[](const Body &b, uintptr_t out) { WriteAABox(b.GetWorldSpaceBounds(), out); })
        // rigid-body buoyancy: apply the impulse from a fluid whose surface is the plane
        // (surfacePosition, surfaceNormal). Call each step while the body is submerged.
        .function("ApplyBuoyancyImpulse(surfacePosition, surfaceNormal, buoyancy, linearDrag, angularDrag, fluidVelocity, gravity, deltaTime)",
            +[](Body &b, RVec3 sp, Vec3 sn, float buoyancy, float linearDrag, float angularDrag, Vec3 fluidVelocity, Vec3 gravity, float dt) {
                return b.ApplyBuoyancyImpulse(sp, sn, buoyancy, linearDrag, angularDrag, fluidVelocity, gravity, dt); })
        // scalars / flags
        .function("GetBodyType", &Body::GetBodyType)
        .function("IsRigidBody", &Body::IsRigidBody)
        .function("IsSoftBody", &Body::IsSoftBody)
        .function("IsActive", &Body::IsActive)
        .function("IsStatic", &Body::IsStatic)
        .function("IsKinematic", &Body::IsKinematic)
        .function("IsDynamic", &Body::IsDynamic)
        .function("IsSensor", &Body::IsSensor)
        .function("GetMotionType", &Body::GetMotionType)
        .function("GetObjectLayer", &Body::GetObjectLayer)
        .function("GetFriction", &Body::GetFriction)
        .function("GetRestitution", &Body::GetRestitution)
        .function("GetUserData", &Body::GetUserData)
        // shape + motion properties (non-owning handles)
        .function("GetShape", +[](const Body &b) { return const_cast<Shape *>(b.GetShape()); }, allow_raw_pointers())
        .function("GetMotionProperties", +[](Body &b) { return b.GetMotionProperties(); }, allow_raw_pointers())
        // Soft bodies: MotionProperties isn't polymorphic in embind, so downcast to the concrete
        // SoftBodyMotionProperties (only valid when IsSoftBody()).
        .function("GetSoftBodyMotionProperties", +[](Body &b) {
            return static_cast<SoftBodyMotionProperties *>(b.GetMotionPropertiesUnchecked()); }, allow_raw_pointers())
        // Soft body: current (deformed) vertex positions as a flat Float32Array (3 per
        // vertex), relative to the body's center of mass. Pair with the static face
        // indices from SoftBodySharedSettings.GetFaceIndices() to build a render mesh.
        .function("GetSoftBodyVertices: Float32Array", +[](const Body &b) -> val {
            auto *mp = static_cast<const SoftBodyMotionProperties *>(b.GetMotionPropertiesUnchecked());
            const auto &verts = mp->GetVertices();
            std::vector<float> out; out.reserve(verts.size() * 3);
            for (const auto &v : verts) { Vec3 p = v.mPosition; out.push_back(p.GetX()); out.push_back(p.GetY()); out.push_back(p.GetZ()); }
            val a = val::global("Float32Array").new_(out.size());
            a.call<void>("set", val(typed_memory_view(out.size(), out.data())));
            return a;
        })
        // setters
        .function("SetFriction(friction)", &Body::SetFriction)
        .function("SetRestitution(restitution)", &Body::SetRestitution)
        .function("SetLinearVelocity(linearVelocity)", &Body::SetLinearVelocity)
        .function("SetAngularVelocity(angularVelocity)", &Body::SetAngularVelocity)
        .function("SetUserData(userData)", &Body::SetUserData)
        .function("SetMotionType(motionType)", &Body::SetMotionType)
        .function("SetIsSensor(isSensor)", &Body::SetIsSensor)
        .function("GetAllowSleeping", &Body::GetAllowSleeping)
        .function("SetAllowSleeping(allowSleeping)", &Body::SetAllowSleeping)
        // forces/impulses directly on a Body* (e.g. one from BodyLockInterface.TryGetBody).
        // The (…, point) overloads apply at a world-space point.
        .function("AddForce(force)", +[](Body &b, Vec3 f) { b.AddForce(f); })
        .function("AddForce(force, point)", +[](Body &b, Vec3 f, RVec3 p) { b.AddForce(f, p); })
        .function("AddTorque(torque)", +[](Body &b, Vec3 t) { b.AddTorque(t); })
        .function("AddImpulse(impulse)", +[](Body &b, Vec3 i) { b.AddImpulse(i); })
        .function("AddImpulse(impulse, point)", +[](Body &b, Vec3 i, RVec3 p) { b.AddImpulse(i, p); })
        // flags
        .function("CanBeKinematicOrDynamic", &Body::CanBeKinematicOrDynamic)
        .function("IsInBroadPhase", &Body::IsInBroadPhase)
        .function("SetCollideKinematicVsNonDynamic(collide)", &Body::SetCollideKinematicVsNonDynamic)
        .function("GetCollideKinematicVsNonDynamic", &Body::GetCollideKinematicVsNonDynamic)
        .function("SetUseManifoldReduction(useReduction)", &Body::SetUseManifoldReduction)
        .function("GetUseManifoldReduction", &Body::GetUseManifoldReduction)
        .function("SetApplyGyroscopicForce(apply)", &Body::SetApplyGyroscopicForce)
        .function("GetApplyGyroscopicForce", &Body::GetApplyGyroscopicForce)
        .function("SetEnhancedInternalEdgeRemoval(apply)", &Body::SetEnhancedInternalEdgeRemoval)
        .function("GetEnhancedInternalEdgeRemoval", &Body::GetEnhancedInternalEdgeRemoval)
        // collision group — non-const overload returns a mutable, non-owning handle
        .function("GetCollisionGroup", +[](Body &b) { return &b.GetCollisionGroup(); }, allow_raw_pointers())
        .function("ResetSleepTimer", &Body::ResetSleepTimer)
        .function("SetLinearVelocityClamped(linearVelocity)", &Body::SetLinearVelocityClamped)
        .function("SetAngularVelocityClamped(angularVelocity)", &Body::SetAngularVelocityClamped)
        .out_function("GetAccumulatedForce(out)", out_desc::Vec3, +[](const Body &b, uintptr_t out) { WriteVec3(b.GetAccumulatedForce(), out); })
        .out_function("GetAccumulatedTorque(out)", out_desc::Vec3, +[](const Body &b, uintptr_t out) { WriteVec3(b.GetAccumulatedTorque(), out); })
        .function("ResetForce", &Body::ResetForce)
        .function("ResetTorque", &Body::ResetTorque)
        .function("ResetMotion", &Body::ResetMotion)
        .function("AddAngularImpulse(angularImpulse)", +[](Body &b, Vec3 i) { b.AddAngularImpulse(i); })
        .function("MoveKinematic(targetPosition, targetRotation, deltaTime)",
            +[](Body &b, RVec3 p, Quat r, float dt) { b.MoveKinematic(p, r, dt); })
        .out_function("GetInverseInertia(out)", out_desc::Mat44, +[](const Body &b, uintptr_t out) { Mat44 m = b.GetInverseInertia(); WriteMat4(m, out); })
        .out_function("GetInverseCenterOfMassTransform(out)", out_desc::Mat44, +[](const Body &b, uintptr_t out) { RMat44 m = b.GetInverseCenterOfMassTransform(); WriteMat4(m, out); })
        .out_function("GetWorldSpaceSurfaceNormal(out, subShapeID, position)",
            out_desc::Vec3, out_desc::Pass, out_desc::Pass,
            +[](const Body &b, uintptr_t out, uint32 subShapeID, RVec3 pos) {
                WriteVec3(b.GetWorldSpaceSurfaceNormal(toSubShapeID(subShapeID), pos), out); })
        .function("GetTransformedShape", +[](const Body &b) { return b.GetTransformedShape(); })
        .function("GetBodyCreationSettings", +[](const Body &b) { return b.GetBodyCreationSettings(); })
        .function("GetSoftBodyCreationSettings", +[](const Body &b) { return b.GetSoftBodyCreationSettings(); })
        .function("SaveState(stream)", +[](const Body &b, StateRecorder &s) { b.SaveState(s); }, allow_raw_pointers())
        .function("RestoreState(stream)", +[](Body &b, StateRecorder &s) { b.RestoreState(s); }, allow_raw_pointers());

    // ---- MotionProperties (non-owning; obtained from Body) ----
    jolt_class_<MotionProperties>("MotionProperties")
        .out_function("GetLinearVelocity(out)", out_desc::Vec3, +[](const MotionProperties &s, uintptr_t out) { WriteVec3(s.GetLinearVelocity(), out); })
        .out_function("GetAngularVelocity(out)", out_desc::Vec3, +[](const MotionProperties &s, uintptr_t out) { WriteVec3(s.GetAngularVelocity(), out); })
        .function("GetMotionQuality", &MotionProperties::GetMotionQuality)
        .function("GetAllowedDOFs", &MotionProperties::GetAllowedDOFs)
        .function("GetMaxLinearVelocity", &MotionProperties::GetMaxLinearVelocity)
        .function("GetMaxAngularVelocity", &MotionProperties::GetMaxAngularVelocity)
        .function("GetLinearDamping", &MotionProperties::GetLinearDamping)
        .function("GetAngularDamping", &MotionProperties::GetAngularDamping)
        .function("GetGravityFactor", &MotionProperties::GetGravityFactor)
        .function("GetInverseMass", &MotionProperties::GetInverseMass)
        .function("GetInverseMassUnchecked", &MotionProperties::GetInverseMassUnchecked)
        .function("SetLinearVelocity(linearVelocity)", &MotionProperties::SetLinearVelocity)
        .function("SetAngularVelocity(angularVelocity)", &MotionProperties::SetAngularVelocity)
        .function("SetMaxLinearVelocity(maxLinearVelocity)", &MotionProperties::SetMaxLinearVelocity)
        .function("SetMaxAngularVelocity(maxAngularVelocity)", &MotionProperties::SetMaxAngularVelocity)
        .function("SetLinearDamping(linearDamping)", &MotionProperties::SetLinearDamping)
        .function("SetAngularDamping(angularDamping)", &MotionProperties::SetAngularDamping)
        .function("SetGravityFactor(gravityFactor)", &MotionProperties::SetGravityFactor)
        .function("SetInverseMass(inverseMass)", &MotionProperties::SetInverseMass)
        .function("GetAllowSleeping", &MotionProperties::GetAllowSleeping)
        .function("SetLinearVelocityClamped(linearVelocity)", &MotionProperties::SetLinearVelocityClamped)
        .function("SetAngularVelocityClamped(angularVelocity)", &MotionProperties::SetAngularVelocityClamped)
        .function("MoveKinematic(deltaPosition, deltaRotation, deltaTime)",
            +[](MotionProperties &mp, Vec3 dp, Quat dr, float dt) { mp.MoveKinematic(dp, dr, dt); })
        .function("ClampLinearVelocity", &MotionProperties::ClampLinearVelocity)
        .function("ClampAngularVelocity", &MotionProperties::ClampAngularVelocity)
        .function("SetMassProperties(allowedDOFs, massProperties)",
            +[](MotionProperties &mp, EAllowedDOFs dofs, const MassProperties &m) { mp.SetMassProperties(dofs, m); })
        .function("ScaleToMass(mass)", &MotionProperties::ScaleToMass)
        .out_function("GetInverseInertiaDiagonal(out)", out_desc::Vec3, +[](const MotionProperties &mp, uintptr_t out) { WriteVec3(mp.GetInverseInertiaDiagonal(), out); })
        .out_function("GetInertiaRotation(out)", out_desc::Quat, +[](const MotionProperties &mp, uintptr_t out) { WriteQuat(mp.GetInertiaRotation(), out); })
        .function("SetInverseInertia(diagonal, rotation)",
            +[](MotionProperties &mp, Vec3 d, Quat r) { mp.SetInverseInertia(d, r); })
        .out_function("GetLocalSpaceInverseInertia(out)", out_desc::Mat44, +[](const MotionProperties &mp, uintptr_t out) { Mat44 m = mp.GetLocalSpaceInverseInertia(); WriteMat4(m, out); })
        .out_function("GetInverseInertiaForRotation(out, rotation)",
            out_desc::Mat44, out_desc::Pass,
            +[](const MotionProperties &mp, uintptr_t out, Mat44 rot) { Mat44 m = mp.GetInverseInertiaForRotation(rot); WriteMat4(m, out); })
        .out_function("MultiplyWorldSpaceInverseInertiaByVector(out, rotation, v)",
            out_desc::Vec3, out_desc::Pass, out_desc::Pass,
            +[](const MotionProperties &mp, uintptr_t out, Quat r, Vec3 v) { WriteVec3(mp.MultiplyWorldSpaceInverseInertiaByVector(r, v), out); })
        .out_function("GetPointVelocityCOM(out, pointRelativeToCOM)",
            out_desc::Vec3, out_desc::Pass, +[](const MotionProperties &mp, uintptr_t out, Vec3 p) { WriteVec3(mp.GetPointVelocityCOM(p), out); })
        .out_function("GetAccumulatedForce(out)", out_desc::Vec3, +[](const MotionProperties &mp, uintptr_t out) { WriteVec3(mp.GetAccumulatedForce(), out); })
        .out_function("GetAccumulatedTorque(out)", out_desc::Vec3, +[](const MotionProperties &mp, uintptr_t out) { WriteVec3(mp.GetAccumulatedTorque(), out); })
        .function("ResetForce", &MotionProperties::ResetForce)
        .function("ResetTorque", &MotionProperties::ResetTorque)
        .function("ResetMotion", &MotionProperties::ResetMotion)
        .out_function("LockTranslation(out, v)", out_desc::Vec3, out_desc::Pass, +[](const MotionProperties &mp, uintptr_t out, Vec3 v) { WriteVec3(mp.LockTranslation(v), out); })
        .out_function("LockAngular(out, v)", out_desc::Vec3, out_desc::Pass, +[](const MotionProperties &mp, uintptr_t out, Vec3 v) { WriteVec3(mp.LockAngular(v), out); })
        .function("SetNumVelocityStepsOverride(n)", &MotionProperties::SetNumVelocityStepsOverride)
        .function("GetNumVelocityStepsOverride", &MotionProperties::GetNumVelocityStepsOverride)
        .function("SetNumPositionStepsOverride(n)", &MotionProperties::SetNumPositionStepsOverride)
        .function("GetNumPositionStepsOverride", &MotionProperties::GetNumPositionStepsOverride);

    // ---- contact types + JS-subclassable ContactListener (callback convention) ----
    jolt_class_<ContactManifold>("ContactManifold")
        .property("mBaseOffset", &ContactManifold::mBaseOffset)
        .property("mWorldSpaceNormal", &ContactManifold::mWorldSpaceNormal)
        .property("mPenetrationDepth", &ContactManifold::mPenetrationDepth)
        .function("GetPointCount", +[](const ContactManifold &m) { return (uint32)m.mRelativeContactPointsOn1.size(); })
        .function("GetSubShapeID1", +[](const ContactManifold &m) { return m.mSubShapeID1.GetValue(); })
        .function("GetSubShapeID2", +[](const ContactManifold &m) { return m.mSubShapeID2.GetValue(); })
        .out_function("GetWorldSpaceContactPointOn1(out, index)",
            out_desc::Vec3, out_desc::Pass, +[](const ContactManifold &m, uintptr_t out, uint i) { RVec3 p = m.GetWorldSpaceContactPointOn1(i);
               WriteVec3(p, out); })
        .out_function("GetWorldSpaceContactPointOn2(out, index)",
            out_desc::Vec3, out_desc::Pass, +[](const ContactManifold &m, uintptr_t out, uint i) { RVec3 p = m.GetWorldSpaceContactPointOn2(i);
               WriteVec3(p, out); });
    jolt_class_<ContactSettings>("ContactSettings")
        .constructor<>()
        .property("mCombinedFriction", &ContactSettings::mCombinedFriction)
        .property("mCombinedRestitution", &ContactSettings::mCombinedRestitution)
        .property("mIsSensor", &ContactSettings::mIsSensor)
        .property("mRelativeLinearSurfaceVelocity", &ContactSettings::mRelativeLinearSurfaceVelocity)
        .property("mRelativeAngularSurfaceVelocity", &ContactSettings::mRelativeAngularSurfaceVelocity)
        .property("mInvMassScale1", &ContactSettings::mInvMassScale1)
        .property("mInvInertiaScale1", &ContactSettings::mInvInertiaScale1)
        .property("mInvMassScale2", &ContactSettings::mInvMassScale2)
        .property("mInvInertiaScale2", &ContactSettings::mInvInertiaScale2);
    jolt_class_<ContactListener>("ContactListener")
        .allow_subclass<ContactListenerWrapper>("ContactListenerWrapper");
    // Cross-thread contact listener for the multi-threaded build (see ThreadedContactListener
    // above). Dispatches to thread-local globals; the worker reads bodies + writes contact
    // settings through these pointer helpers (raw pointers arrive as numbers).
    jolt_class_<ThreadedContactListener, base<ContactListener>>("ThreadedContactListener")
        .constructor<>();
    emscripten::function("getContactBodyID", +[](uintptr_t p) { return (uint32)((const Body *)p)->GetID().GetIndexAndSequenceNumber(); }, allow_raw_pointers());
    emscripten::function("getContactBodyCOM", +[](uintptr_t p) { return Vec3(((const Body *)p)->GetCenterOfMassPosition()); }, allow_raw_pointers());
    emscripten::function("rotateVectorByBody", +[](uintptr_t p, Vec3 v) { return Vec3(((const Body *)p)->GetRotation() * v); }, allow_raw_pointers());
    emscripten::function("setContactRelativeLinearSurfaceVelocity", +[](uintptr_t p, Vec3 v) { ((ContactSettings *)p)->mRelativeLinearSurfaceVelocity = v; }, allow_raw_pointers());
    emscripten::function("setContactRelativeAngularSurfaceVelocity", +[](uintptr_t p, Vec3 v) { ((ContactSettings *)p)->mRelativeAngularSurfaceVelocity = v; }, allow_raw_pointers());
    // body activation listener (bodies waking / sleeping)
    jolt_class_<BodyActivationListener>("BodyActivationListener")
        .allow_subclass<BodyActivationListenerWrapper>("BodyActivationListenerWrapper");
    // buffering contact listener — install via SetContactListener; read via post.js
    jolt_class_<ContactListenerBuffer, base<ContactListener>>("ContactListenerBuffer")
        .constructor<>()
        .function("Clear",            &ContactListenerBuffer::Clear)
        .function("GetAddedCount",    &ContactListenerBuffer::GetAddedCount)
        .function("GetPersistedCount",&ContactListenerBuffer::GetPersistedCount)
        .function("GetRemovedCount",  &ContactListenerBuffer::GetRemovedCount)
        .function("AddedI32Ptr",      &ContactListenerBuffer::AddedI32Ptr)
        .function("AddedF32Ptr",      &ContactListenerBuffer::AddedF32Ptr)
        .function("PersistedI32Ptr",  &ContactListenerBuffer::PersistedI32Ptr)
        .function("PersistedF32Ptr",  &ContactListenerBuffer::PersistedF32Ptr)
        .function("PointsF32Ptr",     &ContactListenerBuffer::PointsF32Ptr)
        .function("RemovedI32Ptr",    &ContactListenerBuffer::RemovedI32Ptr);

    // post-step active body snapshot — call Refresh() after Step(); read via post.js
    jolt_class_<ActiveBodyBuffer>("ActiveBodyBuffer")
        .constructor<>()
        .function("Refresh(physicsSystem)", &ActiveBodyBuffer::Refresh, allow_raw_pointers())
        .function("GetBodyCount",  &ActiveBodyBuffer::GetBodyCount)
        .function("BodiesF32Ptr",  &ActiveBodyBuffer::BodiesF32Ptr);

    // ---- container exemplar ----
    registerJoltArray<Vec3>("ArrayVec3");

    // Body-by-ID lookup. TryGetBody returns a Body* (nullptr if the ID is stale). The NoLock
    // variant is fine single-threaded (current usage); in the MT build a raw Body* read off the
    // step is unsafe against concurrent mutation — that needs a BodyLockRead/Write scope (unbound).
    jolt_class_<BodyLockInterface>("BodyLockInterface")
        .function("TryGetBody(bodyID)",
            +[](const BodyLockInterface &li, uint32 id) { return li.TryGetBody(toBodyID(id)); }, allow_raw_pointers());

    // ---- PhysicsSystem (reference accessors via lambdas) ----
    jolt_class_<PhysicsSystem>("PhysicsSystem")
        .function("GetBodyInterface",
            +[](PhysicsSystem &ps) { return &ps.GetBodyInterface(); }, allow_raw_pointers())
        .function("GetBodyLockInterface",
            +[](PhysicsSystem &ps) { return const_cast<BodyLockInterface *>(static_cast<const BodyLockInterface *>(&ps.GetBodyLockInterface())); }, allow_raw_pointers())
        .function("GetBodyLockInterfaceNoLock",
            +[](PhysicsSystem &ps) { return const_cast<BodyLockInterface *>(static_cast<const BodyLockInterface *>(&ps.GetBodyLockInterfaceNoLock())); }, allow_raw_pointers())
        .function("SetContactListener(listener)", &PhysicsSystem::SetContactListener, allow_raw_pointers())
        .function("SetBodyActivationListener(listener)", &PhysicsSystem::SetBodyActivationListener, allow_raw_pointers())
        .function("AddConstraint(constraint)", &PhysicsSystem::AddConstraint, allow_raw_pointers())
        .function("RemoveConstraint(constraint)", &PhysicsSystem::RemoveConstraint, allow_raw_pointers())
        .function("SetGravity(gravity)", &PhysicsSystem::SetGravity)
        .out_function("GetGravity(out)", out_desc::Vec3, +[](const PhysicsSystem &s, uintptr_t out) { WriteVec3(s.GetGravity(), out); })
        .function("OptimizeBroadPhase", &PhysicsSystem::OptimizeBroadPhase)
        .function("GetNumBodies", &PhysicsSystem::GetNumBodies)
        .function("GetNarrowPhaseQuery",   // const& -> non-const handle (query methods are const)
            +[](PhysicsSystem &ps) { return const_cast<NarrowPhaseQuery *>(&ps.GetNarrowPhaseQuery()); }, allow_raw_pointers())
        .function("GetBroadPhaseQuery",
            +[](PhysicsSystem &ps) { return const_cast<BroadPhaseQuery *>(&ps.GetBroadPhaseQuery()); }, allow_raw_pointers())
        // world-space surface normal at a raycast hit (locks the body internally).
        .out_function("GetRayHitNormal(out, ray, hit)",
            out_desc::Vec3, out_desc::Pass, out_desc::Pass, +[](PhysicsSystem &ps, uintptr_t out, const RRayCast &ray, const RayCastResult &hit) {
                Vec3 n = Vec3::sZero();
                BodyLockRead lock(ps.GetBodyLockInterfaceNoLock(), hit.mBodyID);
                if (lock.Succeeded())
                    n = lock.GetBody().GetWorldSpaceSurfaceNormal(hit.mSubShapeID2, ray.GetPointOnRay(hit.mFraction));
                WriteVec3(n, out);
            })
        .function("GetNumActiveBodies",
            +[](const PhysicsSystem &ps) { return ps.GetNumActiveBodies(EBodyType::RigidBody); })
        .function("GetMaxBodies", &PhysicsSystem::GetMaxBodies)
        // serialize / restore the full simulation state (rewind the recorder before restore).
        .function("SaveState(stateRecorder)", +[](const PhysicsSystem &ps, StateRecorder &sr) { ps.SaveState(sr); }, allow_raw_pointers())
        .function("RestoreState(stateRecorder)", +[](PhysicsSystem &ps, StateRecorder &sr) { return ps.RestoreState(sr); }, allow_raw_pointers())
        // live tunables — mutate the returned settings in place (non-owning handle).
        .function("GetPhysicsSettings",
            +[](PhysicsSystem &ps) { return const_cast<PhysicsSettings *>(&ps.GetPhysicsSettings()); }, allow_raw_pointers())
        .function("SetPhysicsSettings(settings)", &PhysicsSystem::SetPhysicsSettings)
        // body enumeration -> plain JS array of numeric ids (variable length; not per-frame hot).
        .function("GetBodies: number[]",
            +[](const PhysicsSystem &ps) {
                BodyIDVector ids; ps.GetBodies(ids);
                val arr = val::array();
                for (size_t i = 0; i < ids.size(); i++) arr.set(i, (uint32)ids[i].GetIndexAndSequenceNumber());
                return arr; })
        .function("GetActiveBodies: number[]",
            +[](const PhysicsSystem &ps) {
                BodyIDVector ids; ps.GetActiveBodies(EBodyType::RigidBody, ids);
                val arr = val::array();
                for (size_t i = 0; i < ids.size(); i++) arr.set(i, (uint32)ids[i].GetIndexAndSequenceNumber());
                return arr; })
        // pre-step hooks (subclass PhysicsStepListener in JS)
        .function("AddStepListener(listener)", &PhysicsSystem::AddStepListener, allow_raw_pointers())
        .function("RemoveStepListener(listener)", &PhysicsSystem::RemoveStepListener, allow_raw_pointers())
        // world-space bounds of everything in the broad phase.
        .out_function("GetBounds(out)", out_desc::AABox,
            +[](const PhysicsSystem &ps, uintptr_t out) { WriteAABox(ps.GetBounds(), out); })
        .function("WereBodiesInContact(bodyID1, bodyID2)",
            +[](const PhysicsSystem &ps, uint32 a, uint32 b) { return ps.WereBodiesInContact(toBodyID(a), toBodyID(b)); })
        // listener getters (non-owning; may be null).
        .function("GetContactListener",
            +[](PhysicsSystem &ps) { return ps.GetContactListener(); }, allow_raw_pointers())
        .function("GetBodyActivationListener",
            +[](PhysicsSystem &ps) { return ps.GetBodyActivationListener(); }, allow_raw_pointers())
        // no-lock query / interface accessors (const& -> non-const handle; query methods are const).
        .function("GetNarrowPhaseQueryNoLock",
            +[](PhysicsSystem &ps) { return const_cast<NarrowPhaseQuery *>(&ps.GetNarrowPhaseQueryNoLock()); }, allow_raw_pointers())
        .function("GetBodyInterfaceNoLock",
            +[](PhysicsSystem &ps) { return &ps.GetBodyInterfaceNoLock(); }, allow_raw_pointers())
        // custom simulation shape filter (JS-subclass SimShapeFilter).
        .function("SetSimShapeFilter(filter)",
            +[](PhysicsSystem &ps, const SimShapeFilter *f) { ps.SetSimShapeFilter(f); }, allow_raw_pointers())
        .function("GetSimShapeFilter",
            +[](PhysicsSystem &ps) { return const_cast<SimShapeFilter *>(ps.GetSimShapeFilter()); }, allow_raw_pointers())
        // SaveState/RestoreState with a state mask + optional StateRecorderFilter.
        .function("SaveStateWithFilter(stateRecorder, state, filter)",
            +[](const PhysicsSystem &ps, StateRecorder &sr, EStateRecorderState state, StateRecorderFilter *filter) {
                ps.SaveState(sr, state, filter); }, allow_raw_pointers())
        .function("RestoreStateWithFilter(stateRecorder, filter)",
            +[](PhysicsSystem &ps, StateRecorder &sr, StateRecorderFilter *filter) {
                return ps.RestoreState(sr, filter); }, allow_raw_pointers())
        .function("SetSoftBodyContactListener(listener)", &PhysicsSystem::SetSoftBodyContactListener, allow_raw_pointers())
        .function("GetSoftBodyContactListener", &PhysicsSystem::GetSoftBodyContactListener, allow_raw_pointers());

    jolt_class_<PhysicsSettings>("PhysicsSettings")
        .constructor<>()
        .property("mBaumgarte", &PhysicsSettings::mBaumgarte)
        .property("mSpeculativeContactDistance", &PhysicsSettings::mSpeculativeContactDistance)
        .property("mPenetrationSlop", &PhysicsSettings::mPenetrationSlop)
        .property("mLinearCastThreshold", &PhysicsSettings::mLinearCastThreshold)
        .property("mLinearCastMaxPenetration", &PhysicsSettings::mLinearCastMaxPenetration)
        .property("mMaxPenetrationDistance", &PhysicsSettings::mMaxPenetrationDistance)
        .property("mNumVelocitySteps", &PhysicsSettings::mNumVelocitySteps)
        .property("mNumPositionSteps", &PhysicsSettings::mNumPositionSteps)
        .property("mMinVelocityForRestitution", &PhysicsSettings::mMinVelocityForRestitution)
        .property("mTimeBeforeSleep", &PhysicsSettings::mTimeBeforeSleep)
        .property("mPointVelocitySleepThreshold", &PhysicsSettings::mPointVelocitySleepThreshold)
        .property("mDeterministicSimulation", &PhysicsSettings::mDeterministicSimulation)
        .property("mConstraintWarmStart", &PhysicsSettings::mConstraintWarmStart)
        .property("mUseBodyPairContactCache", &PhysicsSettings::mUseBodyPairContactCache)
        .property("mUseManifoldReduction", &PhysicsSettings::mUseManifoldReduction)
        .property("mUseLargeIslandSplitter", &PhysicsSettings::mUseLargeIslandSplitter)
        .property("mAllowSleeping", &PhysicsSettings::mAllowSleeping)
        .property("mCheckActiveEdges", &PhysicsSettings::mCheckActiveEdges)
        .property("mMaxInFlightBodyPairs", &PhysicsSettings::mMaxInFlightBodyPairs)
        .property("mStepListenersBatchSize", &PhysicsSettings::mStepListenersBatchSize)
        .property("mStepListenerBatchesPerJob", &PhysicsSettings::mStepListenerBatchesPerJob)
        .property("mManifoldTolerance", &PhysicsSettings::mManifoldTolerance)
        .property("mBodyPairCacheMaxDeltaPositionSq", &PhysicsSettings::mBodyPairCacheMaxDeltaPositionSq)
        .property("mBodyPairCacheCosMaxDeltaRotationDiv2", &PhysicsSettings::mBodyPairCacheCosMaxDeltaRotationDiv2)
        .property("mContactNormalCosMaxDeltaRotation", &PhysicsSettings::mContactNormalCosMaxDeltaRotation)
        .property("mContactPointPreserveLambdaMaxDistSq", &PhysicsSettings::mContactPointPreserveLambdaMaxDistSq);

    jolt_class_<PhysicsStepListener>("PhysicsStepListener")
        .allow_subclass<PhysicsStepListenerWrapper>("PhysicsStepListenerWrapper");

    // ==== queries: single-closest raycast (the 90% path; no collector needed) ====
    jolt_class_<RRayCast>("RRayCast")
        .constructor<RVec3, Vec3>("origin, direction")   // (origin, direction) — direction length = ray length
        // reuse a ray without recreating (members are value_array of a template base,
        // which embind .property can't take a member pointer to — use a setter).
        .function("Set(origin, direction)",
            +[](RRayCast &r, RVec3 o, Vec3 d) { r.mOrigin = o; r.mDirection = d; })
        .out_function("GetPointOnRay(out, fraction)",
            out_desc::Vec3, out_desc::Pass, +[](const RRayCast &r, uintptr_t out, float f) { WriteVec3(r.GetPointOnRay(f), out); });
    jolt_class_<RayCast>("RayCast")
        .constructor<Vec3, Vec3>("origin, direction")
        .function("Set(origin, direction)",
            +[](RayCast &r, Vec3 o, Vec3 d) { r.mOrigin = o; r.mDirection = d; })
        .out_function("GetPointOnRay(out, fraction)",
            out_desc::Vec3, out_desc::Pass, +[](const RayCast &r, uintptr_t out, float f) { WriteVec3(r.GetPointOnRay(f), out); })
        .out_function("GetOrigin(out)", out_desc::Vec3, +[](const RayCast &r, uintptr_t out) { WriteVec3(r.mOrigin, out); })
        .out_function("GetDirection(out)", out_desc::Vec3, +[](const RayCast &r, uintptr_t out) { WriteVec3(r.mDirection, out); });
    jolt_class_<OrientedBox>("OrientedBox")
        .constructor<>()
        .constructor<Mat44, Vec3>("orientation, halfExtents")
        .out_function("GetOrientation(out)", out_desc::Mat44, +[](const OrientedBox &b, uintptr_t out) { WriteMat4(b.mOrientation, out); })
        .function("SetOrientation(orientation)", +[](OrientedBox &b, Mat44 m) { b.mOrientation = m; })
        .out_function("GetHalfExtents(out)", out_desc::Vec3, +[](const OrientedBox &b, uintptr_t out) { WriteVec3(b.mHalfExtents, out); })
        .function("SetHalfExtents(halfExtents)", +[](OrientedBox &b, Vec3 v) { b.mHalfExtents = v; });
    jolt_class_<AABoxCast>("AABoxCast")
        .constructor<>()
        .out_function("GetBox(out)", out_desc::AABox, +[](const AABoxCast &c, uintptr_t out) { WriteAABox(c.mBox, out); })
        .function("SetBox(box)", +[](AABoxCast &c, AABox b) { c.mBox = b; })
        .out_function("GetDirection(out)", out_desc::Vec3, +[](const AABoxCast &c, uintptr_t out) { WriteVec3(c.mDirection, out); })
        .function("SetDirection(direction)", +[](AABoxCast &c, Vec3 v) { c.mDirection = v; });
    jolt_class_<BroadPhaseCastResult>("BroadPhaseCastResult")
        .property("mFraction", &BroadPhaseCastResult::mFraction)
        .function("GetBodyID", +[](const BroadPhaseCastResult &r) { return fromBodyID(r.mBodyID); })
        .function("Reset", &BroadPhaseCastResult::Reset);
    jolt_class_<RayCastResult, base<BroadPhaseCastResult>>("RayCastResult")
        .constructor<>()
        .function("GetSubShapeID2", +[](const RayCastResult &r) { return (uint32)r.mSubShapeID2.GetValue(); });
    // -- collision filters (a default-constructed base = no-op "hit everything") --
    jolt_class_<BroadPhaseLayerFilter>("BroadPhaseLayerFilter").constructor<>()
        .allow_subclass<BroadPhaseLayerFilterWrapper>("BroadPhaseLayerFilterWrapper");
    jolt_class_<ObjectLayerFilter>("ObjectLayerFilter").constructor<>()
        .allow_subclass<ObjectLayerFilterWrapper>("ObjectLayerFilterWrapper");
    jolt_class_<BodyFilter>("BodyFilter").constructor<>()
        .allow_subclass<BodyFilterWrapper>("BodyFilterWrapper");
    jolt_class_<SpecifiedBroadPhaseLayerFilter, base<BroadPhaseLayerFilter>>("SpecifiedBroadPhaseLayerFilter")
        .constructor<BroadPhaseLayer>("layer");
    jolt_class_<SpecifiedObjectLayerFilter, base<ObjectLayerFilter>>("SpecifiedObjectLayerFilter")
        .constructor<ObjectLayer>("layer");
    jolt_class_<IgnoreSingleBodyFilter, base<BodyFilter>>("IgnoreSingleBodyFilter")
        .constructor("bodyID", +[](uint32 id) -> IgnoreSingleBodyFilter * { return new IgnoreSingleBodyFilter(toBodyID(id)); });
    jolt_class_<IgnoreMultipleBodiesFilter, base<BodyFilter>>("IgnoreMultipleBodiesFilter")
        .constructor<>()
        .function("Clear", &IgnoreMultipleBodiesFilter::Clear)
        .function("Reserve(size)", &IgnoreMultipleBodiesFilter::Reserve)
        .function("IgnoreBody(bodyID)", +[](IgnoreMultipleBodiesFilter &f, uint32 id) { f.IgnoreBody(toBodyID(id)); });
    jolt_class_<ShapeFilter>("ShapeFilter").constructor<>()
        .allow_subclass<ShapeFilterWrapper>("ShapeFilterWrapper");
    jolt_class_<SimShapeFilter>("SimShapeFilter").constructor<>()
        .allow_subclass<SimShapeFilterWrapper>("SimShapeFilterWrapper");
    // Standard layer filters (apply the ObjectLayerPairFilter rules for object layer L).
    // Constructed from JoltInterface.GetObjectVsBroadPhaseLayerFilter()/GetObjectLayerPairFilter().
    // Non-copyable, so ctor-only — the passed filter interface must outlive the filter.
    jolt_class_<DefaultBroadPhaseLayerFilter, base<BroadPhaseLayerFilter>>("DefaultBroadPhaseLayerFilter")
        .constructor("objectVsBroadPhaseLayerFilter, objectLayer",
            +[](const ObjectVsBroadPhaseLayerFilter &f, uint16 l) -> DefaultBroadPhaseLayerFilter * { return new DefaultBroadPhaseLayerFilter(f, l); }, allow_raw_pointers());
    jolt_class_<DefaultObjectLayerFilter, base<ObjectLayerFilter>>("DefaultObjectLayerFilter")
        .constructor("objectLayerPairFilter, objectLayer",
            +[](const ObjectLayerPairFilter &f, uint16 l) -> DefaultObjectLayerFilter * { return new DefaultObjectLayerFilter(f, l); }, allow_raw_pointers());

    jolt_class_<NarrowPhaseQuery>("NarrowPhaseQuery")
        // resets the hit then casts; returns true on hit (ioHit updated in place).
        .function("CastRay(ray, hit)",
            +[](const NarrowPhaseQuery &npq, const RRayCast &ray, RayCastResult &hit) {
                hit.Reset(); return npq.CastRay(ray, hit); })
        // ergonomic all-hit raycast against `objectLayer` -> a JS array of hits, sorted
        // near->far, each { bodyID, fraction, point:[x,y,z], normal:[x,y,z] }. Hides the
        // collector + filter plumbing (default filters from the JoltInterface).
        .function("CastRayAll(ray, objectLayer, jolt): Array<{ bodyID: number; fraction: number; point: Vec3; normal: Vec3 }>",
            +[](const NarrowPhaseQuery &npq, const RRayCast &ray, uint16 layer, JoltInterface &jolt) -> val {
                RayCastSettings settings;
                settings.SetBackFaceMode(EBackFaceMode::CollideWithBackFaces);
                AllHitCollisionCollector<CastRayCollector> collector;
                PhysicsSystem *ps = jolt.GetPhysicsSystem();
                npq.CastRay(ray, settings, collector,
                    ps->GetDefaultBroadPhaseLayerFilter(layer), ps->GetDefaultLayerFilter(layer), BodyFilter(), ShapeFilter());
                collector.Sort();
                const BodyLockInterface &lock = ps->GetBodyLockInterfaceNoLock();
                val arr = val::array();
                for (size_t i = 0; i < collector.mHits.size(); i++) {
                    const RayCastResult &h = collector.mHits[i];
                    RVec3 p = ray.GetPointOnRay(h.mFraction);
                    Vec3 n = Vec3::sZero();
                    { BodyLockRead bl(lock, h.mBodyID); if (bl.Succeeded()) n = bl.GetBody().GetWorldSpaceSurfaceNormal(h.mSubShapeID2, p); }
                    val o = val::object();
                    o.set("bodyID", (uint32)h.mBodyID.GetIndexAndSequenceNumber());
                    o.set("fraction", h.mFraction);
                    val pa = val::array(); pa.set(0, (float)p.GetX()); pa.set(1, (float)p.GetY()); pa.set(2, (float)p.GetZ()); o.set("point", pa);
                    val na = val::array(); na.set(0, n.GetX()); na.set(1, n.GetY()); na.set(2, n.GetZ()); o.set("normal", na);
                    arr.set((int)i, o);
                }
                return arr;
            }, allow_raw_pointers())
        .function("CastRayWithFilters(ray, hit, broadPhaseFilter, objectLayerFilter, bodyFilter)",
            +[](const NarrowPhaseQuery &npq, const RRayCast &ray, RayCastResult &hit,
                const BroadPhaseLayerFilter &bp, const ObjectLayerFilter &ol, const BodyFilter &bf) {
                hit.Reset(); return npq.CastRay(ray, hit, bp, ol, bf); }, allow_raw_pointers())
        .function("CastRayCollide(ray, settings, collector)",
            +[](const NarrowPhaseQuery &npq, const RRayCast &ray, const RayCastSettings &settings,
                CastRayCollector &collector) {
                npq.CastRay(ray, settings, collector); }, allow_raw_pointers())
        .function("CastRayCollideWithFilters(ray, settings, collector, broadPhaseFilter, objectLayerFilter, bodyFilter, shapeFilter)",
            +[](const NarrowPhaseQuery &npq, const RRayCast &ray, const RayCastSettings &settings,
                CastRayCollector &collector,
                const BroadPhaseLayerFilter &bpf, const ObjectLayerFilter &olf, const BodyFilter &bf, const ShapeFilter &sf) {
                npq.CastRay(ray, settings, collector, bpf, olf, bf, sf); }, allow_raw_pointers())
        // shape queries (fill the given collector; no-op filters). baseOffset makes
        // results relative to it — pass [0,0,0] for world-space.
        // Simple variants use all-pass filters. The *WithFilters variants take the four Jolt
        // filters — pass PhysicsSystem.GetDefaultBroadPhaseLayerFilter(layer) / GetDefaultLayerFilter(layer)
        // for standard layer rules, IgnoreSingleBody/MultipleBodiesFilter to skip bodies, ShapeFilter{}.
        .function("CollideShape(shape, scale, comTransform, settings, baseOffset, collector)",
            +[](const NarrowPhaseQuery &npq, const Shape *shape, Vec3 scale, RMat44 comTransform,
                const CollideShapeSettings &settings, RVec3 baseOffset, CollideShapeCollector &collector) {
                npq.CollideShape(shape, scale, comTransform, settings, baseOffset, collector); }, allow_raw_pointers())
        .function("CollideShapeWithFilters(shape, scale, comTransform, settings, baseOffset, collector, broadPhaseFilter, objectLayerFilter, bodyFilter, shapeFilter)",
            +[](const NarrowPhaseQuery &npq, const Shape *shape, Vec3 scale, RMat44 comTransform,
                const CollideShapeSettings &settings, RVec3 baseOffset, CollideShapeCollector &collector,
                const BroadPhaseLayerFilter &bpf, const ObjectLayerFilter &olf, const BodyFilter &bf, const ShapeFilter &sf) {
                npq.CollideShape(shape, scale, comTransform, settings, baseOffset, collector, bpf, olf, bf, sf); }, allow_raw_pointers())
        .function("CollideShapeWithInternalEdgeRemoval(shape, scale, comTransform, settings, baseOffset, collector)",
            +[](const NarrowPhaseQuery &npq, const Shape *shape, Vec3 scale, RMat44 comTransform,
                const CollideShapeSettings &settings, RVec3 baseOffset, CollideShapeCollector &collector) {
                npq.CollideShapeWithInternalEdgeRemoval(shape, scale, comTransform, settings, baseOffset, collector); }, allow_raw_pointers())
        .function("CollideShapeWithInternalEdgeRemovalWithFilters(shape, scale, comTransform, settings, baseOffset, collector, broadPhaseFilter, objectLayerFilter, bodyFilter, shapeFilter)",
            +[](const NarrowPhaseQuery &npq, const Shape *shape, Vec3 scale, RMat44 comTransform,
                const CollideShapeSettings &settings, RVec3 baseOffset, CollideShapeCollector &collector,
                const BroadPhaseLayerFilter &bpf, const ObjectLayerFilter &olf, const BodyFilter &bf, const ShapeFilter &sf) {
                npq.CollideShapeWithInternalEdgeRemoval(shape, scale, comTransform, settings, baseOffset, collector, bpf, olf, bf, sf); }, allow_raw_pointers())
        .function("CastShape(shapeCast, settings, baseOffset, collector)",
            +[](const NarrowPhaseQuery &npq, const RShapeCast &shapeCast, const ShapeCastSettings &settings,
                RVec3 baseOffset, CastShapeCollector &collector) {
                npq.CastShape(shapeCast, settings, baseOffset, collector); }, allow_raw_pointers())
        .function("CastShapeWithFilters(shapeCast, settings, baseOffset, collector, broadPhaseFilter, objectLayerFilter, bodyFilter, shapeFilter)",
            +[](const NarrowPhaseQuery &npq, const RShapeCast &shapeCast, const ShapeCastSettings &settings,
                RVec3 baseOffset, CastShapeCollector &collector,
                const BroadPhaseLayerFilter &bpf, const ObjectLayerFilter &olf, const BodyFilter &bf, const ShapeFilter &sf) {
                npq.CastShape(shapeCast, settings, baseOffset, collector, bpf, olf, bf, sf); }, allow_raw_pointers())
        .function("CollidePoint(point, collector)",
            +[](const NarrowPhaseQuery &npq, RVec3 point, CollidePointCollector &collector) {
                npq.CollidePoint(point, collector); }, allow_raw_pointers())
        .function("CollidePointWithFilters(point, collector, broadPhaseFilter, objectLayerFilter, bodyFilter, shapeFilter)",
            +[](const NarrowPhaseQuery &npq, RVec3 point, CollidePointCollector &collector,
                const BroadPhaseLayerFilter &bpf, const ObjectLayerFilter &olf, const BodyFilter &bf, const ShapeFilter &sf) {
                npq.CollidePoint(point, collector, bpf, olf, bf, sf); }, allow_raw_pointers())
        .function("CollectTransformedShapes(box, collector)",
            +[](const NarrowPhaseQuery &npq, AABox box, TransformedShapeCollector &collector) {
                npq.CollectTransformedShapes(box, collector); }, allow_raw_pointers())
        .function("CollectTransformedShapesWithFilters(box, collector, broadPhaseFilter, objectLayerFilter, bodyFilter, shapeFilter)",
            +[](const NarrowPhaseQuery &npq, AABox box, TransformedShapeCollector &collector,
                const BroadPhaseLayerFilter &bpf, const ObjectLayerFilter &olf, const BodyFilter &bf, const ShapeFilter &sf) {
                npq.CollectTransformedShapes(box, collector, bpf, olf, bf, sf); }, allow_raw_pointers());

    jolt_class_<BroadPhaseQuery>("BroadPhaseQuery")
        .function("CastRay(ray, collector, broadPhaseLayerFilter, objectLayerFilter)",
            +[](const BroadPhaseQuery &q, const RayCast &ray, RayCastBodyCollector &collector,
                const BroadPhaseLayerFilter &bpf, const ObjectLayerFilter &olf) {
                q.CastRay(ray, collector, bpf, olf); }, allow_raw_pointers())
        .function("CollideAABox(box, collector, broadPhaseLayerFilter, objectLayerFilter)",
            +[](const BroadPhaseQuery &q, AABox box, CollideShapeBodyCollector &collector,
                const BroadPhaseLayerFilter &bpf, const ObjectLayerFilter &olf) {
                q.CollideAABox(box, collector, bpf, olf); }, allow_raw_pointers())
        .function("CollideSphere(center, radius, collector, broadPhaseLayerFilter, objectLayerFilter)",
            +[](const BroadPhaseQuery &q, Vec3 center, float radius, CollideShapeBodyCollector &collector,
                const BroadPhaseLayerFilter &bpf, const ObjectLayerFilter &olf) {
                q.CollideSphere(center, radius, collector, bpf, olf); }, allow_raw_pointers())
        .function("CollidePoint(point, collector, broadPhaseLayerFilter, objectLayerFilter)",
            +[](const BroadPhaseQuery &q, Vec3 point, CollideShapeBodyCollector &collector,
                const BroadPhaseLayerFilter &bpf, const ObjectLayerFilter &olf) {
                q.CollidePoint(point, collector, bpf, olf); }, allow_raw_pointers())
        .function("CollideOrientedBox(box, collector, broadPhaseLayerFilter, objectLayerFilter)",
            +[](const BroadPhaseQuery &q, const OrientedBox &box, CollideShapeBodyCollector &collector,
                const BroadPhaseLayerFilter &bpf, const ObjectLayerFilter &olf) {
                q.CollideOrientedBox(box, collector, bpf, olf); }, allow_raw_pointers())
        .function("CastAABox(box, collector, broadPhaseLayerFilter, objectLayerFilter)",
            +[](const BroadPhaseQuery &q, const AABoxCast &box, CastShapeBodyCollector &collector,
                const BroadPhaseLayerFilter &bpf, const ObjectLayerFilter &olf) {
                q.CastAABox(box, collector, bpf, olf); }, allow_raw_pointers())
        .out_function("GetBounds(out)", out_desc::AABox,
            +[](const BroadPhaseQuery &q, uintptr_t out) { WriteAABox(q.GetBounds(), out); });

    // -- shape-query result structs --
    jolt_class_<CollideShapeResult>("CollideShapeResult")
        .property("mContactPointOn1", &CollideShapeResult::mContactPointOn1)
        .property("mContactPointOn2", &CollideShapeResult::mContactPointOn2)
        .property("mPenetrationAxis", &CollideShapeResult::mPenetrationAxis)   // normal = -normalize(this)
        .property("mPenetrationDepth", &CollideShapeResult::mPenetrationDepth)
        .function("GetBodyID2", +[](const CollideShapeResult &r) { return fromBodyID(r.mBodyID2); })
        .function("GetSubShapeID1", +[](const CollideShapeResult &r) { return (uint32)r.mSubShapeID1.GetValue(); })
        .function("GetSubShapeID2", +[](const CollideShapeResult &r) { return (uint32)r.mSubShapeID2.GetValue(); })
        .function("GetShape1FaceCount", +[](const CollideShapeResult &r) { return (uint32)r.mShape1Face.size(); })
        .function("GetShape2FaceCount", +[](const CollideShapeResult &r) { return (uint32)r.mShape2Face.size(); })
        .out_function("GetShape1FaceVertex(out, index)", out_desc::Vec3, out_desc::Pass,
            +[](const CollideShapeResult &r, uintptr_t out, uint32 i) { WriteVec3(r.mShape1Face[i], out); })
        .out_function("GetShape2FaceVertex(out, index)", out_desc::Vec3, out_desc::Pass,
            +[](const CollideShapeResult &r, uintptr_t out, uint32 i) { WriteVec3(r.mShape2Face[i], out); });
    jolt_class_<ShapeCastResult, base<CollideShapeResult>>("ShapeCastResult")
        .property("mFraction", &ShapeCastResult::mFraction)
        .property("mIsBackFaceHit", &ShapeCastResult::mIsBackFaceHit);
    jolt_class_<CollidePointResult>("CollidePointResult")
        .function("GetBodyID", +[](const CollidePointResult &r) { return fromBodyID(r.mBodyID); });

    // -- query settings + shape cast --
    jolt_class_<RayCastSettings>("RayCastSettings")
        .constructor<>()
        .function("SetBackFaceMode(mode)", &RayCastSettings::SetBackFaceMode)
        .property("mBackFaceModeTriangles", &RayCastSettings::mBackFaceModeTriangles)
        .property("mBackFaceModeConvex", &RayCastSettings::mBackFaceModeConvex)
        .property("mTreatConvexAsSolid", &RayCastSettings::mTreatConvexAsSolid);
    jolt_class_<CollideShapeSettings>("CollideShapeSettings")
        .constructor<>()
        .property("mMaxSeparationDistance", &CollideShapeSettings::mMaxSeparationDistance)
        .property("mBackFaceMode", &CollideShapeSettings::mBackFaceMode)
        // inherited from CollideSettingsBase -> getter/setter (member-ptr to base fails)
        .property("mActiveEdgeMode",
            +[](const CollideShapeSettings &s) { return s.mActiveEdgeMode; },
            +[](CollideShapeSettings &s, EActiveEdgeMode v) { s.mActiveEdgeMode = v; })
        .property("mCollectFacesMode",
            +[](const CollideShapeSettings &s) { return s.mCollectFacesMode; },
            +[](CollideShapeSettings &s, ECollectFacesMode v) { s.mCollectFacesMode = v; })
        .property("mCollisionTolerance",
            +[](const CollideShapeSettings &s) { return s.mCollisionTolerance; },
            +[](CollideShapeSettings &s, float v) { s.mCollisionTolerance = v; })
        .property("mPenetrationTolerance",
            +[](const CollideShapeSettings &s) { return s.mPenetrationTolerance; },
            +[](CollideShapeSettings &s, float v) { s.mPenetrationTolerance = v; })
        .out_function("GetActiveEdgeMovementDirection(out)", out_desc::Vec3,
            +[](const CollideShapeSettings &s, uintptr_t out) { WriteVec3(s.mActiveEdgeMovementDirection, out); })
        .function("SetActiveEdgeMovementDirection(direction)",
            +[](CollideShapeSettings &s, Vec3 v) { s.mActiveEdgeMovementDirection = v; });
    jolt_class_<ShapeCastSettings>("ShapeCastSettings")
        .constructor<>()
        .property("mBackFaceModeTriangles", &ShapeCastSettings::mBackFaceModeTriangles)
        .property("mBackFaceModeConvex", &ShapeCastSettings::mBackFaceModeConvex)
        .property("mReturnDeepestPoint", &ShapeCastSettings::mReturnDeepestPoint)
        .property("mUseShrunkenShapeAndConvexRadius", &ShapeCastSettings::mUseShrunkenShapeAndConvexRadius)
        .property("mActiveEdgeMode",
            +[](const ShapeCastSettings &s) { return s.mActiveEdgeMode; },
            +[](ShapeCastSettings &s, EActiveEdgeMode v) { s.mActiveEdgeMode = v; })
        .property("mCollectFacesMode",
            +[](const ShapeCastSettings &s) { return s.mCollectFacesMode; },
            +[](ShapeCastSettings &s, ECollectFacesMode v) { s.mCollectFacesMode = v; })
        .property("mCollisionTolerance",
            +[](const ShapeCastSettings &s) { return s.mCollisionTolerance; },
            +[](ShapeCastSettings &s, float v) { s.mCollisionTolerance = v; })
        .property("mPenetrationTolerance",
            +[](const ShapeCastSettings &s) { return s.mPenetrationTolerance; },
            +[](ShapeCastSettings &s, float v) { s.mPenetrationTolerance = v; })
        .out_function("GetActiveEdgeMovementDirection(out)", out_desc::Vec3,
            +[](const ShapeCastSettings &s, uintptr_t out) { WriteVec3(s.mActiveEdgeMovementDirection, out); })
        .function("SetActiveEdgeMovementDirection(direction)",
            +[](ShapeCastSettings &s, Vec3 v) { s.mActiveEdgeMovementDirection = v; });
    jolt_class_<RShapeCast>("RShapeCast")
        .constructor("shape, scale, centerOfMassStart, direction", +[](const Shape *shape, Vec3 scale, RMat44 comStart, Vec3 dir) -> RShapeCast * {
            return new RShapeCast(shape, scale, comStart, dir); }, allow_raw_pointers())
        .class_function("sFromWorldTransform(shape, scale, worldTransform, direction)",
            +[](const Shape *shape, Vec3 scale, RMat44 worldTransform, Vec3 dir) {
                return RShapeCast::sFromWorldTransform(shape, scale, worldTransform, dir); }, allow_raw_pointers());

    // -- collectors (closest-hit + all-hits per query type) --
    jolt_class_<CastRayCollector>("CastRayCollector")
        .allow_subclass<CastRayCollectorWrapper>("CastRayCollectorWrapper");
    jolt_class_<TransformedShapeCollector>("TransformedShapeCollector")
        .allow_subclass<TransformedShapeCollectorWrapper>("TransformedShapeCollectorWrapper");
    jolt_class_<CollideShapeCollector>("CollideShapeCollector")
        .allow_subclass<CollideShapeCollectorWrapper>("CollideShapeCollectorWrapper");
    jolt_class_<CastShapeCollector>("CastShapeCollector")
        .allow_subclass<CastShapeCollectorWrapper>("CastShapeCollectorWrapper");
    jolt_class_<CollidePointCollector>("CollidePointCollector")
        .allow_subclass<CollidePointCollectorWrapper>("CollidePointCollectorWrapper");
    jolt_class_<RayCastBodyCollector>("RayCastBodyCollector")
        .allow_subclass<RayCastBodyCollectorWrapper>("RayCastBodyCollectorWrapper");
    jolt_class_<CastShapeBodyCollector>("CastShapeBodyCollector")
        .allow_subclass<CastShapeBodyCollectorWrapper>("CastShapeBodyCollectorWrapper");
    jolt_class_<CollideShapeBodyCollector>("CollideShapeBodyCollector")
        .allow_subclass<CollideShapeBodyCollectorWrapper>("CollideShapeBodyCollectorWrapper");
    registerClosestHit<CollideShapeCollector>("CollideShapeClosestHitCollector");
    registerAllHit<CollideShapeCollector>("CollideShapeAllHitCollector");
    registerClosestHit<CastShapeCollector>("CastShapeClosestHitCollector");
    registerAllHit<CastShapeCollector>("CastShapeAllHitCollector");
    registerClosestHit<CollidePointCollector>("CollidePointClosestHitCollector");
    registerAllHit<CollidePointCollector>("CollidePointAllHitCollector");
    registerClosestHit<CastRayCollector>("CastRayClosestHitCollector");
    registerAllHit<CastRayCollector>("CastRayAllHitCollector");
    registerClosestHit<RayCastBodyCollector>("RayCastBodyClosestHitCollector");
    registerAllHit<RayCastBodyCollector>("RayCastBodyAllHitCollector");
    registerClosestHit<CastShapeBodyCollector>("CastShapeBodyClosestHitCollector");
    registerAllHit<CastShapeBodyCollector>("CastShapeBodyAllHitCollector");
    // CollideShapeBodyCollector ResultType is BodyID (no GetEarlyOutFraction) — cannot use
    // ClosestHit/AllHit templates. Use CollideShapeBodyCollector.implement({ AddHit(id){...} }).

    // world-attachment sentinel body (for constraints attaching a body to the world)
    emscripten::function("sGetFixedToWorldBody", +[]() { return &Body::sFixedToWorld; }, allow_raw_pointers());

    // ==== materials, collision groups, mass ==================================
    // -- PhysicsMaterial (refcounted) --
    jolt_class_<PhysicsMaterial>("PhysicsMaterial")
        .smart_ptr<Ref<PhysicsMaterial>>("PhysicsMaterialRef")
        .constructor(+[]() -> Ref<PhysicsMaterial> { return new PhysicsMaterial(); })
        .function("GetDebugName", +[](const PhysicsMaterial &m) { return std::string(m.GetDebugName()); })
        .function("GetDebugColor", +[](const PhysicsMaterial &m) { return m.GetDebugColor(); });   // -> [r,g,b,a]
    jolt_class_<PhysicsMaterialSimple, base<PhysicsMaterial>>("PhysicsMaterialSimple")
        .smart_ptr<Ref<PhysicsMaterialSimple>>("PhysicsMaterialSimpleRef")
        .constructor(+[]() -> Ref<PhysicsMaterialSimple> { return new PhysicsMaterialSimple(); })
        .constructor("name, color", +[](const std::string &name, Color color) -> Ref<PhysicsMaterialSimple> {
            return new PhysicsMaterialSimple(name, color); });

    // -- collision groups + group filter (per-sub-group filtering: ragdolls, vehicles) --
    jolt_class_<GroupFilter>("GroupFilter")
        .smart_ptr<Ref<GroupFilter>>("GroupFilterRef")
        .allow_subclass<GroupFilterWrapper>("GroupFilterJS");
    jolt_class_<GroupFilterTable, base<GroupFilter>>("GroupFilterTable")
        .smart_ptr<Ref<GroupFilterTable>>("GroupFilterTableRef")
        .constructor("numSubGroups", +[](uint numSubGroups) -> Ref<GroupFilterTable> { return new GroupFilterTable(numSubGroups); })
        .function("DisableCollision(subGroup1, subGroup2)", &GroupFilterTable::DisableCollision)
        .function("EnableCollision(subGroup1, subGroup2)", &GroupFilterTable::EnableCollision)
        .function("IsCollisionEnabled(subGroup1, subGroup2)", &GroupFilterTable::IsCollisionEnabled);
    jolt_class_<CollisionGroup>("CollisionGroup")   // plain value class (holds a RefConst<GroupFilter>)
        .constructor<>()
        .constructor("groupFilter, groupID, subGroupID", +[](const GroupFilter *f, uint32 g, uint32 sg) { return new CollisionGroup(f, g, sg); }, allow_raw_pointers())
        .function("SetGroupFilter(filter)", &CollisionGroup::SetGroupFilter, allow_raw_pointers())
        .function("GetGroupFilter", +[](const CollisionGroup &c) { return const_cast<GroupFilter *>(c.GetGroupFilter()); }, allow_raw_pointers())
        .function("SetGroupID(id)", &CollisionGroup::SetGroupID)
        .function("GetGroupID", &CollisionGroup::GetGroupID)
        .function("SetSubGroupID(id)", &CollisionGroup::SetSubGroupID)
        .function("GetSubGroupID", &CollisionGroup::GetSubGroupID);

    // -- mass properties (custom mass/inertia override) --
    jolt_class_<MassProperties>("MassProperties")
        .constructor<>()
        .property("mMass", &MassProperties::mMass)
        .property("mInertia", &MassProperties::mInertia)   // Mat44 -> [16] col-major
        .function("SetMassAndInertiaOfSolidBox(boxSize, density)", &MassProperties::SetMassAndInertiaOfSolidBox)
        .function("ScaleToMass(mass)", &MassProperties::ScaleToMass)
        .function("Rotate(rotation)", +[](MassProperties &m, Mat44 r) { m.Rotate(r); })
        .function("Translate(translation)", +[](MassProperties &m, Vec3 t) { m.Translate(t); })
        .function("Scale(scale)", +[](MassProperties &m, Vec3 s) { m.Scale(s); })
        .out_function("DecomposePrincipalMomentsOfInertia(outRotation, outDiagonal)",
            out_desc::Mat44, out_desc::Vec3,
            +[](const MassProperties &m, uintptr_t outRotation, uintptr_t outDiagonal) {
                Mat44 rot; Vec3 diag; bool ok = m.DecomposePrincipalMomentsOfInertia(rot, diag);
                WriteMat4(rot, outRotation); WriteVec3(diag, outDiagonal); return ok; });

    // ==== constraints ========================================================
    // helper value types
    jolt_class_<SpringSettings>("SpringSettings")
        .constructor<>()
        .property("mMode", &SpringSettings::mMode)
        .property("mFrequency", &SpringSettings::mFrequency)   // union with mStiffness
        .property("mStiffness", &SpringSettings::mStiffness)
        .property("mDamping", &SpringSettings::mDamping)
        .function("HasStiffness", &SpringSettings::HasStiffness);
    jolt_class_<LinearCurve::Point>("LinearCurvePoint")
        .constructor<>()
        .property("mX", &LinearCurve::Point::mX)
        .property("mY", &LinearCurve::Point::mY);
    jolt_class_<LinearCurve>("LinearCurve")
        .constructor<>()
        .function("Clear", &LinearCurve::Clear)
        .function("Reserve(size)", &LinearCurve::Reserve)
        .function("AddPoint(x, y)", &LinearCurve::AddPoint)
        .function("Sort", &LinearCurve::Sort)
        .function("GetMinX", &LinearCurve::GetMinX)
        .function("GetMaxX", &LinearCurve::GetMaxX)
        .function("GetValue(x)", &LinearCurve::GetValue)
        .function("GetNumPoints", +[](const LinearCurve &c) { return (uint32)c.mPoints.size(); })
        .function("GetPoint(index)", +[](LinearCurve &c, uint32 i) { return &c.mPoints[i]; }, allow_raw_pointers());
    jolt_class_<MotorSettings>("MotorSettings")
        .constructor<>()
        .property("mMinForceLimit", &MotorSettings::mMinForceLimit)
        .property("mMaxForceLimit", &MotorSettings::mMaxForceLimit)
        .property("mMinTorqueLimit", &MotorSettings::mMinTorqueLimit)
        .property("mMaxTorqueLimit", &MotorSettings::mMaxTorqueLimit)
        .function("SetForceLimit(limit)", &MotorSettings::SetForceLimit)
        .function("SetTorqueLimit(limit)", &MotorSettings::SetTorqueLimit)
        .function("SetForceLimits(min, max)", &MotorSettings::SetForceLimits)
        .function("SetTorqueLimits(min, max)", &MotorSettings::SetTorqueLimits)
        .function("GetSpringSettings", +[](MotorSettings &s) { return &s.mSpringSettings; }, allow_raw_pointers());

    // base hierarchy (refcounted)
    jolt_class_<Constraint>("Constraint")
        .smart_ptr<Ref<Constraint>>("ConstraintRef")
        .function("GetType", &Constraint::GetType)
        .function("GetSubType", &Constraint::GetSubType)
        .function("SetEnabled(enabled)", &Constraint::SetEnabled)
        .function("GetEnabled", &Constraint::GetEnabled)
        .function("GetUserData", &Constraint::GetUserData)
        .function("SetUserData(userData)", &Constraint::SetUserData)
        .function("IsActive", &Constraint::IsActive)
        .function("GetConstraintPriority", &Constraint::GetConstraintPriority)
        .function("SetConstraintPriority(priority)", &Constraint::SetConstraintPriority)
        .function("GetNumVelocityStepsOverride", &Constraint::GetNumVelocityStepsOverride)
        .function("SetNumVelocityStepsOverride(n)", &Constraint::SetNumVelocityStepsOverride)
        .function("GetNumPositionStepsOverride", &Constraint::GetNumPositionStepsOverride)
        .function("SetNumPositionStepsOverride(n)", &Constraint::SetNumPositionStepsOverride)
        .function("ResetWarmStart", &Constraint::ResetWarmStart)
        .function("GetConstraintSettings", &Constraint::GetConstraintSettings);
    jolt_class_<TwoBodyConstraint, base<Constraint>>("TwoBodyConstraint")
        .smart_ptr<Ref<TwoBodyConstraint>>("TwoBodyConstraintRef")
        .function("GetBody1", &TwoBodyConstraint::GetBody1, allow_raw_pointers())
        .function("GetBody2", &TwoBodyConstraint::GetBody2, allow_raw_pointers())
        .out_function("GetConstraintToBody1Matrix(out)", out_desc::Mat44,
            +[](const TwoBodyConstraint &c, uintptr_t out) { WriteMat4(c.GetConstraintToBody1Matrix(), out); })
        .out_function("GetConstraintToBody2Matrix(out)", out_desc::Mat44,
            +[](const TwoBodyConstraint &c, uintptr_t out) { WriteMat4(c.GetConstraintToBody2Matrix(), out); });
    jolt_class_<ConstraintSettings>("ConstraintSettings")   // abstract
        .smart_ptr<Ref<ConstraintSettings>>("ConstraintSettingsRef")
        .property("mEnabled", &ConstraintSettings::mEnabled)
        .property("mConstraintPriority", &ConstraintSettings::mConstraintPriority)
        .property("mNumVelocityStepsOverride", &ConstraintSettings::mNumVelocityStepsOverride)
        .property("mNumPositionStepsOverride", &ConstraintSettings::mNumPositionStepsOverride)
        .property("mDrawConstraintSize", &ConstraintSettings::mDrawConstraintSize)
        .property("mUserData", &ConstraintSettings::mUserData);
    jolt_class_<TwoBodyConstraintSettings, base<ConstraintSettings>>("TwoBodyConstraintSettings")   // abstract
        .smart_ptr<Ref<TwoBodyConstraintSettings>>("TwoBodyConstraintSettingsRef")
        .function("Create(body1, body2)",
            +[](const TwoBodyConstraintSettings &s, Body &b1, Body &b2) -> Ref<TwoBodyConstraint> {
                return s.Create(b1, b2); }, allow_raw_pointers());

    // -- Fixed --
    jolt_class_<FixedConstraintSettings, base<TwoBodyConstraintSettings>>("FixedConstraintSettings")
        .smart_ptr<Ref<FixedConstraintSettings>>("FixedConstraintSettingsRef")
        .constructor(+[]() -> Ref<FixedConstraintSettings> { return new FixedConstraintSettings(); })
        .property("mSpace", &FixedConstraintSettings::mSpace)
        .property("mAutoDetectPoint", &FixedConstraintSettings::mAutoDetectPoint)
        .property("mPoint1", &FixedConstraintSettings::mPoint1)
        .property("mAxisX1", &FixedConstraintSettings::mAxisX1)
        .property("mAxisY1", &FixedConstraintSettings::mAxisY1)
        .property("mPoint2", &FixedConstraintSettings::mPoint2)
        .property("mAxisX2", &FixedConstraintSettings::mAxisX2)
        .property("mAxisY2", &FixedConstraintSettings::mAxisY2);
    jolt_class_<FixedConstraint, base<TwoBodyConstraint>>("FixedConstraint")
        .smart_ptr<Ref<FixedConstraint>>("FixedConstraintRef")
        .out_function("GetTotalLambdaPosition(out)", out_desc::Vec3, +[](const FixedConstraint &s, uintptr_t out) { WriteVec3(s.GetTotalLambdaPosition(), out); })
        .out_function("GetTotalLambdaRotation(out)", out_desc::Vec3, +[](const FixedConstraint &s, uintptr_t out) { WriteVec3(s.GetTotalLambdaRotation(), out); });

    // -- Point --
    jolt_class_<PointConstraintSettings, base<TwoBodyConstraintSettings>>("PointConstraintSettings")
        .smart_ptr<Ref<PointConstraintSettings>>("PointConstraintSettingsRef")
        .constructor(+[]() -> Ref<PointConstraintSettings> { return new PointConstraintSettings(); })
        .property("mSpace", &PointConstraintSettings::mSpace)
        .property("mPoint1", &PointConstraintSettings::mPoint1)
        .property("mPoint2", &PointConstraintSettings::mPoint2);
    jolt_class_<PointConstraint, base<TwoBodyConstraint>>("PointConstraint")
        .smart_ptr<Ref<PointConstraint>>("PointConstraintRef")
        .function("SetPoint1(space, point)", &PointConstraint::SetPoint1)
        .function("SetPoint2(space, point)", &PointConstraint::SetPoint2)
        .out_function("GetLocalSpacePoint1(out)", out_desc::Vec3, +[](const PointConstraint &c, uintptr_t out) { WriteVec3(c.GetLocalSpacePoint1(), out); })
        .out_function("GetLocalSpacePoint2(out)", out_desc::Vec3, +[](const PointConstraint &c, uintptr_t out) { WriteVec3(c.GetLocalSpacePoint2(), out); })
        .out_function("GetTotalLambdaPosition(out)", out_desc::Vec3, +[](const PointConstraint &s, uintptr_t out) { WriteVec3(s.GetTotalLambdaPosition(), out); });

    // -- Distance --
    jolt_class_<DistanceConstraintSettings, base<TwoBodyConstraintSettings>>("DistanceConstraintSettings")
        .smart_ptr<Ref<DistanceConstraintSettings>>("DistanceConstraintSettingsRef")
        .constructor(+[]() -> Ref<DistanceConstraintSettings> { return new DistanceConstraintSettings(); })
        .property("mSpace", &DistanceConstraintSettings::mSpace)
        .property("mPoint1", &DistanceConstraintSettings::mPoint1)
        .property("mPoint2", &DistanceConstraintSettings::mPoint2)
        .property("mMinDistance", &DistanceConstraintSettings::mMinDistance)
        .property("mMaxDistance", &DistanceConstraintSettings::mMaxDistance);
    jolt_class_<DistanceConstraint, base<TwoBodyConstraint>>("DistanceConstraint")
        .smart_ptr<Ref<DistanceConstraint>>("DistanceConstraintRef")
        .function("SetDistance(min, max)", &DistanceConstraint::SetDistance)
        .function("GetMinDistance", &DistanceConstraint::GetMinDistance)
        .function("GetMaxDistance", &DistanceConstraint::GetMaxDistance)
        .function("GetTotalLambdaPosition", &DistanceConstraint::GetTotalLambdaPosition)
        .function("GetLimitsSpringSettings", +[](DistanceConstraint &c) { return &c.GetLimitsSpringSettings(); }, allow_raw_pointers())
        .function("SetLimitsSpringSettings(settings)", &DistanceConstraint::SetLimitsSpringSettings);

    // -- Hinge --
    jolt_class_<HingeConstraintSettings, base<TwoBodyConstraintSettings>>("HingeConstraintSettings")
        .smart_ptr<Ref<HingeConstraintSettings>>("HingeConstraintSettingsRef")
        .constructor(+[]() -> Ref<HingeConstraintSettings> { return new HingeConstraintSettings(); })
        .property("mSpace", &HingeConstraintSettings::mSpace)
        .property("mPoint1", &HingeConstraintSettings::mPoint1)
        .property("mHingeAxis1", &HingeConstraintSettings::mHingeAxis1)
        .property("mNormalAxis1", &HingeConstraintSettings::mNormalAxis1)
        .property("mPoint2", &HingeConstraintSettings::mPoint2)
        .property("mHingeAxis2", &HingeConstraintSettings::mHingeAxis2)
        .property("mNormalAxis2", &HingeConstraintSettings::mNormalAxis2)
        .property("mLimitsMin", &HingeConstraintSettings::mLimitsMin)
        .property("mLimitsMax", &HingeConstraintSettings::mLimitsMax)
        .property("mMaxFrictionTorque", &HingeConstraintSettings::mMaxFrictionTorque)
        .function("GetMotorSettings", +[](HingeConstraintSettings &s) { return &s.mMotorSettings; }, allow_raw_pointers());
    jolt_class_<HingeConstraint, base<TwoBodyConstraint>>("HingeConstraint")
        .smart_ptr<Ref<HingeConstraint>>("HingeConstraintRef")
        .function("GetCurrentAngle", &HingeConstraint::GetCurrentAngle)
        .function("SetMotorState(state)", &HingeConstraint::SetMotorState)
        .function("GetMotorState", &HingeConstraint::GetMotorState)
        .function("SetTargetAngularVelocity(velocity)", &HingeConstraint::SetTargetAngularVelocity)
        .function("GetTargetAngularVelocity", &HingeConstraint::GetTargetAngularVelocity)
        .function("SetTargetAngle(angle)", &HingeConstraint::SetTargetAngle)
        .function("GetTargetAngle", &HingeConstraint::GetTargetAngle)
        .function("SetLimits(min, max)", &HingeConstraint::SetLimits)
        .function("GetLimitsMin", &HingeConstraint::GetLimitsMin)
        .function("GetLimitsMax", &HingeConstraint::GetLimitsMax)
        .function("HasLimits", &HingeConstraint::HasLimits)
        .function("SetMaxFrictionTorque(frictionTorque)", &HingeConstraint::SetMaxFrictionTorque)
        .function("GetMaxFrictionTorque", &HingeConstraint::GetMaxFrictionTorque)
        .function("GetMotorSettings", +[](HingeConstraint &c) { return &c.GetMotorSettings(); }, allow_raw_pointers())
        .out_function("GetLocalSpacePoint1(out)", out_desc::Vec3, +[](const HingeConstraint &c, uintptr_t out) { WriteVec3(c.GetLocalSpacePoint1(), out); })
        .out_function("GetLocalSpacePoint2(out)", out_desc::Vec3, +[](const HingeConstraint &c, uintptr_t out) { WriteVec3(c.GetLocalSpacePoint2(), out); })
        .out_function("GetLocalSpaceHingeAxis1(out)", out_desc::Vec3, +[](const HingeConstraint &c, uintptr_t out) { WriteVec3(c.GetLocalSpaceHingeAxis1(), out); })
        .out_function("GetLocalSpaceHingeAxis2(out)", out_desc::Vec3, +[](const HingeConstraint &c, uintptr_t out) { WriteVec3(c.GetLocalSpaceHingeAxis2(), out); })
        .out_function("GetLocalSpaceNormalAxis1(out)", out_desc::Vec3, +[](const HingeConstraint &c, uintptr_t out) { WriteVec3(c.GetLocalSpaceNormalAxis1(), out); })
        .out_function("GetLocalSpaceNormalAxis2(out)", out_desc::Vec3, +[](const HingeConstraint &c, uintptr_t out) { WriteVec3(c.GetLocalSpaceNormalAxis2(), out); })
        .function("SetTargetOrientationBS(orientation)", &HingeConstraint::SetTargetOrientationBS)
        .function("GetLimitsSpringSettings", +[](HingeConstraint &c) { return &c.GetLimitsSpringSettings(); }, allow_raw_pointers())
        .function("SetLimitsSpringSettings(settings)", &HingeConstraint::SetLimitsSpringSettings)
        .out_function("GetTotalLambdaPosition(out)", out_desc::Vec3, +[](const HingeConstraint &c, uintptr_t out) { WriteVec3(c.GetTotalLambdaPosition(), out); })
        .out_function("GetTotalLambdaRotation(out)", out_desc::Float2, +[](const HingeConstraint &c, uintptr_t out) { WriteVec2(c.GetTotalLambdaRotation(), out); })
        .function("GetTotalLambdaRotationLimits", &HingeConstraint::GetTotalLambdaRotationLimits)
        .function("GetTotalLambdaMotor", &HingeConstraint::GetTotalLambdaMotor);

    // -- Cone --
    jolt_class_<ConeConstraintSettings, base<TwoBodyConstraintSettings>>("ConeConstraintSettings")
        .smart_ptr<Ref<ConeConstraintSettings>>("ConeConstraintSettingsRef")
        .constructor(+[]() -> Ref<ConeConstraintSettings> { return new ConeConstraintSettings(); })
        .property("mSpace", &ConeConstraintSettings::mSpace)
        .property("mPoint1", &ConeConstraintSettings::mPoint1)
        .property("mTwistAxis1", &ConeConstraintSettings::mTwistAxis1)
        .property("mPoint2", &ConeConstraintSettings::mPoint2)
        .property("mTwistAxis2", &ConeConstraintSettings::mTwistAxis2)
        .property("mHalfConeAngle", &ConeConstraintSettings::mHalfConeAngle);
    jolt_class_<ConeConstraint, base<TwoBodyConstraint>>("ConeConstraint")
        .smart_ptr<Ref<ConeConstraint>>("ConeConstraintRef")
        .function("SetHalfConeAngle(angle)", &ConeConstraint::SetHalfConeAngle)
        .function("GetCosHalfConeAngle", &ConeConstraint::GetCosHalfConeAngle)
        .out_function("GetTotalLambdaPosition(out)", out_desc::Vec3, +[](const ConeConstraint &c, uintptr_t out) { WriteVec3(c.GetTotalLambdaPosition(), out); })
        .function("GetTotalLambdaRotation", &ConeConstraint::GetTotalLambdaRotation);

    // -- Slider --
    jolt_class_<SliderConstraintSettings, base<TwoBodyConstraintSettings>>("SliderConstraintSettings")
        .smart_ptr<Ref<SliderConstraintSettings>>("SliderConstraintSettingsRef")
        .constructor(+[]() -> Ref<SliderConstraintSettings> { return new SliderConstraintSettings(); })
        .function("SetSliderAxis(sliderAxis)", &SliderConstraintSettings::SetSliderAxis)
        .property("mSpace", &SliderConstraintSettings::mSpace)
        .property("mAutoDetectPoint", &SliderConstraintSettings::mAutoDetectPoint)
        .property("mPoint1", &SliderConstraintSettings::mPoint1)
        .property("mSliderAxis1", &SliderConstraintSettings::mSliderAxis1)
        .property("mNormalAxis1", &SliderConstraintSettings::mNormalAxis1)
        .property("mPoint2", &SliderConstraintSettings::mPoint2)
        .property("mSliderAxis2", &SliderConstraintSettings::mSliderAxis2)
        .property("mNormalAxis2", &SliderConstraintSettings::mNormalAxis2)
        .property("mLimitsMin", &SliderConstraintSettings::mLimitsMin)
        .property("mLimitsMax", &SliderConstraintSettings::mLimitsMax)
        .property("mMaxFrictionForce", &SliderConstraintSettings::mMaxFrictionForce)
        .function("GetMotorSettings", +[](SliderConstraintSettings &s) { return &s.mMotorSettings; }, allow_raw_pointers());
    jolt_class_<SliderConstraint, base<TwoBodyConstraint>>("SliderConstraint")
        .smart_ptr<Ref<SliderConstraint>>("SliderConstraintRef")
        .function("GetCurrentPosition", &SliderConstraint::GetCurrentPosition)
        .function("SetMotorState(state)", &SliderConstraint::SetMotorState)
        .function("GetMotorState", &SliderConstraint::GetMotorState)
        .function("SetTargetVelocity(velocity)", &SliderConstraint::SetTargetVelocity)
        .function("GetTargetVelocity", &SliderConstraint::GetTargetVelocity)
        .function("SetTargetPosition(position)", &SliderConstraint::SetTargetPosition)
        .function("GetTargetPosition", &SliderConstraint::GetTargetPosition)
        .function("SetLimits(min, max)", &SliderConstraint::SetLimits)
        .function("GetLimitsMin", &SliderConstraint::GetLimitsMin)
        .function("GetLimitsMax", &SliderConstraint::GetLimitsMax)
        .function("HasLimits", &SliderConstraint::HasLimits)
        .function("SetMaxFrictionForce(frictionForce)", &SliderConstraint::SetMaxFrictionForce)
        .function("GetMaxFrictionForce", &SliderConstraint::GetMaxFrictionForce)
        .function("GetMotorSettings", +[](SliderConstraint &c) { return &c.GetMotorSettings(); }, allow_raw_pointers())
        .function("GetLimitsSpringSettings", +[](SliderConstraint &c) { return &c.GetLimitsSpringSettings(); }, allow_raw_pointers())
        .function("SetLimitsSpringSettings(settings)", &SliderConstraint::SetLimitsSpringSettings)
        .out_function("GetTotalLambdaPosition(out)", out_desc::Float2, +[](const SliderConstraint &c, uintptr_t out) { WriteVec2(c.GetTotalLambdaPosition(), out); })
        .function("GetTotalLambdaPositionLimits", &SliderConstraint::GetTotalLambdaPositionLimits)
        .out_function("GetTotalLambdaRotation(out)", out_desc::Vec3, +[](const SliderConstraint &c, uintptr_t out) { WriteVec3(c.GetTotalLambdaRotation(), out); })
        .function("GetTotalLambdaMotor", &SliderConstraint::GetTotalLambdaMotor);

    // -- SwingTwist (note: mPosition1/2, not mPoint) --
    jolt_class_<SwingTwistConstraintSettings, base<TwoBodyConstraintSettings>>("SwingTwistConstraintSettings")
        .smart_ptr<Ref<SwingTwistConstraintSettings>>("SwingTwistConstraintSettingsRef")
        .constructor(+[]() -> Ref<SwingTwistConstraintSettings> { return new SwingTwistConstraintSettings(); })
        .property("mSpace", &SwingTwistConstraintSettings::mSpace)
        .property("mPosition1", &SwingTwistConstraintSettings::mPosition1)
        .property("mTwistAxis1", &SwingTwistConstraintSettings::mTwistAxis1)
        .property("mPlaneAxis1", &SwingTwistConstraintSettings::mPlaneAxis1)
        .property("mPosition2", &SwingTwistConstraintSettings::mPosition2)
        .property("mTwistAxis2", &SwingTwistConstraintSettings::mTwistAxis2)
        .property("mPlaneAxis2", &SwingTwistConstraintSettings::mPlaneAxis2)
        .property("mSwingType", &SwingTwistConstraintSettings::mSwingType)
        .property("mNormalHalfConeAngle", &SwingTwistConstraintSettings::mNormalHalfConeAngle)
        .property("mPlaneHalfConeAngle", &SwingTwistConstraintSettings::mPlaneHalfConeAngle)
        .property("mTwistMinAngle", &SwingTwistConstraintSettings::mTwistMinAngle)
        .property("mTwistMaxAngle", &SwingTwistConstraintSettings::mTwistMaxAngle)
        .property("mMaxFrictionTorque", &SwingTwistConstraintSettings::mMaxFrictionTorque)
        .function("GetSwingMotorSettings", +[](SwingTwistConstraintSettings &s) { return &s.mSwingMotorSettings; }, allow_raw_pointers())
        .function("GetTwistMotorSettings", +[](SwingTwistConstraintSettings &s) { return &s.mTwistMotorSettings; }, allow_raw_pointers());
    jolt_class_<SwingTwistConstraint, base<TwoBodyConstraint>>("SwingTwistConstraint")
        .smart_ptr<Ref<SwingTwistConstraint>>("SwingTwistConstraintRef")
        .function("GetSwingMotorSettings", +[](SwingTwistConstraint &c) { return &c.GetSwingMotorSettings(); }, allow_raw_pointers())
        .function("GetTwistMotorSettings", +[](SwingTwistConstraint &c) { return &c.GetTwistMotorSettings(); }, allow_raw_pointers())
        .function("SetSwingMotorState(state)", &SwingTwistConstraint::SetSwingMotorState)
        .function("SetTwistMotorState(state)", &SwingTwistConstraint::SetTwistMotorState)
        .function("SetTargetOrientationCS(orientation)", &SwingTwistConstraint::SetTargetOrientationCS)
        .out_function("GetLocalSpacePosition1(out)", out_desc::Vec3, +[](const SwingTwistConstraint &c, uintptr_t out) { WriteVec3(c.GetLocalSpacePosition1(), out); })
        .out_function("GetLocalSpacePosition2(out)", out_desc::Vec3, +[](const SwingTwistConstraint &c, uintptr_t out) { WriteVec3(c.GetLocalSpacePosition2(), out); })
        .out_function("GetConstraintToBody1(out)", out_desc::Quat, +[](const SwingTwistConstraint &c, uintptr_t out) { WriteQuat(c.GetConstraintToBody1(), out); })
        .out_function("GetConstraintToBody2(out)", out_desc::Quat, +[](const SwingTwistConstraint &c, uintptr_t out) { WriteQuat(c.GetConstraintToBody2(), out); })
        .function("GetNormalHalfConeAngle", &SwingTwistConstraint::GetNormalHalfConeAngle)
        .function("SetNormalHalfConeAngle(angle)", &SwingTwistConstraint::SetNormalHalfConeAngle)
        .function("GetPlaneHalfConeAngle", &SwingTwistConstraint::GetPlaneHalfConeAngle)
        .function("SetPlaneHalfConeAngle(angle)", &SwingTwistConstraint::SetPlaneHalfConeAngle)
        .function("GetTwistMinAngle", &SwingTwistConstraint::GetTwistMinAngle)
        .function("SetTwistMinAngle(angle)", &SwingTwistConstraint::SetTwistMinAngle)
        .function("GetTwistMaxAngle", &SwingTwistConstraint::GetTwistMaxAngle)
        .function("SetTwistMaxAngle(angle)", &SwingTwistConstraint::SetTwistMaxAngle)
        .function("SetMaxFrictionTorque(frictionTorque)", &SwingTwistConstraint::SetMaxFrictionTorque)
        .function("GetMaxFrictionTorque", &SwingTwistConstraint::GetMaxFrictionTorque)
        .function("GetSwingMotorState", &SwingTwistConstraint::GetSwingMotorState)
        .function("GetTwistMotorState", &SwingTwistConstraint::GetTwistMotorState)
        .function("SetTargetAngularVelocityCS(angularVelocity)", &SwingTwistConstraint::SetTargetAngularVelocityCS)
        .out_function("GetTargetAngularVelocityCS(out)", out_desc::Vec3, +[](const SwingTwistConstraint &c, uintptr_t out) { WriteVec3(c.GetTargetAngularVelocityCS(), out); })
        .out_function("GetTargetOrientationCS(out)", out_desc::Quat, +[](const SwingTwistConstraint &c, uintptr_t out) { WriteQuat(c.GetTargetOrientationCS(), out); })
        .function("SetTargetOrientationBS(orientation)", &SwingTwistConstraint::SetTargetOrientationBS)
        .out_function("GetRotationInConstraintSpace(out)", out_desc::Quat, +[](const SwingTwistConstraint &c, uintptr_t out) { WriteQuat(c.GetRotationInConstraintSpace(), out); })
        .out_function("GetTotalLambdaPosition(out)", out_desc::Vec3, +[](const SwingTwistConstraint &c, uintptr_t out) { WriteVec3(c.GetTotalLambdaPosition(), out); })
        .function("GetTotalLambdaTwist", &SwingTwistConstraint::GetTotalLambdaTwist)
        .function("GetTotalLambdaSwingY", &SwingTwistConstraint::GetTotalLambdaSwingY)
        .function("GetTotalLambdaSwingZ", &SwingTwistConstraint::GetTotalLambdaSwingZ)
        .out_function("GetTotalLambdaMotor(out)", out_desc::Vec3, +[](const SwingTwistConstraint &c, uintptr_t out) { WriteVec3(c.GetTotalLambdaMotor(), out); });

    // -- SixDOF (config via helper setters; C-array members not exposed directly) --
    jolt_class_<SixDOFConstraintSettings, base<TwoBodyConstraintSettings>>("SixDOFConstraintSettings")
        .smart_ptr<Ref<SixDOFConstraintSettings>>("SixDOFConstraintSettingsRef")
        .constructor(+[]() -> Ref<SixDOFConstraintSettings> { return new SixDOFConstraintSettings(); })
        .property("mSpace", &SixDOFConstraintSettings::mSpace)
        .property("mPosition1", &SixDOFConstraintSettings::mPosition1)
        .property("mAxisX1", &SixDOFConstraintSettings::mAxisX1)
        .property("mAxisY1", &SixDOFConstraintSettings::mAxisY1)
        .property("mPosition2", &SixDOFConstraintSettings::mPosition2)
        .property("mAxisX2", &SixDOFConstraintSettings::mAxisX2)
        .property("mAxisY2", &SixDOFConstraintSettings::mAxisY2)
        .property("mSwingType", &SixDOFConstraintSettings::mSwingType)
        .function("MakeFreeAxis(axis)", &SixDOFConstraintSettings::MakeFreeAxis)
        .function("MakeFixedAxis(axis)", &SixDOFConstraintSettings::MakeFixedAxis)
        .function("SetLimitedAxis(axis, min, max)", &SixDOFConstraintSettings::SetLimitedAxis)
        .function("IsFreeAxis(axis)", &SixDOFConstraintSettings::IsFreeAxis)
        .function("IsFixedAxis(axis)", &SixDOFConstraintSettings::IsFixedAxis)
        .function("GetLimitMin(axis)", +[](const SixDOFConstraintSettings &s, int i) { return s.mLimitMin[i]; })
        .function("SetLimitMin(axis, v)", +[](SixDOFConstraintSettings &s, int i, float v) { s.mLimitMin[i] = v; })
        .function("GetLimitMax(axis)", +[](const SixDOFConstraintSettings &s, int i) { return s.mLimitMax[i]; })
        .function("SetLimitMax(axis, v)", +[](SixDOFConstraintSettings &s, int i, float v) { s.mLimitMax[i] = v; })
        .function("GetMaxFriction(axis)", +[](const SixDOFConstraintSettings &s, int i) { return s.mMaxFriction[i]; })
        .function("SetMaxFriction(axis, v)", +[](SixDOFConstraintSettings &s, int i, float v) { s.mMaxFriction[i] = v; })
        .function("GetLimitsSpringSettings(axis)", +[](SixDOFConstraintSettings &s, int i) { return &s.mLimitsSpringSettings[i]; }, allow_raw_pointers())
        .function("GetMotorSettings(axis)", +[](SixDOFConstraintSettings &s, int i) { return &s.mMotorSettings[i]; }, allow_raw_pointers());
    jolt_class_<SixDOFConstraint, base<TwoBodyConstraint>>("SixDOFConstraint")
        .smart_ptr<Ref<SixDOFConstraint>>("SixDOFConstraintRef")
        .function("GetLimitsMin(axis)", &SixDOFConstraint::GetLimitsMin)
        .function("GetLimitsMax(axis)", &SixDOFConstraint::GetLimitsMax)
        .function("SetMotorState(axis, state)", &SixDOFConstraint::SetMotorState)
        .function("SetTargetPositionCS(position)", &SixDOFConstraint::SetTargetPositionCS)
        .function("SetTargetOrientationCS(orientation)", &SixDOFConstraint::SetTargetOrientationCS)
        .function("SetTranslationLimits(limitMin, limitMax)", &SixDOFConstraint::SetTranslationLimits)
        .function("SetRotationLimits(limitMin, limitMax)", &SixDOFConstraint::SetRotationLimits)
        .out_function("GetTranslationLimitsMin(out)", out_desc::Vec3, +[](const SixDOFConstraint &c, uintptr_t out) { WriteVec3(c.GetTranslationLimitsMin(), out); })
        .out_function("GetTranslationLimitsMax(out)", out_desc::Vec3, +[](const SixDOFConstraint &c, uintptr_t out) { WriteVec3(c.GetTranslationLimitsMax(), out); })
        .out_function("GetRotationLimitsMin(out)", out_desc::Vec3, +[](const SixDOFConstraint &c, uintptr_t out) { WriteVec3(c.GetRotationLimitsMin(), out); })
        .out_function("GetRotationLimitsMax(out)", out_desc::Vec3, +[](const SixDOFConstraint &c, uintptr_t out) { WriteVec3(c.GetRotationLimitsMax(), out); })
        .function("IsFixedAxis(axis)", &SixDOFConstraint::IsFixedAxis)
        .function("IsFreeAxis(axis)", &SixDOFConstraint::IsFreeAxis)
        .function("GetLimitsSpringSettings(axis)", +[](SixDOFConstraint &c, SixDOFConstraintSettings::EAxis axis) { return const_cast<SpringSettings *>(&c.GetLimitsSpringSettings(axis)); }, allow_raw_pointers())
        .function("SetLimitsSpringSettings(axis, settings)", &SixDOFConstraint::SetLimitsSpringSettings)
        .function("SetMaxFriction(axis, friction)", &SixDOFConstraint::SetMaxFriction)
        .function("GetMaxFriction(axis)", &SixDOFConstraint::GetMaxFriction)
        .out_function("GetRotationInConstraintSpace(out)", out_desc::Quat, +[](const SixDOFConstraint &c, uintptr_t out) { WriteQuat(c.GetRotationInConstraintSpace(), out); })
        .function("GetMotorSettings(axis)", +[](SixDOFConstraint &c, SixDOFConstraintSettings::EAxis axis) { return &c.GetMotorSettings(axis); }, allow_raw_pointers())
        .function("GetMotorState(axis)", &SixDOFConstraint::GetMotorState)
        .out_function("GetTargetVelocityCS(out)", out_desc::Vec3, +[](const SixDOFConstraint &c, uintptr_t out) { WriteVec3(c.GetTargetVelocityCS(), out); })
        .function("SetTargetVelocityCS(velocity)", &SixDOFConstraint::SetTargetVelocityCS)
        .function("SetTargetAngularVelocityCS(angularVelocity)", &SixDOFConstraint::SetTargetAngularVelocityCS)
        .out_function("GetTargetAngularVelocityCS(out)", out_desc::Vec3, +[](const SixDOFConstraint &c, uintptr_t out) { WriteVec3(c.GetTargetAngularVelocityCS(), out); })
        .out_function("GetTargetPositionCS(out)", out_desc::Vec3, +[](const SixDOFConstraint &c, uintptr_t out) { WriteVec3(c.GetTargetPositionCS(), out); })
        .out_function("GetTargetOrientationCS(out)", out_desc::Quat, +[](const SixDOFConstraint &c, uintptr_t out) { WriteQuat(c.GetTargetOrientationCS(), out); })
        .function("SetTargetOrientationBS(orientation)", &SixDOFConstraint::SetTargetOrientationBS)
        .out_function("GetTotalLambdaPosition(out)", out_desc::Vec3, +[](const SixDOFConstraint &c, uintptr_t out) { WriteVec3(c.GetTotalLambdaPosition(), out); })
        .out_function("GetTotalLambdaRotation(out)", out_desc::Vec3, +[](const SixDOFConstraint &c, uintptr_t out) { WriteVec3(c.GetTotalLambdaRotation(), out); })
        .out_function("GetTotalLambdaMotorTranslation(out)", out_desc::Vec3, +[](const SixDOFConstraint &c, uintptr_t out) { WriteVec3(c.GetTotalLambdaMotorTranslation(), out); })
        .out_function("GetTotalLambdaMotorRotation(out)", out_desc::Vec3, +[](const SixDOFConstraint &c, uintptr_t out) { WriteVec3(c.GetTotalLambdaMotorRotation(), out); });

    // -- Gear --
    jolt_class_<GearConstraintSettings, base<TwoBodyConstraintSettings>>("GearConstraintSettings")
        .smart_ptr<Ref<GearConstraintSettings>>("GearConstraintSettingsRef")
        .constructor(+[]() -> Ref<GearConstraintSettings> { return new GearConstraintSettings(); })
        .function("SetRatio(numTeethGear1, numTeethGear2)", &GearConstraintSettings::SetRatio)
        .property("mSpace", &GearConstraintSettings::mSpace)
        .property("mHingeAxis1", &GearConstraintSettings::mHingeAxis1)
        .property("mHingeAxis2", &GearConstraintSettings::mHingeAxis2)
        .property("mRatio", &GearConstraintSettings::mRatio);
    jolt_class_<GearConstraint, base<TwoBodyConstraint>>("GearConstraint")
        .smart_ptr<Ref<GearConstraint>>("GearConstraintRef")
        .function("SetConstraints(gear1, gear2)", &GearConstraint::SetConstraints, allow_raw_pointers())
        .function("GetTotalLambda", &GearConstraint::GetTotalLambda);

    // -- RackAndPinion --
    jolt_class_<RackAndPinionConstraintSettings, base<TwoBodyConstraintSettings>>("RackAndPinionConstraintSettings")
        .smart_ptr<Ref<RackAndPinionConstraintSettings>>("RackAndPinionConstraintSettingsRef")
        .constructor(+[]() -> Ref<RackAndPinionConstraintSettings> { return new RackAndPinionConstraintSettings(); })
        .function("SetRatio(numTeethRack, rackLength, numTeethPinion)", &RackAndPinionConstraintSettings::SetRatio)
        .property("mSpace", &RackAndPinionConstraintSettings::mSpace)
        .property("mHingeAxis", &RackAndPinionConstraintSettings::mHingeAxis)
        .property("mSliderAxis", &RackAndPinionConstraintSettings::mSliderAxis)
        .property("mRatio", &RackAndPinionConstraintSettings::mRatio);
    jolt_class_<RackAndPinionConstraint, base<TwoBodyConstraint>>("RackAndPinionConstraint")
        .smart_ptr<Ref<RackAndPinionConstraint>>("RackAndPinionConstraintRef")
        .function("SetConstraints(pinion, rack)", &RackAndPinionConstraint::SetConstraints, allow_raw_pointers())
        .function("GetTotalLambda", &RackAndPinionConstraint::GetTotalLambda);

    // -- Pulley --
    jolt_class_<PulleyConstraintSettings, base<TwoBodyConstraintSettings>>("PulleyConstraintSettings")
        .smart_ptr<Ref<PulleyConstraintSettings>>("PulleyConstraintSettingsRef")
        .constructor(+[]() -> Ref<PulleyConstraintSettings> { return new PulleyConstraintSettings(); })
        .property("mSpace", &PulleyConstraintSettings::mSpace)
        .property("mBodyPoint1", &PulleyConstraintSettings::mBodyPoint1)
        .property("mFixedPoint1", &PulleyConstraintSettings::mFixedPoint1)
        .property("mBodyPoint2", &PulleyConstraintSettings::mBodyPoint2)
        .property("mFixedPoint2", &PulleyConstraintSettings::mFixedPoint2)
        .property("mRatio", &PulleyConstraintSettings::mRatio)
        .property("mMinLength", &PulleyConstraintSettings::mMinLength)
        .property("mMaxLength", &PulleyConstraintSettings::mMaxLength);
    jolt_class_<PulleyConstraint, base<TwoBodyConstraint>>("PulleyConstraint")
        .smart_ptr<Ref<PulleyConstraint>>("PulleyConstraintRef")
        .function("SetLength(min, max)", &PulleyConstraint::SetLength)
        .function("GetCurrentLength", &PulleyConstraint::GetCurrentLength)
        .function("GetMinLength", &PulleyConstraint::GetMinLength)
        .function("GetMaxLength", &PulleyConstraint::GetMaxLength)
        .function("GetTotalLambdaPosition", &PulleyConstraint::GetTotalLambdaPosition);

    // -- Path (build a path, then constrain body 2 to follow it) --
    // PathConstraintPath is refcounted; a concrete Hermite path is the usual builder.
    jolt_class_<PathConstraintPath>("PathConstraintPath")
        .smart_ptr<Ref<PathConstraintPath>>("PathConstraintPathRef")
        .allow_subclass<PathConstraintPathWrapper>("PathConstraintPathWrapper")
        .function("GetPathMaxFraction", &PathConstraintPath::GetPathMaxFraction)
        .function("GetClosestPoint(position, fractionHint)", +[](const PathConstraintPath &p, const Vec3 &pos, float fractionHint) {
            return p.GetClosestPoint(pos, fractionHint); })
        .function("SetIsLooping(isLooping)", &PathConstraintPath::SetIsLooping)
        .function("IsLooping", &PathConstraintPath::IsLooping);
    jolt_class_<PathConstraintPathHermite, base<PathConstraintPath>>("PathConstraintPathHermite")
        .smart_ptr<Ref<PathConstraintPathHermite>>("PathConstraintPathHermiteRef")
        .constructor(+[]() -> Ref<PathConstraintPathHermite> { return new PathConstraintPathHermite(); })
        .function("AddPoint(position, tangent, normal)", &PathConstraintPathHermite::AddPoint);

    jolt_class_<PathConstraintSettings, base<TwoBodyConstraintSettings>>("PathConstraintSettings")
        .smart_ptr<Ref<PathConstraintSettings>>("PathConstraintSettingsRef")
        .constructor(+[]() -> Ref<PathConstraintSettings> { return new PathConstraintSettings(); })
        .property("mPathPosition", &PathConstraintSettings::mPathPosition)
        .property("mPathRotation", &PathConstraintSettings::mPathRotation)
        .property("mPathFraction", &PathConstraintSettings::mPathFraction)
        .property("mMaxFrictionForce", &PathConstraintSettings::mMaxFrictionForce)
        .property("mRotationConstraintType", &PathConstraintSettings::mRotationConstraintType)
        .function("SetPath(path, pathFraction)", +[](PathConstraintSettings &s, const PathConstraintPath *path, float fraction) {
            s.mPath = path; s.mPathFraction = fraction; }, allow_raw_pointers())
        .function("GetPositionMotorSettings", +[](PathConstraintSettings &s) { return &s.mPositionMotorSettings; }, allow_raw_pointers());
    jolt_class_<PathConstraint, base<TwoBodyConstraint>>("PathConstraint")
        .smart_ptr<Ref<PathConstraint>>("PathConstraintRef")
        .function("SetPath(path, pathFraction)", &PathConstraint::SetPath, allow_raw_pointers())
        .function("GetPathFraction", &PathConstraint::GetPathFraction)
        .function("SetMaxFrictionForce(frictionForce)", &PathConstraint::SetMaxFrictionForce)
        .function("GetMaxFrictionForce", &PathConstraint::GetMaxFrictionForce)
        .function("SetPositionMotorState(state)", &PathConstraint::SetPositionMotorState)
        .function("GetPositionMotorState", &PathConstraint::GetPositionMotorState)
        .function("SetTargetVelocity(velocity)", &PathConstraint::SetTargetVelocity)
        .function("GetTargetVelocity", &PathConstraint::GetTargetVelocity)
        .function("SetTargetPathFraction(fraction)", &PathConstraint::SetTargetPathFraction)
        .function("GetTargetPathFraction", &PathConstraint::GetTargetPathFraction)
        .function("GetPositionMotorSettings", +[](PathConstraint &c) { return &c.GetPositionMotorSettings(); }, allow_raw_pointers());

    // ---- collision layer configuration (concrete Table implementations) ----
    jolt_class_<BroadPhaseLayer>("BroadPhaseLayer")
        .constructor<BroadPhaseLayer::Type>("value");

    // Base interfaces — carry their query method(s) and are JS-subclassable so users can write
    // custom layer schemes (not just the Table/Mask impls). JS-subclass path is main-thread only.
    jolt_class_<BroadPhaseLayerInterface>("BroadPhaseLayerInterface")
        .function("GetNumBroadPhaseLayers", &BroadPhaseLayerInterface::GetNumBroadPhaseLayers)
        .allow_subclass<BroadPhaseLayerInterfaceWrapper>("BroadPhaseLayerInterfaceWrapper");
    jolt_class_<ObjectVsBroadPhaseLayerFilter>("ObjectVsBroadPhaseLayerFilter")
        .allow_subclass<ObjectVsBroadPhaseLayerFilterWrapper>("ObjectVsBroadPhaseLayerFilterWrapper");
    jolt_class_<ObjectLayerPairFilter>("ObjectLayerPairFilter")
        .allow_subclass<ObjectLayerPairFilterWrapper>("ObjectLayerPairFilterWrapper");

    jolt_class_<ObjectLayerPairFilterTable, base<ObjectLayerPairFilter>>("ObjectLayerPairFilterTable")
        .constructor<uint>("numObjectLayers")
        .function("GetNumObjectLayers", &ObjectLayerPairFilterTable::GetNumObjectLayers)
        .function("EnableCollision(layer1, layer2)", &ObjectLayerPairFilterTable::EnableCollision)
        .function("DisableCollision(layer1, layer2)", &ObjectLayerPairFilterTable::DisableCollision);
    jolt_class_<BroadPhaseLayerInterfaceTable, base<BroadPhaseLayerInterface>>("BroadPhaseLayerInterfaceTable")
        .constructor<uint, uint>("numObjectLayers, numBroadPhaseLayers")
        .function("MapObjectToBroadPhaseLayer(objectLayer, broadPhaseLayer)", &BroadPhaseLayerInterfaceTable::MapObjectToBroadPhaseLayer);
    jolt_class_<ObjectVsBroadPhaseLayerFilterTable, base<ObjectVsBroadPhaseLayerFilter>>("ObjectVsBroadPhaseLayerFilterTable")
        .constructor<BroadPhaseLayerInterface &, uint, ObjectLayerPairFilter &, uint>("broadPhaseLayerInterface, numBroadPhaseLayers, objectLayerPairFilter, numObjectLayers");

    // Mask-based collision filtering (alternative to the Table variants): an ObjectLayer
    // encodes a group bit + a collision mask via sGetObjectLayer(group, mask).
    jolt_class_<ObjectLayerPairFilterMask, base<ObjectLayerPairFilter>>("ObjectLayerPairFilterMask")
        .constructor<>()
        .class_function("sGetObjectLayer(group, mask)", &ObjectLayerPairFilterMask::sGetObjectLayer)
        .class_function("sGetGroup(objectLayer)", &ObjectLayerPairFilterMask::sGetGroup)
        .class_function("sGetMask(objectLayer)", &ObjectLayerPairFilterMask::sGetMask);
    jolt_class_<BroadPhaseLayerInterfaceMask, base<BroadPhaseLayerInterface>>("BroadPhaseLayerInterfaceMask")
        .constructor<uint>("numBroadPhaseLayers")
        .function("ConfigureLayer(broadPhaseLayer, groupsToInclude, groupsToExclude)", &BroadPhaseLayerInterfaceMask::ConfigureLayer);
    jolt_class_<ObjectVsBroadPhaseLayerFilterMask, base<ObjectVsBroadPhaseLayerFilter>>("ObjectVsBroadPhaseLayerFilterMask")
        .constructor<const BroadPhaseLayerInterfaceMask &>("broadPhaseLayerInterface");

    // ---- state serialization (PhysicsSystem.SaveState/RestoreState) ----
    jolt_class_<StateRecorder>("StateRecorder")   // abstract base
        .function("SetValidating(validating)", &StateRecorder::SetValidating)
        .function("IsValidating", &StateRecorder::IsValidating)
        .function("SetIsLastPart(isLastPart)", &StateRecorder::SetIsLastPart)
        .function("IsLastPart", &StateRecorder::IsLastPart);
    // JS-subclassable save/restore filter (select which bodies/constraints/contacts to persist).
    jolt_class_<StateRecorderFilter>("StateRecorderFilter")
        .allow_subclass<StateRecorderFilterWrapper>("StateRecorderFilterWrapper");
    jolt_class_<StateRecorderImpl, base<StateRecorder>>("StateRecorderImpl")
        .constructor<>()
        .function("Rewind", &StateRecorderImpl::Rewind)   // rewind to the start before restoring
        .function("Clear", &StateRecorderImpl::Clear)
        .function("IsEqual(reference)", +[](StateRecorderImpl &s, StateRecorderImpl &ref) { return s.IsEqual(ref); }, allow_raw_pointers());

    // ---- the real facade: JoltSettings + JoltInterface (from JoltJS.h) ----
    jolt_class_<JoltSettings>("JoltSettings")
        .constructor<>()
        .property("mMaxBodies", &JoltSettings::mMaxBodies)
        .property("mMaxBodyPairs", &JoltSettings::mMaxBodyPairs)
        .property("mMaxContactConstraints", &JoltSettings::mMaxContactConstraints)
        .property("mBroadPhaseLayerInterface", &JoltSettings::mBroadPhaseLayerInterface, allow_raw_pointers())
        .property("mObjectVsBroadPhaseLayerFilter", &JoltSettings::mObjectVsBroadPhaseLayerFilter, allow_raw_pointers())
        .property("mObjectLayerPairFilter", &JoltSettings::mObjectLayerPairFilter, allow_raw_pointers());

    jolt_class_<JoltInterface>("JoltInterface")
        .constructor<const JoltSettings &>("settings")
        .class_function("sGetFreeMemory", &JoltInterface::sGetFreeMemory)   // wasm heap free bytes (leak checking)
        .function("Step(deltaTime, collisionSteps)", &JoltInterface::Step)
        .function("GetPhysicsSystem", &JoltInterface::GetPhysicsSystem, allow_raw_pointers())
        // The collision-filter interfaces this JoltInterface owns — pass to DefaultObjectLayerFilter /
        // DefaultBroadPhaseLayerFilter to build the standard layer filters for a query.
        .function("GetObjectLayerPairFilter", &JoltInterface::GetObjectLayerPairFilter, allow_raw_pointers())
        .function("GetObjectVsBroadPhaseLayerFilter", &JoltInterface::GetObjectVsBroadPhaseLayerFilter, allow_raw_pointers());

    // ==== characters =========================================================
    // CharacterVirtual: kinematic, fully script-driven (the usual game character).
    // Character: rigid-body-backed. Movement calls hide Jolt's filter/allocator
    // plumbing — pass the object layer to collide against + the JoltInterface
    // (source of the default broadphase/layer filters and the temp allocator).
    jolt_class_<CharacterContactSettings>("CharacterContactSettings")
        .property("mCanPushCharacter", &CharacterContactSettings::mCanPushCharacter)
        .property("mCanReceiveImpulses", &CharacterContactSettings::mCanReceiveImpulses);
    jolt_class_<CharacterContactListener>("CharacterContactListener")
        .allow_subclass<CharacterContactListenerWrapper>("CharacterContactListenerWrapper");

    // CharacterID: value-type wrapper around a uint32. Used to identify (deleted) characters.
    jolt_class_<CharacterID>("CharacterID")
        .constructor<>()
        .function("GetValue", &CharacterID::GetValue)
        .function("IsInvalid", &CharacterID::IsInvalid)
        .class_function("sSetNextCharacterID(nextValue)", +[](uint32 v) { CharacterID::sSetNextCharacterID(v); })
        .class_function("sNextCharacterID", +[]() { return CharacterID::sNextCharacterID(); });

    // Abstract char-vs-char collision interface (JS-subclassable via the wrapper).
    jolt_class_<CharacterVsCharacterCollision>("CharacterVsCharacterCollision")
        .allow_subclass<CharacterVsCharacterCollisionWrapper>("CharacterVsCharacterCollisionWrapper");
    // Brute-force built-in char-vs-char collision. Add/Remove your CharacterVirtuals, then set it
    // on each character via SetCharacterVsCharacterCollision.
    jolt_class_<CharacterVsCharacterCollisionSimple, base<CharacterVsCharacterCollision>>("CharacterVsCharacterCollisionSimple")
        .constructor<>()
        .function("Add(character)", &CharacterVsCharacterCollisionSimple::Add, allow_raw_pointers())
        .function("Remove(character)", &CharacterVsCharacterCollisionSimple::Remove, allow_raw_pointers());

    // A single contact from CharacterVirtual::GetActiveContacts(). Read-only snapshot; the
    // handle is valid only until the next character Update. (Jolt: CharacterVirtual::Contact.)
    jolt_class_<CharacterVirtual::Contact>("CharacterVirtualContact")
        .function("IsSameBody(other)", +[](const CharacterVirtual::Contact &a, const CharacterVirtual::Contact &b) { return a.IsSameBody(b); })
        .out_function("GetPosition(out)",       out_desc::Vec3, +[](const CharacterVirtual::Contact &c, uintptr_t out) { WriteVec3(c.mPosition, out); })
        .out_function("GetLinearVelocity(out)", out_desc::Vec3, +[](const CharacterVirtual::Contact &c, uintptr_t out) { WriteVec3(c.mLinearVelocity, out); })
        .out_function("GetContactNormal(out)",  out_desc::Vec3, +[](const CharacterVirtual::Contact &c, uintptr_t out) { WriteVec3(c.mContactNormal, out); })
        .out_function("GetSurfaceNormal(out)",  out_desc::Vec3, +[](const CharacterVirtual::Contact &c, uintptr_t out) { WriteVec3(c.mSurfaceNormal, out); })
        .property("mDistance", &CharacterVirtual::Contact::mDistance)
        .property("mFraction", &CharacterVirtual::Contact::mFraction)
        .function("GetBodyB",        +[](const CharacterVirtual::Contact &c) { return fromBodyID(c.mBodyB); })
        .function("GetCharacterIDB", +[](const CharacterVirtual::Contact &c) { return (uint32)c.mCharacterIDB.GetValue(); })
        .function("GetSubShapeIDB",  +[](const CharacterVirtual::Contact &c) { return (uint32)c.mSubShapeIDB.GetValue(); })
        .property("mMotionTypeB", &CharacterVirtual::Contact::mMotionTypeB)
        .property("mIsSensorB",   &CharacterVirtual::Contact::mIsSensorB)
        // mCharacterB may dangle when read via GetActiveContacts() — prefer GetCharacterIDB.
        .function("GetCharacterB", +[](const CharacterVirtual::Contact &c) { return const_cast<CharacterVirtual *>(c.mCharacterB); }, allow_raw_pointers())
        .function("GetUserData", +[](const CharacterVirtual::Contact &c) { return (uint32)c.mUserData; })
        .function("GetMaterial", +[](const CharacterVirtual::Contact &c) { return const_cast<PhysicsMaterial *>(c.mMaterial); }, allow_raw_pointers())
        .property("mHadCollision",     &CharacterVirtual::Contact::mHadCollision)
        .property("mWasDiscarded",     &CharacterVirtual::Contact::mWasDiscarded)
        .property("mCanPushCharacter", &CharacterVirtual::Contact::mCanPushCharacter);

    // Read-only view over CharacterVirtual::ContactList. at() returns a non-owning handle so the
    // large Contact struct isn't copied.
    class_<CharacterVirtual::ContactList>("ArrayCharacterVirtualContact")
        .constructor<>()
        .function("empty", +[](const CharacterVirtual::ContactList &a) { return a.empty(); })
        .function("size",  +[](const CharacterVirtual::ContactList &a) { return (uint32)a.size(); })
        .function("at(index)", +[](CharacterVirtual::ContactList &a, uint32 i) { return &a[i]; }, allow_raw_pointers());

    jolt_class_<CharacterBaseSettings>("CharacterBaseSettings")   // abstract base (config)
        .property("mUp", &CharacterBaseSettings::mUp)
        .property("mMaxSlopeAngle", &CharacterBaseSettings::mMaxSlopeAngle)
        .property("mEnhancedInternalEdgeRemoval", &CharacterBaseSettings::mEnhancedInternalEdgeRemoval)
        .function("SetShape(shape)", +[](CharacterBaseSettings &s, const Shape *sh) { s.mShape = sh; }, allow_raw_pointers())
        .function("SetSupportingVolume(normal, constant)", +[](CharacterBaseSettings &s, Vec3 n, float c) { s.mSupportingVolume = Plane(n, c); });
    jolt_class_<CharacterVirtualSettings, base<CharacterBaseSettings>>("CharacterVirtualSettings")
        .constructor<>()
        .property("mMass", &CharacterVirtualSettings::mMass)
        .property("mMaxStrength", &CharacterVirtualSettings::mMaxStrength)
        .property("mShapeOffset", &CharacterVirtualSettings::mShapeOffset)
        .property("mBackFaceMode", &CharacterVirtualSettings::mBackFaceMode)
        .property("mPredictiveContactDistance", &CharacterVirtualSettings::mPredictiveContactDistance)
        .property("mMaxCollisionIterations", &CharacterVirtualSettings::mMaxCollisionIterations)
        .property("mMaxConstraintIterations", &CharacterVirtualSettings::mMaxConstraintIterations)
        .property("mCharacterPadding", &CharacterVirtualSettings::mCharacterPadding)
        .property("mMaxNumHits", &CharacterVirtualSettings::mMaxNumHits)
        .property("mHitReductionCosMaxAngle", &CharacterVirtualSettings::mHitReductionCosMaxAngle)
        .property("mPenetrationRecoverySpeed", &CharacterVirtualSettings::mPenetrationRecoverySpeed)
        .property("mInnerBodyLayer", &CharacterVirtualSettings::mInnerBodyLayer)
        .function("SetInnerBodyShape(shape)", +[](CharacterVirtualSettings &s, const Shape *sh) { s.mInnerBodyShape = sh; }, allow_raw_pointers())
        .property("mID", &CharacterVirtualSettings::mID)
        .property("mMinTimeRemaining", &CharacterVirtualSettings::mMinTimeRemaining)
        .property("mCollisionTolerance", &CharacterVirtualSettings::mCollisionTolerance)
        .function("GetInnerBodyIDOverride", +[](const CharacterVirtualSettings &s) { return fromBodyID(s.mInnerBodyIDOverride); })
        .function("SetInnerBodyIDOverride(bodyID)", +[](CharacterVirtualSettings &s, uint32 id) { s.mInnerBodyIDOverride = toBodyID(id); });
    jolt_class_<CharacterVirtual::ExtendedUpdateSettings>("ExtendedUpdateSettings")
        .constructor<>()
        .property("mStickToFloorStepDown", &CharacterVirtual::ExtendedUpdateSettings::mStickToFloorStepDown)
        .property("mWalkStairsStepUp", &CharacterVirtual::ExtendedUpdateSettings::mWalkStairsStepUp)
        .property("mWalkStairsMinStepForward", &CharacterVirtual::ExtendedUpdateSettings::mWalkStairsMinStepForward)
        .property("mWalkStairsStepForwardTest", &CharacterVirtual::ExtendedUpdateSettings::mWalkStairsStepForwardTest)
        .property("mWalkStairsCosAngleForwardContact", &CharacterVirtual::ExtendedUpdateSettings::mWalkStairsCosAngleForwardContact)
        .property("mWalkStairsStepDownExtra", &CharacterVirtual::ExtendedUpdateSettings::mWalkStairsStepDownExtra);

    // refcounted base — the getters common to both character kinds
    jolt_class_<CharacterBase>("CharacterBase")
        .smart_ptr<Ref<CharacterBase>>("CharacterBaseRef")
        .function("GetCosMaxSlopeAngle", &CharacterBase::GetCosMaxSlopeAngle)
        .function("SetMaxSlopeAngle(maxSlopeAngle)", &CharacterBase::SetMaxSlopeAngle)
        .function("SetUp(up)", &CharacterBase::SetUp)
        .out_function("GetUp(out)", out_desc::Vec3, +[](const CharacterBase &s, uintptr_t out) { WriteVec3(s.GetUp(), out); })
        .function("IsSlopeTooSteep(normal)", &CharacterBase::IsSlopeTooSteep)
        .function("GetShape", +[](const CharacterBase &c) { return const_cast<Shape *>(c.GetShape()); }, allow_raw_pointers())
        .function("GetGroundState", &CharacterBase::GetGroundState)
        .function("IsSupported", &CharacterBase::IsSupported)
        .out_function("GetGroundPosition(out)", out_desc::Vec3, +[](const CharacterBase &s, uintptr_t out) { WriteVec3(s.GetGroundPosition(), out); })   // RVec3 == Vec3 (single precision)
        .out_function("GetGroundNormal(out)", out_desc::Vec3, +[](const CharacterBase &s, uintptr_t out) { WriteVec3(s.GetGroundNormal(), out); })
        .out_function("GetGroundVelocity(out)", out_desc::Vec3, +[](const CharacterBase &s, uintptr_t out) { WriteVec3(s.GetGroundVelocity(), out); })
        .function("GetGroundBodyID", +[](const CharacterBase &c) { return (uint32)c.GetGroundBodyID().GetIndexAndSequenceNumber(); })
        .function("GetGroundMaterial", +[](const CharacterBase &c) { return const_cast<PhysicsMaterial *>(c.GetGroundMaterial()); }, allow_raw_pointers())
        .function("GetGroundSubShapeID", +[](const CharacterBase &c) { return (uint32)c.GetGroundSubShapeID().GetValue(); })
        .function("GetGroundUserData", +[](const CharacterBase &c) { return (uint32)c.GetGroundUserData(); })
        .function("SaveState(stream)", +[](const CharacterBase &c, StateRecorder &s) { c.SaveState(s); }, allow_raw_pointers())
        .function("RestoreState(stream)", +[](CharacterBase &c, StateRecorder &s) { c.RestoreState(s); }, allow_raw_pointers());

    jolt_class_<CharacterVirtual, base<CharacterBase>>("CharacterVirtual")
        .smart_ptr<Ref<CharacterVirtual>>("CharacterVirtualRef")
        .constructor("settings, position, rotation, physicsSystem", +[](const CharacterVirtualSettings *s, RVec3 pos, Quat rot, PhysicsSystem *ps) -> Ref<CharacterVirtual> {
            return new CharacterVirtual(s, pos, rot, ps); }, allow_raw_pointers())
        .function("SetListener(listener)", &CharacterVirtual::SetListener, allow_raw_pointers())
        .out_function("GetPosition(out)", out_desc::Vec3, +[](const CharacterVirtual &s, uintptr_t out) { WriteVec3(s.GetPosition(), out); })
        .out_function("GetRotation(out)", out_desc::Quat, +[](const CharacterVirtual &s, uintptr_t out) { WriteQuat(s.GetRotation(), out); })
        .out_function("GetLinearVelocity(out)", out_desc::Vec3, +[](const CharacterVirtual &s, uintptr_t out) { WriteVec3(s.GetLinearVelocity(), out); })
        .out_function("GetCenterOfMassPosition(out)", out_desc::Vec3, +[](const CharacterVirtual &s, uintptr_t out) { WriteVec3(s.GetCenterOfMassPosition(), out); })
        .out_function("GetWorldTransform(out)", out_desc::Mat44, +[](const CharacterVirtual &s, uintptr_t out) { auto m = s.GetWorldTransform(); WriteMat4(m, out); })
        .out_function("GetCenterOfMassTransform(out)", out_desc::Mat44, +[](const CharacterVirtual &s, uintptr_t out) { auto m = s.GetCenterOfMassTransform(); WriteMat4(m, out); })
        .function("SetPosition(position)", &CharacterVirtual::SetPosition)
        .function("SetRotation(rotation)", &CharacterVirtual::SetRotation)
        .function("SetLinearVelocity(velocity)", &CharacterVirtual::SetLinearVelocity)
        .function("GetMass", &CharacterVirtual::GetMass)
        .function("SetMass(mass)", &CharacterVirtual::SetMass)
        .function("GetMaxStrength", &CharacterVirtual::GetMaxStrength)
        .function("SetMaxStrength(maxStrength)", &CharacterVirtual::SetMaxStrength)
        .function("GetPenetrationRecoverySpeed", &CharacterVirtual::GetPenetrationRecoverySpeed)
        .function("SetPenetrationRecoverySpeed(speed)", &CharacterVirtual::SetPenetrationRecoverySpeed)
        .function("GetCharacterPadding", &CharacterVirtual::GetCharacterPadding)
        .function("UpdateGroundVelocity", &CharacterVirtual::UpdateGroundVelocity)
        .function("CanWalkStairs(linearVelocity)", &CharacterVirtual::CanWalkStairs)
        .function("GetInnerBodyID", +[](const CharacterVirtual &c) { return (uint32)c.GetInnerBodyID().GetIndexAndSequenceNumber(); })
        .function("Update(deltaTime, gravity, objectLayer, jolt)",
            +[](CharacterVirtual &c, float dt, Vec3 g, uint16 layer, JoltInterface &jolt) {
                PhysicsSystem *ps = jolt.GetPhysicsSystem();
                c.Update(dt, g, ps->GetDefaultBroadPhaseLayerFilter(layer), ps->GetDefaultLayerFilter(layer),
                    BodyFilter(), ShapeFilter(), *jolt.GetTempAllocator()); }, allow_raw_pointers())
        .function("ExtendedUpdate(deltaTime, gravity, updateSettings, objectLayer, jolt)",
            +[](CharacterVirtual &c, float dt, Vec3 g, const CharacterVirtual::ExtendedUpdateSettings &us, uint16 layer, JoltInterface &jolt) {
                PhysicsSystem *ps = jolt.GetPhysicsSystem();
                c.ExtendedUpdate(dt, g, us, ps->GetDefaultBroadPhaseLayerFilter(layer), ps->GetDefaultLayerFilter(layer),
                    BodyFilter(), ShapeFilter(), *jolt.GetTempAllocator()); }, allow_raw_pointers())
        // Filtered variants: pass explicit filters (e.g. an IgnoreSingleBodyFilter so the character
        // skips a carried/ridden body) instead of the default all-bodies filter for its layer.
        .function("UpdateWithFilters(deltaTime, gravity, broadPhaseFilter, objectLayerFilter, bodyFilter, shapeFilter, jolt)",
            +[](CharacterVirtual &c, float dt, Vec3 g, const BroadPhaseLayerFilter &bpf, const ObjectLayerFilter &olf,
                const BodyFilter &bf, const ShapeFilter &sf, JoltInterface &jolt) {
                c.Update(dt, g, bpf, olf, bf, sf, *jolt.GetTempAllocator()); }, allow_raw_pointers())
        .function("ExtendedUpdateWithFilters(deltaTime, gravity, updateSettings, broadPhaseFilter, objectLayerFilter, bodyFilter, shapeFilter, jolt)",
            +[](CharacterVirtual &c, float dt, Vec3 g, const CharacterVirtual::ExtendedUpdateSettings &us,
                const BroadPhaseLayerFilter &bpf, const ObjectLayerFilter &olf, const BodyFilter &bf, const ShapeFilter &sf, JoltInterface &jolt) {
                c.ExtendedUpdate(dt, g, us, bpf, olf, bf, sf, *jolt.GetTempAllocator()); }, allow_raw_pointers())
        .function("SetShape(shape, maxPenetrationDepth, objectLayer, jolt)",
            +[](CharacterVirtual &c, const Shape *sh, float maxPen, uint16 layer, JoltInterface &jolt) -> bool {
                PhysicsSystem *ps = jolt.GetPhysicsSystem();
                return c.SetShape(sh, maxPen, ps->GetDefaultBroadPhaseLayerFilter(layer), ps->GetDefaultLayerFilter(layer),
                    BodyFilter(), ShapeFilter(), *jolt.GetTempAllocator()); }, allow_raw_pointers())
        .function("RefreshContacts(objectLayer, jolt)",
            +[](CharacterVirtual &c, uint16 layer, JoltInterface &jolt) {
                PhysicsSystem *ps = jolt.GetPhysicsSystem();
                c.RefreshContacts(ps->GetDefaultBroadPhaseLayerFilter(layer), ps->GetDefaultLayerFilter(layer),
                    BodyFilter(), ShapeFilter(), *jolt.GetTempAllocator()); }, allow_raw_pointers())
        // identity / listeners
        .function("GetID", +[](const CharacterVirtual &c) { return c.GetID(); })
        .function("GetListener", &CharacterVirtual::GetListener, allow_raw_pointers())
        .function("SetCharacterVsCharacterCollision(collision)", &CharacterVirtual::SetCharacterVsCharacterCollision, allow_raw_pointers())
        // user data
        .function("GetUserData", +[](const CharacterVirtual &c) { return (uint32)c.GetUserData(); })
        .function("SetUserData(userData)", +[](CharacterVirtual &c, uint32 v) { c.SetUserData(v); })
        // shape offset / hit tuning
        .out_function("GetShapeOffset(out)", out_desc::Vec3, +[](const CharacterVirtual &c, uintptr_t out) { WriteVec3(c.GetShapeOffset(), out); })
        .function("SetShapeOffset(shapeOffset)", &CharacterVirtual::SetShapeOffset)
        .function("GetMaxNumHits", &CharacterVirtual::GetMaxNumHits)
        .function("SetMaxNumHits(maxHits)", &CharacterVirtual::SetMaxNumHits)
        .function("GetHitReductionCosMaxAngle", &CharacterVirtual::GetHitReductionCosMaxAngle)
        .function("SetHitReductionCosMaxAngle(cosMaxAngle)", &CharacterVirtual::SetHitReductionCosMaxAngle)
        .function("GetMaxHitsExceeded", &CharacterVirtual::GetMaxHitsExceeded)
        .function("GetEnhancedInternalEdgeRemoval", &CharacterVirtual::GetEnhancedInternalEdgeRemoval)
        .function("SetEnhancedInternalEdgeRemoval(apply)", &CharacterVirtual::SetEnhancedInternalEdgeRemoval)
        // inner body / transformed shape
        .function("SetInnerBodyShape(shape)", +[](CharacterVirtual &c, const Shape *s) { c.SetInnerBodyShape(s); }, allow_raw_pointers())
        .function("GetTransformedShape", +[](const CharacterVirtual &c) { return c.GetTransformedShape(); })
        // velocity helper
        .out_function("CancelVelocityTowardsSteepSlopes(out, desiredVelocity)",
            out_desc::Vec3, out_desc::Pass, +[](const CharacterVirtual &c, uintptr_t out, Vec3 v) { WriteVec3(c.CancelVelocityTowardsSteepSlopes(v), out); })
        // contact-change tracking
        .function("StartTrackingContactChanges", &CharacterVirtual::StartTrackingContactChanges)
        .function("FinishTrackingContactChanges", &CharacterVirtual::FinishTrackingContactChanges)
        // collided-with queries
        .function("HasCollidedWithBody(bodyID)", +[](const CharacterVirtual &c, uint32 id) { return c.HasCollidedWith(toBodyID(id)); })
        .function("HasCollidedWithCharacterID(characterID)", +[](const CharacterVirtual &c, const CharacterID &id) { return c.HasCollidedWith(id); })
        .function("HasCollidedWithCharacter(character)", +[](const CharacterVirtual &c, const CharacterVirtual *o) { return c.HasCollidedWith(o); }, allow_raw_pointers())
        // active contacts (non-owning handle; valid until next Update)
        .function("GetActiveContacts", +[](const CharacterVirtual &c) { return const_cast<CharacterVirtual::ContactList *>(&c.GetActiveContacts()); }, allow_raw_pointers())
        // filtered StickToFloor / WalkStairs
        .function("StickToFloor(stepDown, broadPhaseFilter, objectLayerFilter, bodyFilter, shapeFilter, jolt)",
            +[](CharacterVirtual &c, Vec3 stepDown, const BroadPhaseLayerFilter &bpf, const ObjectLayerFilter &olf,
                const BodyFilter &bf, const ShapeFilter &sf, JoltInterface &jolt) -> bool {
                return c.StickToFloor(stepDown, bpf, olf, bf, sf, *jolt.GetTempAllocator()); }, allow_raw_pointers())
        .function("WalkStairs(deltaTime, stepUp, stepForward, stepForwardTest, stepDownExtra, broadPhaseFilter, objectLayerFilter, bodyFilter, shapeFilter, jolt)",
            +[](CharacterVirtual &c, float dt, Vec3 up, Vec3 fwd, Vec3 fwdTest, Vec3 downExtra,
                const BroadPhaseLayerFilter &bpf, const ObjectLayerFilter &olf, const BodyFilter &bf,
                const ShapeFilter &sf, JoltInterface &jolt) -> bool {
                return c.WalkStairs(dt, up, fwd, fwdTest, downExtra, bpf, olf, bf, sf, *jolt.GetTempAllocator()); }, allow_raw_pointers());

    jolt_class_<CharacterSettings, base<CharacterBaseSettings>>("CharacterSettings")
        .constructor<>()
        .property("mLayer", &CharacterSettings::mLayer)
        .property("mMass", &CharacterSettings::mMass)
        .property("mFriction", &CharacterSettings::mFriction)
        .property("mGravityFactor", &CharacterSettings::mGravityFactor);
    jolt_class_<Character, base<CharacterBase>>("Character")
        .smart_ptr<Ref<Character>>("CharacterRef")
        .constructor("settings, position, rotation, userData, physicsSystem", +[](const CharacterSettings *s, RVec3 pos, Quat rot, uint64 userData, PhysicsSystem *ps) -> Ref<Character> {
            return new Character(s, pos, rot, userData, ps); }, allow_raw_pointers())
        .function("AddToPhysicsSystem(activationMode)", +[](Character &c, EActivation a) { c.AddToPhysicsSystem(a); })
        .function("RemoveFromPhysicsSystem", +[](Character &c) { c.RemoveFromPhysicsSystem(); })
        .function("Activate", +[](Character &c) { c.Activate(); })
        .function("PostSimulation(maxSeparationDistance)", +[](Character &c, float d) { c.PostSimulation(d); })
        .function("SetLinearVelocity(velocity)", +[](Character &c, Vec3 v) { c.SetLinearVelocity(v); })
        .function("AddLinearVelocity(velocity)", +[](Character &c, Vec3 v) { c.AddLinearVelocity(v); })
        .function("AddImpulse(impulse)", +[](Character &c, Vec3 v) { c.AddImpulse(v); })
        .out_function("GetLinearVelocity(out)", out_desc::Vec3, +[](const Character &s, uintptr_t out) { WriteVec3(s.GetLinearVelocity(), out); })
        .out_function("GetPosition(out)", out_desc::Vec3, +[](const Character &s, uintptr_t out) { WriteVec3(s.GetPosition(), out); })
        .out_function("GetRotation(out)", out_desc::Quat, +[](const Character &s, uintptr_t out) { WriteQuat(s.GetRotation(), out); })
        .out_function("GetCenterOfMassPosition(out)", out_desc::Vec3, +[](const Character &s, uintptr_t out) { WriteVec3(s.GetCenterOfMassPosition(), out); })
        .out_function("GetWorldTransform(out)", out_desc::Mat44, +[](const Character &s, uintptr_t out) { auto m = s.GetWorldTransform(); WriteMat4(m, out); })
        .function("SetPosition(position)", +[](Character &c, RVec3 p) { c.SetPosition(p); })
        .function("SetRotation(rotation)", +[](Character &c, Quat q) { c.SetRotation(q); })
        .function("GetBodyID", +[](const Character &c) { return (uint32)c.GetBodyID().GetIndexAndSequenceNumber(); })
        .function("GetLayer", &Character::GetLayer)
        .function("SetLayer(layer)", +[](Character &c, uint16 layer) { c.SetLayer(layer); })
        .function("SetLinearAndAngularVelocity(linearVelocity, angularVelocity)",
            +[](Character &c, Vec3 lv, Vec3 av) { c.SetLinearAndAngularVelocity(lv, av); })
        .function("GetTransformedShape", +[](const Character &c) { return c.GetTransformedShape(); })
        .function("SetShape(shape, maxPenetrationDepth)",
            +[](Character &c, const Shape *s, float maxPen) { return c.SetShape(s, maxPen); }, allow_raw_pointers())
        .function("CheckCollision(position, rotation, movementDirection, maxSeparationDistance, shape, baseOffset, collector)",
            +[](const Character &c, RVec3 pos, Quat rot, Vec3 dir, float maxSep, const Shape *s, RVec3 baseOffset, CollideShapeCollector &collector) {
                c.CheckCollision(pos, rot, dir, maxSep, s, baseOffset, collector); }, allow_raw_pointers());

    // ==== ragdoll + skeleton =================================================
    // Assemble programmatically: build a Skeleton (AddJoint), then RagdollSettings
    // whose mParts[i] (each a BodyCreationSettings) map 1:1 to joints, each with a
    // mToParent constraint; Stabilize() -> CreateRagdoll(groupID, userData, system).
    jolt_class_<Skeleton>("Skeleton")
        .smart_ptr<Ref<Skeleton>>("SkeletonRef")
        .constructor(+[]() -> Ref<Skeleton> { return new Skeleton(); })
        .function("AddJoint(name, parentIndex)", +[](Skeleton &s, const std::string &name, int parent) { return s.AddJoint(name, parent); })
        .function("AddRootJoint(name)", +[](Skeleton &s, const std::string &name) { return s.AddJoint(name, -1); })
        .function("GetJointCount", &Skeleton::GetJointCount)
        .function("GetJointIndex(name)", +[](const Skeleton &s, const std::string &name) { return s.GetJointIndex(name); })
        .function("GetJointName(index)", +[](const Skeleton &s, int i) { return std::string(s.GetJoint(i).mName); })
        .function("CalculateParentJointIndices", &Skeleton::CalculateParentJointIndices)
        .function("AreJointsCorrectlyOrdered", &Skeleton::AreJointsCorrectlyOrdered)
        .function("GetJointParentIndex(index)", +[](const Skeleton &s, int i) { return s.GetJoint(i).mParentJointIndex; });

    // pose target for SetPose / DriveToPoseUsingMotors (local joint states -> matrices)
    jolt_class_<SkeletalAnimationJointState>("JointState")
        .property("mRotation", &SkeletalAnimationJointState::mRotation)
        .property("mTranslation", &SkeletalAnimationJointState::mTranslation);

    jolt_class_<SkeletalAnimationKeyframe, base<SkeletalAnimationJointState>>("SkeletalAnimationKeyframe")
        .property("mTime", &SkeletalAnimationKeyframe::mTime);

    jolt_class_<SkeletalAnimationAnimatedJoint>("AnimatedJoint")
        .property("mJointName",
            +[](const SkeletalAnimationAnimatedJoint &j) -> std::string { return std::string(j.mJointName.c_str()); },
            +[](SkeletalAnimationAnimatedJoint &j, const std::string &n) { j.mJointName = n.c_str(); })
        .function("GetKeyframeCount", +[](const SkeletalAnimationAnimatedJoint &j) { return (uint32)j.mKeyframes.size(); })
        .function("ResizeKeyframes(count)", +[](SkeletalAnimationAnimatedJoint &j, uint32 n) { j.mKeyframes.resize(n); })
        .function("GetKeyframe(index)", +[](SkeletalAnimationAnimatedJoint &j, uint32 i) { return &j.mKeyframes[i]; }, allow_raw_pointers());

    jolt_class_<SkeletalAnimation>("SkeletalAnimation")
        .smart_ptr<Ref<SkeletalAnimation>>("SkeletalAnimationRef")
        .constructor(+[]() -> Ref<SkeletalAnimation> { return new SkeletalAnimation(); })
        .function("GetAnimatedJointCount", +[](const SkeletalAnimation &a) { return (uint32)a.GetAnimatedJoints().size(); })
        .function("ResizeAnimatedJoints(count)", +[](SkeletalAnimation &a, uint32 n) { a.GetAnimatedJoints().resize(n); })
        .function("GetAnimatedJoint(index)", +[](SkeletalAnimation &a, uint32 i) { return &a.GetAnimatedJoints()[i]; }, allow_raw_pointers())
        .function("Sample(time, pose)", +[](const SkeletalAnimation &a, float t, SkeletonPose &p) { a.Sample(t, p); })
        .function("GetDuration", &SkeletalAnimation::GetDuration)
        .function("SetIsLooping(looping)", &SkeletalAnimation::SetIsLooping)
        .function("IsLooping", &SkeletalAnimation::IsLooping);
    jolt_class_<SkeletonPose>("SkeletonPose")
        .constructor<>()
        .function("SetSkeleton(skeleton)", &SkeletonPose::SetSkeleton, allow_raw_pointers())
        .function("GetJointCount", &SkeletonPose::GetJointCount)
        .function("SetRootOffset(offset)", &SkeletonPose::SetRootOffset)
        .out_function("GetRootOffset(out)", out_desc::Vec3, +[](const SkeletonPose &s, uintptr_t out) { WriteVec3(s.GetRootOffset(), out); })
        .function("GetJoint(index)", +[](SkeletonPose &p, int i) { return &p.GetJoint(i); }, allow_raw_pointers())
        .function("CalculateJointMatrices", &SkeletonPose::CalculateJointMatrices)
        .function("CalculateJointStates", &SkeletonPose::CalculateJointStates)
        .out_function("GetJointMatrix(out, index)",
            out_desc::Mat44, out_desc::Pass, +[](const SkeletonPose &p, uintptr_t out, int i) { WriteMat4(p.GetJointMatrix(i), out); })
        .function("GetSkeleton", +[](const SkeletonPose &p) { return const_cast<Skeleton *>(p.GetSkeleton()); }, allow_raw_pointers())
        .function("GetJointMatricesCount", +[](const SkeletonPose &p) { return (uint32)p.GetJointMatrices().size(); })
        .function("SetJointMatrix(index, matrix)",
            +[](SkeletonPose &p, int i, const Mat44 &m) { p.GetJointMatrix(i) = m; });

    // Non parent/child constraint between two ragdoll bodies (by body index).
    jolt_class_<RagdollSettings::AdditionalConstraint>("RagdollAdditionalConstraint")
        .constructor<>()
        .function("GetBodyIdx(index)",  +[](const RagdollSettings::AdditionalConstraint &a, int i) { return a.mBodyIdx[i]; })
        .function("SetBodyIdx(index, value)",  +[](RagdollSettings::AdditionalConstraint &a, int i, int v) { a.mBodyIdx[i] = v; })
        .function("SetConstraint(constraintSettings)",
            +[](RagdollSettings::AdditionalConstraint &a, TwoBodyConstraintSettings *c) { a.mConstraint = c; }, allow_raw_pointers())
        .function("GetConstraint", +[](RagdollSettings::AdditionalConstraint &a) { return a.mConstraint.GetPtr(); }, allow_raw_pointers());

    // a ragdoll part IS a BodyCreationSettings + a constraint to its parent
    jolt_class_<RagdollSettings::Part, base<BodyCreationSettings>>("RagdollPart")
        .function("SetShape(shape)", +[](RagdollSettings::Part &p, const Shape *s) { p.SetShape(s); }, allow_raw_pointers())
        .function("SetToParent(constraintSettings)",
            +[](RagdollSettings::Part &p, TwoBodyConstraintSettings *c) { p.mToParent = c; }, allow_raw_pointers());
    jolt_class_<RagdollSettings>("RagdollSettings")
        .smart_ptr<Ref<RagdollSettings>>("RagdollSettingsRef")
        .constructor(+[]() -> Ref<RagdollSettings> { return new RagdollSettings(); })
        .function("SetSkeleton(skeleton)", +[](RagdollSettings &s, Skeleton *sk) { s.mSkeleton = sk; }, allow_raw_pointers())
        .function("GetSkeleton", +[](RagdollSettings &s) { return s.mSkeleton.GetPtr(); }, allow_raw_pointers())
        .function("ResizeParts(count)", +[](RagdollSettings &s, int n) { s.mParts.resize(n); })
        .function("GetPart(index)", +[](RagdollSettings &s, int i) { return &s.mParts[i]; }, allow_raw_pointers())
        .function("Stabilize", &RagdollSettings::Stabilize)
        .function("DisableParentChildCollisions", +[](RagdollSettings &s) { s.DisableParentChildCollisions(); })
        .function("CalculateBodyIndexToConstraintIndex", &RagdollSettings::CalculateBodyIndexToConstraintIndex)
        .function("CalculateConstraintIndexToBodyIdxPair", &RagdollSettings::CalculateConstraintIndexToBodyIdxPair)
        .function("CreateRagdoll(collisionGroup, userData, physicsSystem)",
            +[](const RagdollSettings &s, uint32 group, uint64 userData, PhysicsSystem *ps) -> Ref<Ragdoll> {
                return Ref<Ragdoll>(s.CreateRagdoll(group, userData, ps)); }, allow_raw_pointers())
        .function("CalculateConstraintPriorities", +[](RagdollSettings &s) { s.CalculateConstraintPriorities(); })
        .function("GetConstraintIndexForBodyIndex(bodyIndex)", &RagdollSettings::GetConstraintIndexForBodyIndex)
        .function("GetAdditionalConstraintCount", +[](const RagdollSettings &s) { return (uint32)s.mAdditionalConstraints.size(); })
        .function("ResizeAdditionalConstraints(count)", +[](RagdollSettings &s, uint32 n) { s.mAdditionalConstraints.resize(n); })
        .function("GetAdditionalConstraint(index)", +[](RagdollSettings &s, uint32 i) { return &s.mAdditionalConstraints[i]; }, allow_raw_pointers())
        .function("AddAdditionalConstraint(bodyIdx1, bodyIdx2, constraintSettings)",
            +[](RagdollSettings &s, int b1, int b2, TwoBodyConstraintSettings *c) {
                s.mAdditionalConstraints.push_back(RagdollSettings::AdditionalConstraint(b1, b2, c)); }, allow_raw_pointers());

    jolt_class_<Ragdoll>("Ragdoll")
        .smart_ptr<Ref<Ragdoll>>("RagdollRef")
        .function("AddToPhysicsSystem(activationMode)", +[](Ragdoll &r, EActivation a) { r.AddToPhysicsSystem(a); })
        .function("RemoveFromPhysicsSystem", +[](Ragdoll &r) { r.RemoveFromPhysicsSystem(); })
        .function("Activate", +[](Ragdoll &r) { r.Activate(); })
        .function("IsActive", +[](const Ragdoll &r) { return r.IsActive(); })
        .function("SetGroupID(groupID)", +[](Ragdoll &r, uint32 g) { r.SetGroupID(g); })
        .function("GetRagdollSettings", +[](const Ragdoll &r) { return const_cast<RagdollSettings *>(r.GetRagdollSettings()); }, allow_raw_pointers())
        .function("GetBodyCount", +[](const Ragdoll &r) { return (uint32)r.GetBodyCount(); })
        .function("GetBodyID(index)", +[](const Ragdoll &r, int i) { return (uint32)r.GetBodyID(i).GetIndexAndSequenceNumber(); })
        .function("GetConstraintCount", +[](const Ragdoll &r) { return (uint32)r.GetConstraintCount(); })
        .function("GetConstraint(index)", +[](Ragdoll &r, int i) { return r.GetConstraint(i); }, allow_raw_pointers())
        .function("SetPose(pose)", +[](Ragdoll &r, const SkeletonPose &p) { r.SetPose(p); })
        .function("GetPose(pose)", +[](Ragdoll &r, SkeletonPose &p) { r.GetPose(p); })
        .function("DriveToPoseUsingMotors(pose)", &Ragdoll::DriveToPoseUsingMotors)
        .function("DriveToPoseUsingKinematics(pose, deltaTime)", +[](Ragdoll &r, const SkeletonPose &p, float dt) { r.DriveToPoseUsingKinematics(p, dt); })
        .function("ResetWarmStart", &Ragdoll::ResetWarmStart)
        .function("SetLinearAndAngularVelocity(linearVelocity, angularVelocity)", +[](Ragdoll &r, Vec3 lv, Vec3 av) { r.SetLinearAndAngularVelocity(lv, av); })
        .function("AddImpulse(impulse)", +[](Ragdoll &r, Vec3 v) { r.AddImpulse(v); })
        .out_function("GetRootPosition(out)", out_desc::Vec3, +[](Ragdoll &r, uintptr_t out) {
            RVec3 p; Quat q; r.GetRootTransform(p, q); WriteVec3(p, out); })
        .out_function("GetRootRotation(out)", out_desc::Quat, +[](Ragdoll &r, uintptr_t out) {
            RVec3 p; Quat q; r.GetRootTransform(p, q); WriteQuat(q, out); })
        .out_function("GetWorldSpaceBounds(out)", out_desc::AABox, +[](const Ragdoll &r, uintptr_t out) { WriteAABox(r.GetWorldSpaceBounds(), out); })
        .function("SetLinearVelocity(linearVelocity)", +[](Ragdoll &r, Vec3 v) { r.SetLinearVelocity(v); })
        .function("AddLinearVelocity(linearVelocity)", +[](Ragdoll &r, Vec3 v) { r.AddLinearVelocity(v); })
        .function("GetBodyIDs: number[]", +[](const Ragdoll &r) {
            const Array<BodyID> &ids = r.GetBodyIDs();
            val out = val::array();
            for (size_t i = 0; i < ids.size(); ++i) out.call<void>("push", fromBodyID(ids[i]));
            return out; });

    // ==== vehicles ===========================================================
    // A VehicleConstraint is BOTH a Constraint (AddConstraint) and a PhysicsStepListener;
    // JS adds it as a constraint, then calls addVehicleStepListener() (below) for the
    // step-listener side (embind exposes one base, so the MI upcast is done in C++).
    enum_<ETireFrictionDirection>("ETireFrictionDirection")
        .value("Longitudinal", ETireFrictionDirection_Longitudinal)
        .value("Lateral", ETireFrictionDirection_Lateral);
    jolt_class_<PhysicsStepListenerContext>("PhysicsStepListenerContext")
        .property("mDeltaTime", &PhysicsStepListenerContext::mDeltaTime)
        .property("mIsFirstStep", &PhysicsStepListenerContext::mIsFirstStep)
        .property("mIsLastStep", &PhysicsStepListenerContext::mIsLastStep)
        .function("GetPhysicsSystem", +[](const PhysicsStepListenerContext &c) { return c.mPhysicsSystem; }, allow_raw_pointers());

    jolt_class_<TireMaxImpulseCallbackResult>("TireMaxImpulseCallbackResult")
        .property("mLongitudinalImpulse", &TireMaxImpulseCallbackResult::mLongitudinalImpulse)
        .property("mLateralImpulse",      &TireMaxImpulseCallbackResult::mLateralImpulse);
    jolt_class_<VehicleConstraintCallbacksEm>("VehicleConstraintCallbacksEm")
        .function("SetVehicleConstraint(constraint)", &VehicleConstraintCallbacksEm::SetVehicleConstraint)
        .allow_subclass<VehicleConstraintCallbacksWrapper>("VehicleConstraintCallbacksJS");
    jolt_class_<WheeledVehicleControllerCallbacksEm>("WheeledVehicleControllerCallbacksEm")
        .function("SetWheeledVehicleController(controller)", &WheeledVehicleControllerCallbacksEm::SetWheeledVehicleController)
        .allow_subclass<WheeledVehicleControllerCallbacksWrapper>("WheeledVehicleControllerCallbacksJS");

    jolt_class_<VehicleEngineSettings>("VehicleEngineSettings")
        .property("mMaxTorque", &VehicleEngineSettings::mMaxTorque)
        .property("mMinRPM", &VehicleEngineSettings::mMinRPM)
        .property("mMaxRPM", &VehicleEngineSettings::mMaxRPM)
        .property("mInertia", &VehicleEngineSettings::mInertia)
        .property("mAngularDamping", &VehicleEngineSettings::mAngularDamping)
        .function("GetNormalizedTorque", +[](VehicleEngineSettings &s) { return &s.mNormalizedTorque; }, allow_raw_pointers());
    jolt_class_<VehicleTransmissionSettings>("VehicleTransmissionSettings")
        .property("mMode", &VehicleTransmissionSettings::mMode)
        .property("mSwitchTime", &VehicleTransmissionSettings::mSwitchTime)
        .property("mClutchReleaseTime", &VehicleTransmissionSettings::mClutchReleaseTime)
        .property("mShiftUpRPM", &VehicleTransmissionSettings::mShiftUpRPM)
        .property("mShiftDownRPM", &VehicleTransmissionSettings::mShiftDownRPM)
        .property("mClutchStrength", &VehicleTransmissionSettings::mClutchStrength)
        .property("mSwitchLatency", &VehicleTransmissionSettings::mSwitchLatency)
        // mGearRatios / mReverseGearRatios are Array<float>; expose add/clear helpers matching the AddDifferential style.
        .function("AddGearRatio(ratio)", +[](VehicleTransmissionSettings &s, float r) { s.mGearRatios.push_back(r); })
        .function("ClearGearRatios", +[](VehicleTransmissionSettings &s) { s.mGearRatios.clear(); })
        .function("AddReverseGearRatio(ratio)", +[](VehicleTransmissionSettings &s, float r) { s.mReverseGearRatios.push_back(r); })
        .function("ClearReverseGearRatios", +[](VehicleTransmissionSettings &s) { s.mReverseGearRatios.clear(); });
    jolt_class_<VehicleDifferentialSettings>("VehicleDifferentialSettings")
        .constructor<>()
        .property("mLeftWheel", &VehicleDifferentialSettings::mLeftWheel)
        .property("mRightWheel", &VehicleDifferentialSettings::mRightWheel)
        .property("mDifferentialRatio", &VehicleDifferentialSettings::mDifferentialRatio)
        .property("mLeftRightSplit", &VehicleDifferentialSettings::mLeftRightSplit)
        .property("mLimitedSlipRatio", &VehicleDifferentialSettings::mLimitedSlipRatio)
        .property("mEngineTorqueRatio", &VehicleDifferentialSettings::mEngineTorqueRatio);
    jolt_class_<VehicleAntiRollBar>("VehicleAntiRollBar")
        .constructor<>()
        .property("mLeftWheel", &VehicleAntiRollBar::mLeftWheel)
        .property("mRightWheel", &VehicleAntiRollBar::mRightWheel)
        .property("mStiffness", &VehicleAntiRollBar::mStiffness);
    jolt_class_<VehicleTrackSettings>("VehicleTrackSettings")
        .property("mDrivenWheel", &VehicleTrackSettings::mDrivenWheel)
        .property("mDifferentialRatio", &VehicleTrackSettings::mDifferentialRatio)
        .property("mInertia", &VehicleTrackSettings::mInertia)
        .property("mAngularDamping", &VehicleTrackSettings::mAngularDamping)
        .property("mMaxBrakeTorque", &VehicleTrackSettings::mMaxBrakeTorque)
        .function("AddWheel(wheelIndex)", +[](VehicleTrackSettings &t, uint w) { t.mWheels.push_back(w); })
        .function("ClearWheels", +[](VehicleTrackSettings &t) { t.mWheels.clear(); });

    // ---- vehicle runtime state classes (derive from their Settings) ----
    jolt_class_<VehicleEngine, base<VehicleEngineSettings>>("VehicleEngine")
        .function("ClampRPM", &VehicleEngine::ClampRPM)
        .function("GetCurrentRPM", &VehicleEngine::GetCurrentRPM)
        .function("SetCurrentRPM(rpm)", &VehicleEngine::SetCurrentRPM)
        .function("GetAngularVelocity", &VehicleEngine::GetAngularVelocity)
        .function("GetTorque(acceleration)", &VehicleEngine::GetTorque);
    jolt_class_<VehicleTransmission, base<VehicleTransmissionSettings>>("VehicleTransmission")
        .function("Set(currentGear, clutchFriction)", &VehicleTransmission::Set)
        .function("GetCurrentGear", &VehicleTransmission::GetCurrentGear)
        .function("GetClutchFriction", &VehicleTransmission::GetClutchFriction)
        .function("IsSwitchingGear", &VehicleTransmission::IsSwitchingGear)
        .function("GetCurrentRatio", &VehicleTransmission::GetCurrentRatio);
    jolt_class_<VehicleTrack, base<VehicleTrackSettings>>("VehicleTrack")
        .property("mAngularVelocity", &VehicleTrack::mAngularVelocity);

    jolt_class_<WheelSettings>("WheelSettings")
        .smart_ptr<Ref<WheelSettings>>("WheelSettingsRef")
        .property("mPosition", &WheelSettings::mPosition)
        .property("mSuspensionDirection", &WheelSettings::mSuspensionDirection)
        .property("mSteeringAxis", &WheelSettings::mSteeringAxis)
        .property("mWheelUp", &WheelSettings::mWheelUp)
        .property("mWheelForward", &WheelSettings::mWheelForward)
        .property("mSuspensionMinLength", &WheelSettings::mSuspensionMinLength)
        .property("mSuspensionMaxLength", &WheelSettings::mSuspensionMaxLength)
        .property("mRadius", &WheelSettings::mRadius)
        .property("mWidth", &WheelSettings::mWidth)
        .property("mSuspensionForcePoint", &WheelSettings::mSuspensionForcePoint)
        .property("mSuspensionPreloadLength", &WheelSettings::mSuspensionPreloadLength)
        .property("mEnableSuspensionForcePoint", &WheelSettings::mEnableSuspensionForcePoint)
        .function("GetSuspensionSpring", +[](WheelSettings &w) { return &w.mSuspensionSpring; }, allow_raw_pointers());
    jolt_class_<WheelSettingsWV, base<WheelSettings>>("WheelSettingsWV")
        .smart_ptr<Ref<WheelSettingsWV>>("WheelSettingsWVRef")
        .constructor(+[]() -> Ref<WheelSettingsWV> { return new WheelSettingsWV(); })
        .property("mInertia", &WheelSettingsWV::mInertia)
        .property("mAngularDamping", &WheelSettingsWV::mAngularDamping)
        .property("mMaxSteerAngle", &WheelSettingsWV::mMaxSteerAngle)
        .property("mMaxBrakeTorque", &WheelSettingsWV::mMaxBrakeTorque)
        .property("mMaxHandBrakeTorque", &WheelSettingsWV::mMaxHandBrakeTorque)
        .function("GetLongitudinalFriction", +[](WheelSettingsWV &w) { return &w.mLongitudinalFriction; }, allow_raw_pointers())
        .function("GetLateralFriction", +[](WheelSettingsWV &w) { return &w.mLateralFriction; }, allow_raw_pointers());
    jolt_class_<WheelSettingsTV, base<WheelSettings>>("WheelSettingsTV")
        .smart_ptr<Ref<WheelSettingsTV>>("WheelSettingsTVRef")
        .constructor(+[]() -> Ref<WheelSettingsTV> { return new WheelSettingsTV(); })
        .property("mLongitudinalFriction", &WheelSettingsTV::mLongitudinalFriction)
        .property("mLateralFriction", &WheelSettingsTV::mLateralFriction);

    jolt_class_<Wheel>("Wheel")
        .function("GetSettings", +[](Wheel &w) { return w.GetSettings(); }, allow_raw_pointers())
        .function("HasContact", &Wheel::HasContact)
        .function("GetRotationAngle", &Wheel::GetRotationAngle)
        .function("GetSteerAngle", &Wheel::GetSteerAngle)
        .function("SetSteerAngle(angle)", &Wheel::SetSteerAngle)
        .function("SetRotationAngle(angle)", &Wheel::SetRotationAngle)
        .function("GetAngularVelocity", &Wheel::GetAngularVelocity)
        .function("SetAngularVelocity(vel)", &Wheel::SetAngularVelocity)
        .function("GetSuspensionLength", &Wheel::GetSuspensionLength)
        .function("HasHitHardPoint", &Wheel::HasHitHardPoint)
        .function("GetSuspensionLambda", &Wheel::GetSuspensionLambda)
        .function("GetLongitudinalLambda", &Wheel::GetLongitudinalLambda)
        .function("GetLateralLambda", &Wheel::GetLateralLambda)
        // BodyID -> uint32 (file convention); guard HasContact() before reading contact* in JS.
        .function("GetContactBodyID", +[](const Wheel &w) { return (uint32)w.GetContactBodyID().GetIndexAndSequenceNumber(); })
        .function("GetContactSubShapeID", +[](const Wheel &w) { return (uint32)w.GetContactSubShapeID().GetValue(); })
        .out_function("GetContactPosition(out)", out_desc::Vec3, +[](const Wheel &w, uintptr_t out) { WriteVec3(w.GetContactPosition(), out); })
        .out_function("GetContactPointVelocity(out)", out_desc::Vec3, +[](const Wheel &w, uintptr_t out) { WriteVec3(w.GetContactPointVelocity(), out); })
        .out_function("GetContactNormal(out)", out_desc::Vec3, +[](const Wheel &w, uintptr_t out) { WriteVec3(w.GetContactNormal(), out); })
        .out_function("GetContactLongitudinal(out)", out_desc::Vec3, +[](const Wheel &w, uintptr_t out) { WriteVec3(w.GetContactLongitudinal(), out); })
        .out_function("GetContactLateral(out)", out_desc::Vec3, +[](const Wheel &w, uintptr_t out) { WriteVec3(w.GetContactLateral(), out); });
    jolt_class_<WheelWV, base<Wheel>>("WheelWV")
        .function("GetSettings", +[](WheelWV &w) { return const_cast<WheelSettingsWV *>(w.GetSettings()); }, allow_raw_pointers())
        .property("mLongitudinalSlip", &WheelWV::mLongitudinalSlip)
        .property("mLateralSlip", &WheelWV::mLateralSlip)
        .property("mCombinedLongitudinalFriction", &WheelWV::mCombinedLongitudinalFriction)
        .property("mCombinedLateralFriction", &WheelWV::mCombinedLateralFriction)
        .property("mBrakeImpulse", &WheelWV::mBrakeImpulse);
    jolt_class_<WheelTV, base<Wheel>>("WheelTV")
        .function("GetSettings", +[](WheelTV &w) { return const_cast<WheelSettingsTV *>(w.GetSettings()); }, allow_raw_pointers())
        .property("mTrackIndex", &WheelTV::mTrackIndex)
        .property("mCombinedLongitudinalFriction", &WheelTV::mCombinedLongitudinalFriction)
        .property("mCombinedLateralFriction", &WheelTV::mCombinedLateralFriction)
        .property("mBrakeImpulse", &WheelTV::mBrakeImpulse);

    jolt_class_<VehicleControllerSettings>("VehicleControllerSettings")
        .smart_ptr<Ref<VehicleControllerSettings>>("VehicleControllerSettingsRef");
    jolt_class_<WheeledVehicleControllerSettings, base<VehicleControllerSettings>>("WheeledVehicleControllerSettings")
        .smart_ptr<Ref<WheeledVehicleControllerSettings>>("WheeledVehicleControllerSettingsRef")
        .constructor(+[]() -> Ref<WheeledVehicleControllerSettings> { return new WheeledVehicleControllerSettings(); })
        .property("mDifferentialLimitedSlipRatio", &WheeledVehicleControllerSettings::mDifferentialLimitedSlipRatio)
        .function("GetEngine", +[](WheeledVehicleControllerSettings &s) { return &s.mEngine; }, allow_raw_pointers())
        .function("GetTransmission", +[](WheeledVehicleControllerSettings &s) { return &s.mTransmission; }, allow_raw_pointers())
        .function("AddDifferential(differential)", +[](WheeledVehicleControllerSettings &s, VehicleDifferentialSettings d) { s.mDifferentials.push_back(d); })
        .function("ClearDifferentials", +[](WheeledVehicleControllerSettings &s) { s.mDifferentials.clear(); });
    jolt_class_<MotorcycleControllerSettings, base<WheeledVehicleControllerSettings>>("MotorcycleControllerSettings")
        .smart_ptr<Ref<MotorcycleControllerSettings>>("MotorcycleControllerSettingsRef")
        .constructor(+[]() -> Ref<MotorcycleControllerSettings> { return new MotorcycleControllerSettings(); })
        .property("mMaxLeanAngle", &MotorcycleControllerSettings::mMaxLeanAngle)
        .property("mLeanSpringConstant", &MotorcycleControllerSettings::mLeanSpringConstant)
        .property("mLeanSpringDamping", &MotorcycleControllerSettings::mLeanSpringDamping)
        .property("mLeanSpringIntegrationCoefficient", &MotorcycleControllerSettings::mLeanSpringIntegrationCoefficient)
        .property("mLeanSpringIntegrationCoefficientDecay", &MotorcycleControllerSettings::mLeanSpringIntegrationCoefficientDecay)
        .property("mLeanSmoothingFactor", &MotorcycleControllerSettings::mLeanSmoothingFactor);
    jolt_class_<TrackedVehicleControllerSettings, base<VehicleControllerSettings>>("TrackedVehicleControllerSettings")
        .smart_ptr<Ref<TrackedVehicleControllerSettings>>("TrackedVehicleControllerSettingsRef")
        .constructor(+[]() -> Ref<TrackedVehicleControllerSettings> { return new TrackedVehicleControllerSettings(); })
        .function("GetEngine", +[](TrackedVehicleControllerSettings &s) { return &s.mEngine; }, allow_raw_pointers())
        .function("GetTransmission", +[](TrackedVehicleControllerSettings &s) { return &s.mTransmission; }, allow_raw_pointers())
        .function("GetTrack(index)", +[](TrackedVehicleControllerSettings &s, int i) { return &s.mTracks[i]; }, allow_raw_pointers());

    jolt_class_<VehicleController>("VehicleController")
        .function("GetConstraint", +[](VehicleController &c) { return &c.GetConstraint(); }, allow_raw_pointers());
    jolt_class_<WheeledVehicleController, base<VehicleController>>("WheeledVehicleController")
        .function("SetDriverInput(forward, right, brake, handBrake)", &WheeledVehicleController::SetDriverInput)
        .function("SetForwardInput(forward)", &WheeledVehicleController::SetForwardInput)
        .function("GetForwardInput", &WheeledVehicleController::GetForwardInput)
        .function("SetRightInput(right)", &WheeledVehicleController::SetRightInput)
        .function("GetRightInput", &WheeledVehicleController::GetRightInput)
        .function("SetBrakeInput(brake)", &WheeledVehicleController::SetBrakeInput)
        .function("GetBrakeInput", &WheeledVehicleController::GetBrakeInput)
        .function("SetHandBrakeInput(handBrake)", &WheeledVehicleController::SetHandBrakeInput)
        .function("GetHandBrakeInput", &WheeledVehicleController::GetHandBrakeInput)
        // non-const overloads return writable VehicleEngine& / VehicleTransmission&.
        .function("GetEngine", +[](WheeledVehicleController &c) { return &c.GetEngine(); }, allow_raw_pointers())
        .function("GetTransmission", +[](WheeledVehicleController &c) { return &c.GetTransmission(); }, allow_raw_pointers())
        .function("GetDifferentialLimitedSlipRatio", &WheeledVehicleController::GetDifferentialLimitedSlipRatio)
        .function("SetDifferentialLimitedSlipRatio(v)", &WheeledVehicleController::SetDifferentialLimitedSlipRatio)
        .function("GetWheelSpeedAtClutch", &WheeledVehicleController::GetWheelSpeedAtClutch);
    jolt_class_<MotorcycleController, base<WheeledVehicleController>>("MotorcycleController")
        .function("EnableLeanController(enable)", &MotorcycleController::EnableLeanController)
        .function("GetWheelBase", &MotorcycleController::GetWheelBase)
        .function("IsLeanControllerEnabled", &MotorcycleController::IsLeanControllerEnabled)
        .function("EnableLeanSteeringLimit(enable)", &MotorcycleController::EnableLeanSteeringLimit)
        .function("IsLeanSteeringLimitEnabled", &MotorcycleController::IsLeanSteeringLimitEnabled)
        .function("SetLeanSpringConstant(c)", &MotorcycleController::SetLeanSpringConstant)
        .function("GetLeanSpringConstant", &MotorcycleController::GetLeanSpringConstant)
        .function("SetLeanSpringDamping(d)", &MotorcycleController::SetLeanSpringDamping)
        .function("GetLeanSpringDamping", &MotorcycleController::GetLeanSpringDamping)
        .function("SetLeanSpringIntegrationCoefficient(c)", &MotorcycleController::SetLeanSpringIntegrationCoefficient)
        .function("GetLeanSpringIntegrationCoefficient", &MotorcycleController::GetLeanSpringIntegrationCoefficient)
        .function("SetLeanSpringIntegrationCoefficientDecay(d)", &MotorcycleController::SetLeanSpringIntegrationCoefficientDecay)
        .function("GetLeanSpringIntegrationCoefficientDecay", &MotorcycleController::GetLeanSpringIntegrationCoefficientDecay)
        .function("SetLeanSmoothingFactor(f)", &MotorcycleController::SetLeanSmoothingFactor)
        .function("GetLeanSmoothingFactor", &MotorcycleController::GetLeanSmoothingFactor);
    jolt_class_<TrackedVehicleController, base<VehicleController>>("TrackedVehicleController")
        .function("SetDriverInput(forward, leftRatio, rightRatio, brake)", &TrackedVehicleController::SetDriverInput)
        .function("SetForwardInput(forward)", &TrackedVehicleController::SetForwardInput)
        .function("GetForwardInput", &TrackedVehicleController::GetForwardInput)
        .function("SetLeftRatio(leftRatio)", &TrackedVehicleController::SetLeftRatio)
        .function("GetLeftRatio", &TrackedVehicleController::GetLeftRatio)
        .function("SetRightRatio(rightRatio)", &TrackedVehicleController::SetRightRatio)
        .function("GetRightRatio", &TrackedVehicleController::GetRightRatio)
        .function("SetBrakeInput(brake)", &TrackedVehicleController::SetBrakeInput)
        .function("GetBrakeInput", &TrackedVehicleController::GetBrakeInput)
        .function("GetEngine", +[](TrackedVehicleController &c) { return &c.GetEngine(); }, allow_raw_pointers())
        .function("GetTransmission", +[](TrackedVehicleController &c) { return &c.GetTransmission(); }, allow_raw_pointers())
        // GetTracks() returns VehicleTrack(&)[2] (a C array) — expose per-index.
        .function("GetTrack(index)", +[](TrackedVehicleController &c, int i) { return &c.GetTracks()[i]; }, allow_raw_pointers());

    jolt_class_<VehicleCollisionTester>("VehicleCollisionTester")
        .smart_ptr<Ref<VehicleCollisionTester>>("VehicleCollisionTesterRef");
    jolt_class_<VehicleCollisionTesterRay, base<VehicleCollisionTester>>("VehicleCollisionTesterRay")
        .smart_ptr<Ref<VehicleCollisionTesterRay>>("VehicleCollisionTesterRayRef")
        .constructor("objectLayer", +[](uint16 layer) -> Ref<VehicleCollisionTesterRay> { return new VehicleCollisionTesterRay(layer); });
    jolt_class_<VehicleCollisionTesterCastSphere, base<VehicleCollisionTester>>("VehicleCollisionTesterCastSphere")
        .smart_ptr<Ref<VehicleCollisionTesterCastSphere>>("VehicleCollisionTesterCastSphereRef")
        .constructor("objectLayer, radius", +[](uint16 layer, float radius) -> Ref<VehicleCollisionTesterCastSphere> { return new VehicleCollisionTesterCastSphere(layer, radius); });
    jolt_class_<VehicleCollisionTesterCastCylinder, base<VehicleCollisionTester>>("VehicleCollisionTesterCastCylinder")
        .smart_ptr<Ref<VehicleCollisionTesterCastCylinder>>("VehicleCollisionTesterCastCylinderRef")
        .constructor("objectLayer, convexRadiusFraction", +[](uint16 layer, float convexRadiusFraction) -> Ref<VehicleCollisionTesterCastCylinder> { return new VehicleCollisionTesterCastCylinder(layer, convexRadiusFraction); });

    jolt_class_<VehicleConstraintSettings, base<ConstraintSettings>>("VehicleConstraintSettings")
        .smart_ptr<Ref<VehicleConstraintSettings>>("VehicleConstraintSettingsRef")
        .constructor(+[]() -> Ref<VehicleConstraintSettings> { return new VehicleConstraintSettings(); })
        .property("mUp", &VehicleConstraintSettings::mUp)
        .property("mForward", &VehicleConstraintSettings::mForward)
        .property("mMaxPitchRollAngle", &VehicleConstraintSettings::mMaxPitchRollAngle)
        .function("AddWheel(wheelSettings)", +[](VehicleConstraintSettings &s, WheelSettings *w) { s.mWheels.push_back(w); }, allow_raw_pointers())
        .function("ClearWheels", +[](VehicleConstraintSettings &s) { s.mWheels.clear(); })
        .function("AddAntiRollBar(antiRollBar)", +[](VehicleConstraintSettings &s, VehicleAntiRollBar b) { s.mAntiRollBars.push_back(b); })
        .function("SetController(controllerSettings)", +[](VehicleConstraintSettings &s, VehicleControllerSettings *c) { s.mController = c; }, allow_raw_pointers());

    jolt_class_<VehicleConstraint, base<Constraint>>("VehicleConstraint")
        .smart_ptr<Ref<VehicleConstraint>>("VehicleConstraintRef")
        .constructor("body, settings", +[](Body &body, const VehicleConstraintSettings &settings) -> Ref<VehicleConstraint> {
            return new VehicleConstraint(body, settings); }, allow_raw_pointers())
        .function("SetVehicleCollisionTester(tester)", &VehicleConstraint::SetVehicleCollisionTester, allow_raw_pointers())
        .function("GetWheel(index)", +[](VehicleConstraint &c, uint i) { return c.GetWheel(i); }, allow_raw_pointers())
        .function("GetController", +[](VehicleConstraint &c) { return c.GetController(); }, allow_raw_pointers())
        .function("GetWheeledController", +[](VehicleConstraint &c) { return static_cast<WheeledVehicleController *>(c.GetController()); }, allow_raw_pointers())
        .function("GetTrackedController", +[](VehicleConstraint &c) { return static_cast<TrackedVehicleController *>(c.GetController()); }, allow_raw_pointers())
        .function("GetMotorcycleController", +[](VehicleConstraint &c) { return static_cast<MotorcycleController *>(c.GetController()); }, allow_raw_pointers())
        .out_function("GetWheelLocalTransform(out, wheelIndex, wheelRight, wheelUp)",
            out_desc::Mat44, out_desc::Pass, out_desc::Pass, out_desc::Pass, +[](VehicleConstraint &c, uintptr_t out, uint i, Vec3 right, Vec3 up) { WriteMat4(c.GetWheelLocalTransform(i, right, up), out); })
        .out_function("GetWheelWorldTransform(out, wheelIndex, wheelRight, wheelUp)",
            out_desc::Mat44, out_desc::Pass, out_desc::Pass, out_desc::Pass, +[](VehicleConstraint &c, uintptr_t out, uint i, Vec3 right, Vec3 up) { WriteMat4(c.GetWheelWorldTransform(i, right, up), out); })
        .function("SetMaxPitchRollAngle(maxPitchRollAngle)", &VehicleConstraint::SetMaxPitchRollAngle)
        .function("GetMaxPitchRollAngle", &VehicleConstraint::GetMaxPitchRollAngle)
        .function("GetVehicleCollisionTester", +[](VehicleConstraint &c) { return const_cast<VehicleCollisionTester *>(c.GetVehicleCollisionTester()); }, allow_raw_pointers())
        .function("OverrideGravity(gravity)", +[](VehicleConstraint &c, Vec3 g) { c.OverrideGravity(g); })
        .function("IsGravityOverridden", &VehicleConstraint::IsGravityOverridden)
        .out_function("GetGravityOverride(out)", out_desc::Vec3, +[](VehicleConstraint &c, uintptr_t out) { WriteVec3(c.GetGravityOverride(), out); })
        .function("ResetGravityOverride", &VehicleConstraint::ResetGravityOverride)
        .out_function("GetLocalForward(out)", out_desc::Vec3, +[](VehicleConstraint &c, uintptr_t out) { WriteVec3(c.GetLocalForward(), out); })
        .out_function("GetLocalUp(out)", out_desc::Vec3, +[](VehicleConstraint &c, uintptr_t out) { WriteVec3(c.GetLocalUp(), out); })
        .out_function("GetWorldUp(out)", out_desc::Vec3, +[](VehicleConstraint &c, uintptr_t out) { WriteVec3(c.GetWorldUp(), out); })
        .function("GetVehicleBody", &VehicleConstraint::GetVehicleBody, allow_raw_pointers())
        // Wheels/anti-roll-bars: expose count + per-index handles rather than binding the containers.
        .function("GetNumWheels", +[](const VehicleConstraint &c) { return (uint32)c.GetWheels().size(); })
        .function("GetNumAntiRollBars", +[](VehicleConstraint &c) { return (uint32)c.GetAntiRollBars().size(); })
        .function("GetAntiRollBar(index)", +[](VehicleConstraint &c, uint32 i) { return &c.GetAntiRollBars()[i]; }, allow_raw_pointers())
        .function("SetNumStepsBetweenCollisionTestActive(steps)", &VehicleConstraint::SetNumStepsBetweenCollisionTestActive)
        .function("GetNumStepsBetweenCollisionTestActive", &VehicleConstraint::GetNumStepsBetweenCollisionTestActive)
        .function("SetNumStepsBetweenCollisionTestInactive(steps)", &VehicleConstraint::SetNumStepsBetweenCollisionTestInactive)
        .function("GetNumStepsBetweenCollisionTestInactive", &VehicleConstraint::GetNumStepsBetweenCollisionTestInactive);

    // MI upcast helper: add a VehicleConstraint's PhysicsStepListener side to the system.
    emscripten::function("addVehicleStepListener", +[](PhysicsSystem *ps, VehicleConstraint *vc) { ps->AddStepListener(vc); }, allow_raw_pointers());

    // ---- expose out-param / ctor metadata for facade.js + postprocess-tsd ----
    // _outMeta: array of named-field objects (stable regardless of future field additions).
    // Shape: {cls, method, sizes:[N], trailing:N, tsTypes:["T",...]}
    emscripten::function("_outMeta", +[]() -> std::string {
        std::ostringstream ss;
        ss << "[";
        bool first = true;
        for (const auto& e : sOutRegistry) {
            if (!first) ss << ","; first = false;
            ss << "{\"cls\":" << jStr(e.cls) << ",\"method\":" << jStr(e.method) << ",\"sizes\":[";
            for (int i = 0; i < (int)e.sizes.size(); i++) { if (i) ss << ","; ss << e.sizes[i]; }
            ss << "],\"trailing\":" << e.trailing << ",\"tsTypes\":[";
            for (int i = 0; i < (int)e.tsTypes.size(); i++) { if (i) ss << ","; ss << jStr(e.tsTypes[i]); }
            ss << "]}";
        }
        ss << "]";
        return ss.str();
    });
    // _ctorMeta: {ClassName: [{n: paramName, t?: tsType}, ...]}
    emscripten::function("_ctorMeta", +[]() -> std::string {
        std::ostringstream ss;
        ss << "{";
        bool first = true;
        for (const auto& e : sCtorRegistry) {
            if (!first) ss << ","; first = false;
            ss << jStr(e.cls) << ":[";
            for (size_t i = 0; i < e.params.size(); i++) {
                if (i) ss << ",";
                ss << "{\"n\":" << jStr(e.params[i].name);
                if (!e.params[i].tsType.empty()) ss << ",\"t\":" << jStr(e.params[i].tsType);
                ss << "}";
            }
            ss << "]";
        }
        ss << "}";
        return ss.str();
    });
    // _retMeta: array of {cls, method, tsType} — return-type overrides from the ": T" DSL.
    emscripten::function("_retMeta", +[]() -> std::string {
        std::ostringstream ss;
        ss << "[";
        bool first = true;
        for (const auto& e : sRetRegistry) {
            if (!first) ss << ","; first = false;
            ss << "{\"cls\":" << jStr(e.cls) << ",\"method\":" << jStr(e.method)
               << ",\"tsType\":" << jStr(e.tsType) << "}";
        }
        ss << "]";
        return ss.str();
    });

#ifdef JPH_DEBUG_RENDERER
    // ---- debug renderer ----
    enum_<ECullMode>("ECullMode")
        .value("CullBackFace",  ECullMode::CullBackFace)
        .value("CullFrontFace", ECullMode::CullFrontFace)
        .value("Off",           ECullMode::Off);
    enum_<ECastShadow>("ECastShadow")
        .value("On",  ECastShadow::On)
        .value("Off", ECastShadow::Off);
    enum_<EDrawMode>("EDrawMode")
        .value("Solid",     EDrawMode::Solid)
        .value("Wireframe", EDrawMode::Wireframe);
    enum_<EShapeColor>("EShapeColor")
        .value("InstanceColor",   EShapeColor::InstanceColor)
        .value("ShapeTypeColor",  EShapeColor::ShapeTypeColor)
        .value("MotionTypeColor", EShapeColor::MotionTypeColor)
        .value("SleepColor",      EShapeColor::SleepColor)
        .value("IslandColor",     EShapeColor::IslandColor)
        .value("MaterialColor",   EShapeColor::MaterialColor);
    enum_<ESoftBodyConstraintColor>("ESoftBodyConstraintColor")
        .value("ConstraintType",  ESoftBodyConstraintColor::ConstraintType)
        .value("ConstraintGroup", ESoftBodyConstraintColor::ConstraintGroup)
        .value("ConstraintOrder", ESoftBodyConstraintColor::ConstraintOrder);

    jolt_class_<BodyManagerDrawSettings>("BodyManagerDrawSettings")
        .constructor<>()
        .property("mDrawGetSupportFunction",              &BodyManagerDrawSettings::mDrawGetSupportFunction)
        .property("mDrawSupportDirection",                &BodyManagerDrawSettings::mDrawSupportDirection)
        .property("mDrawGetSupportingFace",               &BodyManagerDrawSettings::mDrawGetSupportingFace)
        .property("mDrawShape",                           &BodyManagerDrawSettings::mDrawShape)
        .property("mDrawShapeWireframe",                  &BodyManagerDrawSettings::mDrawShapeWireframe)
        .property("mDrawShapeColor",                      &BodyManagerDrawSettings::mDrawShapeColor)
        .property("mDrawBoundingBox",                     &BodyManagerDrawSettings::mDrawBoundingBox)
        .property("mDrawCenterOfMassTransform",           &BodyManagerDrawSettings::mDrawCenterOfMassTransform)
        .property("mDrawWorldTransform",                  &BodyManagerDrawSettings::mDrawWorldTransform)
        .property("mDrawVelocity",                        &BodyManagerDrawSettings::mDrawVelocity)
        .property("mDrawMassAndInertia",                  &BodyManagerDrawSettings::mDrawMassAndInertia)
        .property("mDrawSleepStats",                      &BodyManagerDrawSettings::mDrawSleepStats)
        .property("mDrawSoftBodyVertices",                &BodyManagerDrawSettings::mDrawSoftBodyVertices)
        .property("mDrawSoftBodyVertexVelocities",        &BodyManagerDrawSettings::mDrawSoftBodyVertexVelocities)
        .property("mDrawSoftBodyEdgeConstraints",         &BodyManagerDrawSettings::mDrawSoftBodyEdgeConstraints)
        .property("mDrawSoftBodyBendConstraints",         &BodyManagerDrawSettings::mDrawSoftBodyBendConstraints)
        .property("mDrawSoftBodyVolumeConstraints",       &BodyManagerDrawSettings::mDrawSoftBodyVolumeConstraints)
        .property("mDrawSoftBodySkinConstraints",         &BodyManagerDrawSettings::mDrawSoftBodySkinConstraints)
        .property("mDrawSoftBodyLRAConstraints",          &BodyManagerDrawSettings::mDrawSoftBodyLRAConstraints)
        .property("mDrawSoftBodyRods",                    &BodyManagerDrawSettings::mDrawSoftBodyRods)
        .property("mDrawSoftBodyRodStates",               &BodyManagerDrawSettings::mDrawSoftBodyRodStates)
        .property("mDrawSoftBodyRodBendTwistConstraints", &BodyManagerDrawSettings::mDrawSoftBodyRodBendTwistConstraints)
        .property("mDrawSoftBodyPredictedBounds",         &BodyManagerDrawSettings::mDrawSoftBodyPredictedBounds)
        .property("mDrawSoftBodyConstraintColor",         &BodyManagerDrawSettings::mDrawSoftBodyConstraintColor);

    jolt_class_<DebugRendererVertexTraits>("DebugRendererVertexTraits")
        .class_function("mPositionOffset", +[]() { return DebugRendererVertexTraits::mPositionOffset; })
        .class_function("mNormalOffset",   +[]() { return DebugRendererVertexTraits::mNormalOffset; })
        .class_function("mUVOffset",       +[]() { return DebugRendererVertexTraits::mUVOffset; })
        .class_function("mSize",           +[]() { return DebugRendererVertexTraits::mSize; });

    jolt_class_<DebugRendererTriangleTraits>("DebugRendererTriangleTraits")
        .class_function("mVOffset", +[]() { return DebugRendererTriangleTraits::mVOffset; })
        .class_function("mSize",    +[]() { return DebugRendererTriangleTraits::mSize; });

    jolt_class_<DebugRendererEm>("DebugRendererEm")
        .allow_subclass<DebugRendererWrapper>("DebugRendererWrapper")
        .function("Initialize", &DebugRendererEm::Initialize)
        // DrawBodies / DrawConstraints: JS calls these to trigger drawing; they call back into the JS subclass
        .function("DrawBodies(system, settings)", static_cast<void (DebugRendererEm::*)(PhysicsSystem*, BodyManagerDrawSettings*)>(&DebugRendererEm::DrawBodies), allow_raw_pointers())
        .function("DrawBodies(system)",           static_cast<void (DebugRendererEm::*)(PhysicsSystem*)>(&DebugRendererEm::DrawBodies), allow_raw_pointers())
        .function("DrawConstraints(system)",             &DebugRendererEm::DrawConstraints,             allow_raw_pointers())
        .function("DrawConstraintLimits(system)",        &DebugRendererEm::DrawConstraintLimits,        allow_raw_pointers())
        .function("DrawConstraintReferenceFrame(system)",&DebugRendererEm::DrawConstraintReferenceFrame, allow_raw_pointers())
        // DrawShape/DrawBody: Mat44/Vec3/Color are value_array so wrap to take by value
        .function("DrawShape(shape, modelMatrix, scale, color, wireframe)",
            +[](DebugRendererEm &r, Shape *s, Mat44 m, Vec3 sc, Color c, bool wire) {
                r.DrawShape(s, &m, &sc, &c, wire);
            }, allow_raw_pointers())
        .function("DrawBody(body, color, wireframe)",
            +[](DebugRendererEm &r, Body *b, Color c, bool wire) {
                r.DrawBody(b, &c, wire);
            }, allow_raw_pointers())
        .function("DrawConstraint(constraint)", &DebugRendererEm::DrawConstraint, allow_raw_pointers());
#endif
}
