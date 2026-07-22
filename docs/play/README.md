# Challenge EunshinBishop

Browser-playable website preview containing:

- Chess.com-inspired responsive board
- Easy / Normal / Hard browser engine levels
- Undo one complete turn
- Hint highlighting and expected reply
- Live evaluation bar
- Per-move Best/Excellent/Good/Inaccuracy/Mistake/Blunder review
- Plain-language move explanations
- SAN move list and PGN copy
- Live GitHub release/repository status

## Run locally

The GitHub API and external chess.js script work best over HTTP rather than by opening `index.html` directly.

```bash
cd docs
python -m http.server 8080
```

Open `http://localhost:8080/play/`.

## WASM integration point

`BrowserEngineAdapter` in `play.js` deliberately owns `evaluate()` and `search()`.
Replace it with a Web Worker/UCI adapter that loads the Emscripten output while leaving the board, controls, move review and status UI unchanged.

## Production note

For a fully self-contained release, vendor `chess.min.js` into `docs/play/vendor/` and update the script source in `index.html`.


## v1.3
Added explicit pre-game, starting, player-turn, engine-thinking, reviewing, and game-over states. The board DOM is initialized once and updated incrementally.


## Global final localization

- 60 selectable locales
- Browser-language auto detection
- Language selection persisted in localStorage
- RTL layout for Arabic, Hebrew, Persian, and Urdu
- Canonical Lichess-style English opening names are intentionally preserved
- Core gameplay UI is localized; untranslated engine-specific detail falls back to English


## v1.10 — 20-language final

Supported languages:
English, Korean, Japanese, Simplified Chinese, Traditional Chinese,
Spanish, French, German, Portuguese, Italian, Russian, Ukrainian,
Polish, Dutch, Turkish, Arabic, Hindi, Indonesian, Vietnamese, Thai.

Dynamic review text, status messages, hints, result labels, controls,
and PGN notifications now use the localization layer.


## v1.11 Korean final

Korean localization was manually revised by the project owner.
