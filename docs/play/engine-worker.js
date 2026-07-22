let Module = null;
let readyPromise = null;

function decode(pointer) {
  return Module.UTF8ToString(pointer);
}

async function ensureReady() {
  if (readyPromise) return readyPromise;
  readyPromise = (async () => {
    importScripts('./engine/eunshinbishop.js');
    Module = await createEunshinBishopModule({
      locateFile(path) { return `./engine/${path}`; }
    });
    const init = JSON.parse(decode(Module._eunshin_init()));
    if (!init.ok) throw new Error(init.error || 'Engine initialization failed');
    return init;
  })();
  return readyPromise;
}

self.onmessage = async event => {
  const { id, type, fen, depth, moveTimeMs } = event.data || {};
  try {
    if (type === 'stop') {
      if (Module) Module._eunshin_stop();
      self.postMessage({ id, ok: true });
      return;
    }
    const init = await ensureReady();
    if (type === 'init') {
      self.postMessage({ id, ...init });
      return;
    }
    if (type === 'search') {
      const pointer = Module.ccall('eunshin_search', 'number',
        ['string', 'number', 'number'], [fen, depth, moveTimeMs]);
      self.postMessage({ id, ...JSON.parse(decode(pointer)) });
      return;
    }
    if (type === 'evaluate') {
      const pointer = Module.ccall('eunshin_evaluate', 'number', ['string'], [fen]);
      self.postMessage({ id, ...JSON.parse(decode(pointer)) });
      return;
    }
    throw new Error(`Unknown engine request: ${type}`);
  } catch (error) {
    self.postMessage({ id, ok: false, error: String(error?.message || error) });
  }
};
