# Upstream comparison follow-up (October 13, 2026)

This records the disposition of the remaining items in the October 13
ChupathingyCE comparison (fork `5c6eeb89`, upstream `4b28c1a5`). It is a
scope ledger, not a policy to merge upstream wholesale. Code was integrated
as separate commits on `upstream-remaining-fixes`; fork-specific changes and
untracked local files were left alone.

## Priority 1: configuration

| Upstream commit | Disposition |
| --- | --- |
| `be1c8cca` | Applied as `4f117c09`: report changed settings left commented out. |
| `b3ee742c` | Applied as `9c54b30d`: skip bounded malformed lines and retain other settings. |
| `b1d3eaca` | Applied as `e91e90a9`: case-insensitive choices and unknown-choice logging. |
| `c5ace2ae` | Applied as `e49906b0`: preserve non-default legacy values. |
| `b4f73a74` | Applied as `0ef29122`: warn about misplaced config files. |
| `83f2bf18` | Applied as `daabc9a6`: headless dedicated-server SDL compatibility. |
| `80196e30` | Applied as `ec1202b1`: bounded case-insensitive comparisons. |

## Priority 2: gameplay and server

| Upstream commit | Disposition |
| --- | --- |
| `3c963a21` | Applied as `e417354a`: 32 KB desktop/server report buffer and whole-player JSON rollback when full (Xbox retains 8 KB). |
| `4c3e74cf` | Applied as `93da7697`: Custom Edition HUD button icons. |
| `db3fa793` | Applied as `225361b6`: opt-in `sv_name_filter`, settings/admin/deployment support, bounded name processing, and tests. |

## Priority 3: CI and documentation

| Upstream commit | Disposition |
| --- | --- |
| `73855622` | Applied as `33afe5e4`, resolving the workflow conflict while retaining this fork's macOS Metal artifact uploads. |
| `8e76f1b4` | Applied as `20d384b9`, preserving the fork's button-icon documentation and adding the system-font/resolution-scaling notes. |
| `e9b5cae0`, `044a25ef` | Adapted as `6bfb64fc`, `c82b9a51`: document manual network-version following and selective code ports without replacing fork-specific policy. |

## Already represented or intentionally not copied

- `33e59819` (weapon HUD) was already represented before this branch: secondary magazine overlays use slots 4/5, unused state is cleared, and single-magazine weapons do not emit those overlays.
- The profiler frame-buffer fix, bounded Unicode format parsing/format-call handling, and Escape-to-close-console behavior cited in the comparison were verified in the fork's existing `source/cseries/profile.c`, `source/text/unicode.c`, and `source/main/console.c` / `source/main/main.c`; no duplicate cherry-picks were made.
- Network version 28 is already set in `port/linux/include/halo_port_limits.h`.
- Upstream project-identity text was not copied wholesale into this fork's README/contributing guide. Internal `docs/bug-sweep.md` and `docs/parity.md` were retained under the fork's own documentation policy.
- Upstream-only items outside the stated remaining-work list were not swept in as part of this scoped integration.
- Updater signing is explicitly deferred at the user's direction. The all-zero placeholder in `port/linux/src/update_key.h` remains unchanged; upstream signing keys were not copied. Configuring a fork-owned key remains a separate TODO.

## Verification evidence and limits

- `git diff --check 5c6eeb89..HEAD` passed; the working diff has no tracked uncommitted changes.
- `python3 tools/test_config_tables.py` passed.
- `python3 tools/test_touch_layout.py` passed its touch layout and integration checks.
- `.github/workflows/build.yml` parsed with Ruby's YAML parser.
- `uv run --no-project --with pytest python -m pytest -q tools/test_server_control.py tools/test_config_tables.py` passed (9 tests), including the server name-filter C regression coverage under the test harness.
- `ninja linux` did not complete on the Apple M1 host: its `-march=native` resolves to `apple-m1`, which is not a valid CPU target for the i686 Linux cross-compile. This is an environment/toolchain limitation; a Linux build and full relevant tests remain to be run on a supported host/CI.
