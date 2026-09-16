// Shared Jolt WASM heap overlay for Examples / Examples_old.
// Works with both the new embind API (JoltInterface.sGetFreeMemory) and the
// old webidl binder (JoltInterface.prototype.sGetFreeMemory).

function getJoltUsedBytes(Jolt) {
  const iface = Jolt.JoltInterface;
  const free = typeof iface.sGetFreeMemory === 'function'
    ? iface.sGetFreeMemory()
    : iface.prototype.sGetFreeMemory();
  const total = Jolt.HEAP8 ? Jolt.HEAP8.length
    : (Jolt.wasmMemory?.buffer?.byteLength ?? 0);
  return total - free;
}

function formatBytes(bytes) {
  const abs = Math.abs(bytes);
  if (abs < 1024) return `${bytes | 0} B`;
  if (abs < 1024 * 1024) return `${(bytes / 1024).toFixed(1)} KB`;
  return `${(bytes / (1024 * 1024)).toFixed(2)} MB`;
}

function formatDelta(bytes) {
  if (bytes == null) return '…';
  const sign = bytes > 0 ? '+' : '';
  return sign + formatBytes(bytes);
}

const MEMORY_WINDOWS = [10, 30, 60];

// Overlay in the top-right corner: current WASM heap used by Jolt, plus growth
// over the last 10 / 30 / 60 seconds. Extra rows via setCustomField / removeCustomField.
export class JoltMemoryDisplay {
  constructor(Jolt) {
    this._Jolt = Jolt;
    let el = document.getElementById('jolt-memory');
    if (!el) {
      el = document.createElement('div');
      el.id = 'jolt-memory';
      Object.assign(el.style, {
        position: 'fixed',
        top: '8px',
        right: '8px',
        zIndex: '1000',
        padding: '8px 10px',
        background: 'rgba(0, 0, 0, 0.7)',
        color: '#e8e8e8',
        font: '12px/1.45 ui-monospace, SFMono-Regular, Menlo, Consolas, monospace',
        textAlign: 'left',
        whiteSpace: 'pre',
        borderRadius: '4px',
        pointerEvents: 'none',
      });
      document.body.appendChild(el);
    }
    this._el = el;
    this._history = [];           // { t, used } samples kept for ~60s
    this._customFields = new Map(); // name -> value (insertion order)
    this._raf = requestAnimationFrame(() => this._tick());
  }

  // Adds a custom row, or updates it if the name already exists.
  setCustomField(name, value) {
    this._customFields.set(name, String(value));
  }

  removeCustomField(name) {
    this._customFields.delete(name);
  }

  _growthSince(now, seconds) {
    const target = now - seconds * 1000;
    let sample = null;
    for (let i = 0; i < this._history.length; ++i) {
      if (this._history[i].t <= target) sample = this._history[i];
      else break;
    }
    if (!sample || now - sample.t < seconds * 1000 * 0.9) return null;
    return this._history[this._history.length - 1].used - sample.used;
  }

  _tick() {
    const now = performance.now();
    const used = getJoltUsedBytes(this._Jolt);
    this._history.push({ t: now, used });
    const cutoff = now - 60 * 1000;
    while (this._history.length > 1 && this._history[0].t < cutoff) this._history.shift();

    const lines = [`Jolt memory: ${formatBytes(used)}`];
    for (const s of MEMORY_WINDOWS)
      lines.push(`Δ ${s}s: ${formatDelta(this._growthSince(now, s))}`);
    for (const [name, value] of this._customFields)
      lines.push(`${name}: ${value}`);
    this._el.textContent = lines.join('\n');
    this._raf = requestAnimationFrame(() => this._tick());
  }
}

// Call once after Jolt is initialized; returns the display instance for custom fields.
export function showJoltMemory(Jolt) {
  return new JoltMemoryDisplay(Jolt);
}
