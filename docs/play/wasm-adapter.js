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

    // The C++ evaluator reports from the side-to-move perspective (STM),
    // not permanently from White's perspective. Normalise it here so every
    // browser UI caller receives a stable White-POV score.
    const rawStmScore = Number(result.score) || 0;
    const sideToMove = String(fen).trim().split(/\s+/)[1];
    this.cachedEvalWhite = sideToMove === 'b' ? -rawStmScore : rawStmScore;
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

    // Search the position BEFORE the played move. This is the only valid
    // place to ask what the best move was.
    const best = await this.search(before, {
      level: opts.level || 'normal',
      depth: Math.min(opts.depth || 6, 6),
      moveTimeMs: 240
    });

    if (!best?.move || !best?.raw?.bestmove) {
      throw new Error('The engine did not return a best move for review.');
    }

    const after = new Chess(before.fen());
    const applied = after.move({
      from: move.from,
      to: move.to,
      promotion: move.promotion || 'q'
    });
    if (!applied) {
      throw new Error('The played move could not be reconstructed for review.');
    }

    // A static evaluation cannot see that a hanging queen is captured on the
    // very next move. Run one deliberately short reply search from the played
    // position. Its score is from the opponent's perspective, so negate it to
    // obtain the original mover's perspective.
    const reply = await this.search(after, {
      level: 'easy',
      depth: 4,
      moveTimeMs: 140
    });

    const playedScore = -(Number(reply.score) || 0);
    const bestScore = Number(best.score) || 0;
    const loss = Math.max(0, bestScore - playedScore);

    const playedUci =
      `${move.from}${move.to}${move.promotion && move.promotion !== 'q' ? move.promotion : ''}`
        .toLowerCase();
    const bestUci = String(best.raw.bestmove || '').toLowerCase();
    const isBest = playedUci === bestUci ||
      (move.promotion === 'q' && `${move.from}${move.to}q`.toLowerCase() === bestUci);

    return {
      best,
      playedScore,
      bestScore,
      loss,
      isBest,
      reply,
      after
    };
  }

  stop() { return this.request('stop', {}, 3000); }
}
