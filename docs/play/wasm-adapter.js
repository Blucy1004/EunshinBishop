class EunshinWasmAdapter {
  constructor() {
    this.worker = new Worker('./engine-worker.js');
    this.nextId = 1;
    this.pending = new Map();
    this.cachedEvalWhite = 0;
    this.ready = this.request('init');
    this.worker.onmessage = event => {
      const pending = this.pending.get(event.data.id);
      if (!pending) return;
      this.pending.delete(event.data.id);
      if (event.data.ok === false) pending.reject(new Error(event.data.error || 'Engine error'));
      else pending.resolve(event.data);
    };
    this.worker.onerror = event => {
      for (const { reject } of this.pending.values()) reject(new Error(event.message || 'Engine worker failed'));
      this.pending.clear();
    };
  }
  request(type, payload = {}) {
    const id = this.nextId++;
    return new Promise((resolve, reject) => {
      this.pending.set(id, { resolve, reject });
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
    const result = await this.request('evaluate', { fen });
    this.cachedEvalWhite = result.score || 0;
    return this.cachedEvalWhite;
  }
  toVerboseMove(chess, uci) {
    if (!uci || uci === '0000') return null;
    const move = chess.move({ from: uci.slice(0,2), to: uci.slice(2,4), promotion: uci[4] || 'q' });
    if (!move) return null;
    chess.undo();
    return move;
  }
  pvToSan(chess, pv) {
    const clone = new Chess(chess.fen());
    const san = [];
    for (const uci of pv || []) {
      const move = clone.move({ from: uci.slice(0,2), to: uci.slice(2,4), promotion: uci[4] || 'q' });
      if (!move) break;
      san.push(move.san);
    }
    return san;
  }
  async search(chess, opts = {}) {
    await this.ready;
    const cfg = this.config(opts.level || level);
    const result = await this.request('search', {
      fen: chess.fen(),
      depth: opts.depth || cfg.depth,
      moveTimeMs: opts.moveTimeMs ?? cfg.moveTimeMs
    });
    const move = this.toVerboseMove(chess, result.bestmove);
    this.cachedEvalWhite = chess.turn() === 'w' ? (result.score || 0) : -(result.score || 0);
    return {
      move,
      score: result.score || 0,
      scoreWhite: this.cachedEvalWhite,
      candidates: [],
      pv: this.pvToSan(chess, result.pv),
      raw: result
    };
  }
  async scorePlayedMove(before, move, opts = {}) {
    const mover = before.turn();
    const best = await this.search(before, { level: opts.level || 'normal', depth: opts.depth, moveTimeMs: 500 });
    const after = new Chess(before.fen());
    const applied = after.move({ from: move.from, to: move.to, promotion: move.promotion || 'q' });
    if (!applied) return { best, playedScore: -30000, loss: 30000, reply: null, after };
    const reply = await this.search(after, { level: opts.level || 'normal', depth: Math.max(3, (opts.depth || 7) - 1), moveTimeMs: 450 });
    const playedScore = -(reply.score || 0);
    const bestScore = best.score || 0;
    return {
      best,
      playedScore,
      scoreWhite: mover === 'w' ? playedScore : -playedScore,
      loss: Math.max(0, bestScore - playedScore),
      reply,
      after
    };
  }
  stop() { return this.request('stop'); }
}
