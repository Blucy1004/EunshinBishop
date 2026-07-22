class EunshinWasmAdapter {
  constructor() {
    this.worker = new Worker('./engine-worker.js');
    this.nextId = 1;
    this.pending = new Map();
    this.cachedEvalWhite = 0;

    // Register handlers before the first request. A fast worker response must
    // never arrive before onmessage exists.
    this.worker.onmessage = event => {
      const pending = this.pending.get(event.data.id);
      if (!pending) return;
      clearTimeout(pending.timer);
      this.pending.delete(event.data.id);
      if (event.data.ok === false) pending.reject(new Error(event.data.error || 'Engine error'));
      else pending.resolve(event.data);
    };

    this.worker.onerror = event => {
      const error = new Error(event.message || 'Engine worker failed');
      for (const { reject, timer } of this.pending.values()) {
        clearTimeout(timer);
        reject(error);
      }
      this.pending.clear();
    };

    this.ready = this.request('init', {}, 30000);
  }

  request(type, payload = {}, timeoutMs = 15000) {
    const id = this.nextId++;
    return new Promise((resolve, reject) => {
      const timer = setTimeout(() => {
        this.pending.delete(id);
        reject(new Error(`Engine request timed out: ${type}`));
      }, timeoutMs);
      this.pending.set(id, { resolve, reject, timer });
      this.worker.postMessage({ id, type, ...payload });
    });
  }

  config(levelName) {
    if (levelName === 'easy') return { depth: 3, moveTimeMs: 120 };
    if (levelName === 'hard') return { depth: 10, moveTimeMs: 1800 };
    return { depth: 7, moveTimeMs: 650 };
  }

  evaluate() { return this.cachedEvalWhite; }

  async evaluateFen(fen) {
    await this.ready;
    const result = await this.request('evaluate', { fen }, 10000);
    this.cachedEvalWhite = Number(result.score) || 0;
    return this.cachedEvalWhite;
  }

  toVerboseMove(chess, uci) {
    if (!uci || uci === '0000') return null;
    const move = chess.move({ from: uci.slice(0, 2), to: uci.slice(2, 4), promotion: uci[4] || 'q' });
    if (!move) return null;
    chess.undo();
    return move;
  }

  pvToSan(chess, pv) {
    const clone = new Chess(chess.fen());
    const san = [];
    for (const uci of pv || []) {
      const move = clone.move({ from: uci.slice(0, 2), to: uci.slice(2, 4), promotion: uci[4] || 'q' });
      if (!move) break;
      san.push(move.san);
    }
    return san;
  }

  async search(chess, opts = {}) {
    await this.ready;
    const cfg = this.config(opts.level || (typeof level !== 'undefined' ? level : 'normal'));
    const depth = opts.depth || cfg.depth;
    const moveTimeMs = opts.moveTimeMs ?? cfg.moveTimeMs;
    const result = await this.request('search', {
      fen: chess.fen(),
      depth,
      moveTimeMs
    }, Math.max(15000, moveTimeMs + 10000));

    const move = this.toVerboseMove(chess, result.bestmove);
    if (!move && result.bestmove && result.bestmove !== '0000') {
      throw new Error(`Engine returned an illegal move: ${result.bestmove}`);
    }

    this.cachedEvalWhite = chess.turn() === 'w' ? (Number(result.score) || 0) : -(Number(result.score) || 0);
    return {
      move,
      score: Number(result.score) || 0,
      scoreWhite: this.cachedEvalWhite,
      candidates: [],
      pv: this.pvToSan(chess, result.pv),
      raw: result
    };
  }

  async scorePlayedMove(before, move, opts = {}) {
    const mover = before.turn();

    // Review must never block the actual game for several full searches.
    // One short best-move search plus a static evaluation of the resulting
    // position is enough for the browser review classification.
    const best = await this.search(before, {
      level: opts.level || 'normal',
      depth: Math.min(opts.depth || 6, 6),
      moveTimeMs: 260
    });

    const after = new Chess(before.fen());
    const applied = after.move({ from: move.from, to: move.to, promotion: move.promotion || 'q' });
    if (!applied) return { best, playedScore: -30000, loss: 30000, reply: null, after };

    const afterWhite = await this.evaluateFen(after.fen());
    const playedScore = mover === 'w' ? afterWhite : -afterWhite;
    const bestScore = best.score || 0;

    return {
      best,
      playedScore,
      scoreWhite: afterWhite,
      loss: Math.max(0, bestScore - playedScore),
      reply: null,
      after
    };
  }

  stop() { return this.request('stop', {}, 3000); }
}
