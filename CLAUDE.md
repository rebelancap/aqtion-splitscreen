# aqtion-splitscreen — project charter

`~/dev/CLAUDE.md` applies unless overridden here. Versioning follows the
program-wide rules (dev builds `0.0.0.N` until the maintainer picks a
release number; the version lives in `VERSION` at repo root, which
upstream's `version.py` already prints verbatim). `STATUS.md` at repo root
is kept current at the end of every round.

## What this is

A fork of the AQtion client engine, [actionquake/q2pro](https://github.com/actionquake/q2pro)
(remote `upstream`, their branch `aqtion`; our work on branch
`splitscreen`), adding local splitscreen (4 players first, then 8) and
gamepad support with automatic pad assignment and hold-to-join: the same
product as the finished sibling project `../Quake3e-splitscreen`. The Steam
"AQtion" is this engine: `aqtion.exe` is a Steam-shim launcher and
`q2pro.exe` is the client. Must work with the unmodified Action game DLL
(`action/gamex86_64.dll`, built from the separate aq2-tng project) and,
eventually, on unmodified online AQ2 servers.

## Architecture decision (2026-10-08)

**Multi-client-instance, same as Quake3e-splitscreen.** Each local player
is a complete virtual client inside one process: its own `client_state_t`,
its own connection half of `client_static_t`, its own netchan (own UDP
socket + qport), rendered into its own viewport cell. The game DLL and the
server each see N ordinary players, so the Action mod and remote servers
work unchanged. Renderer, sound device, filesystem, engine menus and
console stay single and shared.

What is different from Q3, and why this is the easier port: Quake 2 has
no client-side game module. HUD, scoreboard and Action's team/weapon menus
are server-sent layouts that already live in per-connection state, so
there is no VM instancing at all. The price is more file-level static
state in the Q2PRO client (inventoried in the design doc) and a renderer
that has no 2D viewport origin today (one addition, `R_SetDrawRect`).

Keep the diff against upstream contained: new files under
`src/client/split/` (and a gamepad backend file) plus thin hooks in
existing ones, so upstream merges stay cheap and a PR remains thinkable.
The Q3 fork is the reference for UX and policy decisions (profiles, guest,
join hint, aim feel, aim assist, Independent mode, refusal wording):
reuse them by section number, do not re-derive them.

## Documents

- `docs/BUILD-AND-PROVENANCE.md` — which upstream commit the Steam build
  is (tag `9eb6182`, 2023-10-13; `aqtion` HEAD is 342 commits ahead and
  is our base), the Meson/MSVC build recipe, paths, Steam/Discord, the
  game DLL, AQtion-specific client features. Read before the first build.
- `docs/SPLITSCREEN-DESIGN.md` — the implementation design: context
  model, `cls` split, static inventory, frame flow, viewport
  virtualization, networking, input, sound, menus, risks, milestones
  (Spikes A/B/C/P, M1–M4) and open decisions. Executors read the sections
  their brief names.

## Overrides of ~/dev/CLAUDE.md

- **Testing (overrides the VR testing paths):** not a VR project; ignore
  SteamVR/null-driver/simulator. Agents test with a windowed run
  (`+set vid_fullscreen 0 +set vid_geometry 1280x720`), evidence via the
  engine's own `screenshot` command and the console log (`+set logfile 2`),
  copied into `work/`. Extra local players can be driven without pads via
  console commands / bots. Real multi-gamepad checks are the maintainer's;
  put them in STATUS.md Next steps as a checklist. The "one game instance",
  "don't interrupt the maintainer's screen" and cleanup rules still apply.
- **Delivery:** no game folder is patched; the build is a standalone
  engine exe (see Game data). Dev builds land in `build/`; an install script (when a game
  dir is known) only copies our exe + `BUILD-INFO.txt` beside the Steam
  files and never touches existing files. Never replace `q2pro.exe`,
  `aqtion.exe` or any DLL in the install.
- **Toolchain:** the program-wide "no CMake/Ninja/Python" machine fact is
  stale for this box: CPython 3.13, Ninja and CMake are installed; Meson
  is the only missing prerequisite (`python -m pip install --user meson`,
  needs the maintainer's OK). Build is Meson + MSVC 2019 x64 via
  `scripts\build.ps1` → `build\` (to be written in Spike A; the native
  file must point `python3` at the real CPython because `python3` resolves
  to the Store stub). Wraps are fetched on the first configure and cached
  in `subprojects/packagecache/` (gitignored).

## Game data

No game data is committed, ever. The Steam install is read-only as
`+set basedir "$AQ_BASEPATH"` (`$AQ_BASEPATH` = the AQtion Steam folder,
configured locally outside the repo), always with
`+set homedir <repo>\work\aqhome +set game action` so configs, logs and
screenshots land in our tree and the install is never written.
`scripts\run.ps1` wraps this. **Listen servers use Steam's own game
library** (`action\gamex86_64.dll` / `.so`, TNG 2.82, legacy game API 4)
through the adapter in `src/server/game.c` (since 0.0.0.22; Spike A's
"Steam DLL crashes on map load" was the missing `CheckForExtension`
import, now supplied). Releases ship our exe and a tiny README only, no
game library; `build\gamex86_64.dll` from `src/action` is for dev runs via
`sys_forcegamelib` only. An exe placed in the AQtion root uses the install
as basedir and `<AQtion>\splitscreen\` as homedir (`split_self_locate()`;
a subfolder placement uses `<exe dir>\home`). Two known
leaks to watch (design doc): bot nav paths are relative to the working
directory, and crash dumps go beside the exe. The sibling
`../Quake3e-splitscreen` repo is read-only reference for the pad layer
(`code/client/in_gamepad.c`), the split files (`code/client/cl_split*.c`)
and the maintainer's decisions in its design doc sections 12–17.

## Git identity

Commit as `rebelancap <rebelancap@protonmail.com>` only.

Public repo: no personal names, user paths, e-mails or host names in files
or commit messages; use `<repo>`, `<AQtion install>`, "the maintainer",
`$AQ_BASEPATH`.
