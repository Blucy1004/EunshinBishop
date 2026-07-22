# Official site integration

Copy `docs/play/` into the repository's existing `docs/` directory.

In `docs/index.html`, add this navigation link near the other section links:

```html
<a href="play/" class="nav-play">Challenge</a>
```

Add a hero or feature call-to-action:

```html
<a class="button button-secondary" href="play/">
  Challenge EunshinBishop
  <span aria-hidden="true">♞</span>
</a>
```

No changes to the existing homepage JavaScript are required. Live Engine Status is included in the playroom and reads the public GitHub API with an offline fallback.


## v1.6 fixes
- Fixed oversized drag ghost caused by container-relative font sizing after moving the piece to document.body.
- Drag ghost now uses an explicit square-relative pixel size.
- Board is inert and pointer-locked outside PLAYER_TURN.
- Any active drag and selection are cancelled on state transitions.


## v1.9 Global Final

The playroom now includes `docs/play/i18n.js` with 60 locale choices.
Opening names remain canonical Lichess-style English names, including
`Nimzo-Larsen Attack`.


## v1.10 — 20 languages

Replace the previous `docs/play` directory with this version.
The language selector has been reduced to 20 complete locales.
