// Disable embind's FinalizationRegistry-based auto-.delete()-on-GC.
//
// The majority of small objects that with webidl binder typically required manual memory management
// now use value_* embind types which use regular javascript objects for arguments and return types.
// The remaining concerns are objects that are created and destroyed with clearer intent.
// 
// There are also some highly unexpected cases FR creates in the context of idiomatic javascript.
// For example, attaching a JS-implemented contact listener to a physics system, and then letting the contact listener
// fall out of scope. The expectation might be that the contact listener is still alive and stored within the physics system.
// What instead happens is the contact listener can be garbage collected and finalized, and then the physics system has a dangling pointer to a deleted object.
// This can lead to crashes that are very difficult to debug.
//
// By disabling the finalizer, we avoid these issues and make memory management more explicit and predictable. 
//
// This overrides embind's $attachFinalizer with the same identity function embind itself uses
// when the environment has no FinalizationRegistry, so no registry is ever created. The default
// no-op $detachFinalizer and $finalizationRegistry = false stay consistent, and explicit
// .delete() (releaseClassHandle) is unaffected. globalThis.FinalizationRegistry is left intact
// for the host page — only embind stops using it.
addToLibrary({
  $attachFinalizer__deps: [],
  $attachFinalizer: (handle) => handle,
});
