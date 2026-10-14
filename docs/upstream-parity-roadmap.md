# Upstream parity roadmap checkpoint

This checkpoint records the work integrated on `upstream-remaining-fixes` and
what remains. It is a working status note, not a claim that platform-wide
acceptance is complete. Keep `HALO_PORT_NETWORK_VERSION` at 28 unless a
separately reviewed compatibility change is authorized. Do not import upstream
signing keys; signed-release key setup remains deferred.

## Integrated and committed

- `e709eb69` — selected native reliability, co-op, rendering, text, and
  configuration fixes.
- `346ffab5` — Custom Edition campaign map discovery, menu selection, and
  co-op launch.
- `783011c3` — split Android guest ABI, GLES renderer, and app defines, as a
  prerequisite for the web build.
- `5e3779d0` through `32eadb13` — WebAssembly/WebGL 2 browser build and site,
  touch controls, browser rooms over WebRTC, and native/browser internet-play
  signalling and transport. The native internet-play change is in signalling;
  the game's protocol remains version 28.

On macOS with Emscripten 6.0.10, `ninja web` linked `halo.js` and `halo.wasm`;
13 Node tests and 11 pytest tests passed. A test for native-to-browser WebRTC
connectivity was deliberately not run. Linux, Android, and Windows builds of
the integrated branch have not been checked here.

## Still to port or verify

1. **Map sharing (not implemented).** Adapt upstream Custom Edition map sharing
to Delta and network version 28. Design and test map identity, bounded and
validated transfers, cache installation, late joins, resource maps, trust
boundaries, and mixed-version fallback. Existing Delta map identity only
reports a mismatch; it does not transfer files.
2. **Native voice chat (not implemented).** The initial upstream voice commit
was inspected and its cherry-pick aborted because the fork has substantial
conflicts in configuration, UI, input, build scripts, and menus. Opus and
voice transport are not integrated. Port it as a separate project; confirm
wire IDs and legacy-peer behavior, and add bounded/error-path and regression
tests before enabling it. Do not count the aborted cherry-pick as work in the
branch.
3. **WebRTC/browser acceptance.** The browser build and native WebRTC code are
integrated, but the dedicated native-to-browser connectivity test was not
run. Run it on a suitable browser environment and investigate any failures.
4. **Platform/integration checks.** CI or appropriate hosts still need to
build Linux, Windows, and Android. Verify browser persistence, data import,
input, split-screen, browser rooms and documented limitations. Run the
integration audit for Delta, dedicated server, map families, bots, Infection,
and Forge; confirm protocol version 28 and compatibility.
5. **Other roadmap work.** Port selected visual improvements (first-person
legs and shield color/glow/flare); fix the bot-menu validation test; review
release metadata. Signing-key setup remains deferred unless separately
authorized.

## Checkpoint environment

The worktree used for web integration was fast-forwarded into
`upstream-remaining-fixes` and removed. Emscripten was installed under
`~/emsdk`; source `~/emsdk/emsdk_env.sh` to use it. Re-run `python configure.py`
after changing build configuration.
