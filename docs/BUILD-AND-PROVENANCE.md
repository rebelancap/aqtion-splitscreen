# AQtion client: build and provenance notes

Investigation of the AQtion engine source (`actionquake/q2pro`, a fork of
skullernet/q2pro) ahead of adding multi-client-instance local splitscreen.
All findings are from a read-only pass over `<repo>` (branch `aqtion`,
HEAD `39f038ff`, 2026-05-23) and the shipped Steam binaries in
`<AQtion install>`. Line numbers refer to `aqtion` HEAD unless a commit is named.

## 1. Provenance

**The Steam build is upstream commit `9eb61824` (tag/release `9eb6182`,
2023-10-13), PR #128 "Hotfix 1".**

- The banner `AQtion r1~c981fcb Oct 13 2023 Win64 x86_64` comes from
  `version.py:10-13` (`r<rev-list --count>~<short sha>`). `r1` means the CI
  checkout was shallow (`actions/checkout` default depth 1), so the revision
  number is meaningless.
- `c981fcb` is not in any branch because it is GitHub's synthetic
  pull-request merge commit (`refs/pull/128/merge`), produced when
  `build.yml` runs on a `pull_request` event (`.github/workflows/build.yml:3-7`).
  The GitHub API returns it as "Merge 9eb61824… into 6f2454fc…", committed
  2023-10-13T23:18Z. `compare 9eb61824...c981fcb` gives **0 changed
  files**, so the shipped source tree is exactly `9eb61824`.
- PR #128 (`hotfix-1` → `aqtion`) merged as `f2d0f82b` on 2023-10-17. Its
  last two commits are `28a35b0b` "Disable cl_discord" (default `cl_discord`
  set to `0`) and `9eb61824` "Cherry picking". The release `q2pro_9eb6182`
  (published 2023-10-13 23:47Z) has `q2pro-msvc-x64.zip` and `q2pro-mingw-x86_64.zip`.
- The Steam `q2pro.exe` is the **MSVC** artifact: `dumpbin /dependents` shows
  `VCRUNTIME140.dll` + UCRT, `discord_game_sdk.dll`, `OPENGL32`, `WINMM`,
  `WS2_32`, `bcrypt`, `CRYPT32`. There are no `DSOUND`, `libEGL.dll`, or
  `gamex86_64.dll` imports; those are loaded at runtime with `LoadLibrary`
  (`libEGL.dll` appears only as a string). The `9eb6182` workflow built it
  with `-Ddiscord-sdk=true` (`git show 9eb61824:.github/workflows/build.yml:20`).
  The PDB path `D:\a\q2pro\q2pro\…` is the `windows-latest` runner.
- The tags `r548`…`r1861` are old skullernet SVN-era revisions (2011–2020)
  and are irrelevant. Release tags since 2023 are short SHAs.

**How far `aqtion` has moved since then:** 342 commits (315 non-merge) in
`9eb61824..upstream/aqtion`. First-parent history has only 19 merges. The
big ones are #134/#137 (Oct–Nov 2023, skullernet sync + protocol
backward compatibility), a 2025-11 CI/subproject refresh with MSVC meson
fixes (`4fb60d6f`, `b65b5a7a`), **v1.3.8 Release (#143, `4d6f5d1c`,
2026-03-04)**, force_cl_xerp (#305), and the #316 aqtion-alpha sync
(2026-05). The total diff is 1022 files, +114k/−28k. The engine-only part
(`src/client src/common src/windows src/unix inc meson*`) is 135 files,
+14.8k/−8.0k. Notable changes:

- Protocol: AQtion minor protocol went from `3014` (GHUD2) at `9eb6182` to
  `3017` (`inc/common/protocol.h:66-76`: CLIENTNUM_SHORT 3015, CINEMATICS
  3016, EXTENDED_LIMITS 3017). The skullernet "extended" protocols
  (Q2PRO 1024-1026, 3434-3436) arrived too (`protocol.h:35-38,56-59`).
  The major protocol is still `PROTOCOL_VERSION_AQTION 38` (`protocol.h:32`).
- Rerelease support: MD5 models, cinematics via libavcodec, and the new
  game API `3302` (`inc/shared/game.h:28-38`). The engine is always built
  with `USE_NEW_GAME_API=1` (`meson.build:300`) but still accepts API
  3/4 game DLLs (`src/server/game.c:1176`). `rerelease-1` itself is a stale
  2023-10-11 branch and is not needed.
- `feat/steamshim` (2024-02) is a divergent experiment (962 files differ,
  −100k lines). It adds only a toy launcher `steamshim/steamshim.c` that
  spawns `.\q2pro.exe`. Ignore it.
- `master` (2023-07) and `master-presync-archive` (2023-01) are stale.
  `aqtion-alpha` = `aqtion` + 199 commits (TNG game-code syncs,
  scoreboard staggering). It is the integration branch for the next release.

**Recommendation: fork from `upstream/aqtion` HEAD (`39f038ff`, ≈ v1.3.8 +
fixes)**, with a branch `splitscreen`, and merge `aqtion` (not
`aqtion-alpha`) periodically. Implications:

- *Live servers.* The client advertises its max minor version and the
  server clamps it to its own (`src/server/main.c:1273-1281`). The client
  accepts any server minor in `3011..3017` (`src/client/parse.c:665-672`,
  `AQTION_SUPPORTED`, `protocol.h:92-94`). So a HEAD client works against
  both 2023-era (3014) and current (3017) AQtion servers. Plain
  Q2PRO/R1Q2/34 servers work too.
- *Steam game data.* `baseaq/` and `action/` paks are data only and are
  unaffected by engine version. **Correction (Spike A, measured):** the
  Steam `action/gamex86_64.dll` (TNG 2.82, API 4) does **not** work with
  a HEAD engine. Its `GetGameAPI` copies 45 pointers of `game_import_t`
  and calls the last one, `CheckForExtension("Client_GetProtocol")`.
  Upstream removed that member in `4d6f5d1c` (v1.3.8; it moved to
  `GetGameAPIEx`), so HEAD's struct has 44, the call goes through NULL
  and the client crashes on map load. Listen servers must use our own
  build of `src/action` (see 6). Remote servers are unaffected.
  **Fixed in 0.0.0.20:** its game_export_t also differs (an extra
  `FetchGameExtension` before `edicts`). `SV_InitGameProgs` now hands a
  library without `GetGameAPIEx` the import with `CheckForExtension`
  appended and reads an API-4 one's exports through an adapter
  (`game_export_aq4_t`, design doc §8). The Steam DLL hosts listen and
  dedicated games again and the release exe uses it (self-locate sets
  `libdir` to the AQtion folder); our own DLL is unaffected.

## 2. Build system

**Meson only** (`meson.build`, `meson_options.txt`, `meson_version >= 0.59.0`,
`meson.build:4`). There is no CMake. `AQtion/AQtion.vcxproj` is an NMake
wrapper that just calls `meson compile`
(`AQtion/AQtion.vcxproj:66-94`, `AQtion/meson_release_configure.bat`).

- **MSVC is supported and CI-tested.** The `elif cc.get_id() == 'msvc'`
  branch at `meson.build:348-349` only disables warnings C4146/C4244/C4305.
  The rest is handled in source: `inc/shared/atomic.h:21-26` (volatile
  shim, since MSVC lacks `stdatomic.h`), `inc/shared/platform.h:151+`
  (`__declspec` equivalents) and `inc/shared/shared.h:26-35` (pragmas).
  `build.yml` has a `msvc` job (`windows-latest`, `ilammy/msvc-dev-cmd`,
  `pip install meson ninja`, `ilammy/setup-nasm`; `build.yml:82-114`).
  It passed on `aqtion` HEAD (run 26347590373, all 8 jobs green including
  `msvc (x64)` and `mingw (x86_64)`). CI uses VS 2022. VS 2019 (MSVC
  19.29) supports `/std:c11` (the default is `c_std=c11`, `meson.build:6`),
  but CI does not exercise it. That is the main toolchain risk.
- **Official Windows release artifacts:** `release.yml:62-81` ships both
  `q2pro-msvc-x64.zip` and `q2pro-mingw-x86_64.zip`. The MinGW job is a
  cross-compile on Ubuntu 24.04 (`build.yml:45-80`, cross file
  `.ci/x86_64-w64-mingw32.txt`). The nightly is MinGW only (`.ci/nightly.sh:89-92`).
  The Steam exe is the MSVC flavour.
- **Windows CI options** (`build.yml:11-29`): `--auto-features=enabled
  --fatal-meson-warnings -Danticheat-server=true -Davcodec=disabled
  -Dclient-gtv=true -Dpacketdup-hack=true -Dtests=false -Dvariable-fps=true
  -Dwerror=false -Daqtion-build=true -Ddiscord-sdk=false -Dsdl2=disabled
  -Dx11=disabled -Dwayland=disabled -Dwindows-egl=true -Dwrap_mode=forcefallback`.
- **Version:** `project(version: run_command(find_program('python3'),
  'version.py'))` (`meson.build:3`). Meson needs a working `python3`. On
  this box `python3` on PATH is the Windows Store stub, which fails. A
  real CPython 3.13 is installed (`python.exe` only). Fix this with a
  native file that pins `python3` (below). A `VERSION` file at repo root
  overrides the git-derived string (`version.py:5-7`).
- **Subprojects** (`subprojects/*.wrap`, all built static via
  `default_library=static`, `meson.build:364`). Wraps are downloaded at
  configure time, so the **first configure needs network**. The tarballs
  are cached in `subprojects/packagecache`.

  | wrap | what | source |
  |---|---|---|
  | `zlib.wrap` | zlib-ng 2.2.4 (zlib-compat) | github + wrapdb patch |
  | `libpng.wrap` | libpng 1.6.50 | github + wrapdb patch |
  | `libjpeg-turbo.wrap` | 3.1.0, built with `jpeg-turbo=disabled` (`meson.build:423-426`) | github + wrapdb patch |
  | `libcurl.wrap` | curl 8.15.0, local meson port in `subprojects/packagefiles/curl` | curl.se |
  | `jansson.wrap` | jansson 2.14 (required by game DLL, `meson.build:450-458`) | github; patch zip from `raw.githubusercontent.com/actionquake/q2pro/aqtion/extern/jansson_patch.zip` (also in-tree at `extern/jansson_patch.zip`) |
  | `openal-soft.wrap` | **headers only** (`packagefiles/openal-soft/meson.build`); OpenAL32.dll is loaded at runtime (`src/client/sound/qal.c`) | github |
  | `khr-headers.wrap` | KHR/EGL headers, local tarball `subprojects/packagefiles/khr-headers.tar.xz`, no network | in-tree |

  There is no SDL2 wrap, and Windows builds have no SDL2 (see 3). FFmpeg
  (`avcodec`) is disabled on Windows CI. NASM is installed by CI. With
  `jpeg-turbo=disabled` it should not be required, but this is unverified.

### Toolchain state on this box (checked, nothing installed)

Present: VS 2019 (MSVC 14.29.30133, Windows SDK 10.0.19041 and 10.0.26100),
CPython 3.13.5 (`python`, not `python3`), Ninja 1.13.2 (winget link),
CMake 4.4.3 (unused). Missing: **meson**, NASM (maybe needed).

### Recipe A (recommended): native MSVC 2019 + meson on this box

Prerequisites: `python -m pip install --user meson` (pip into the existing
CPython, about 3 MB). If you prefer winget: `mesonbuild.meson` (MSI, ships
its own Python). Optional if configure asks for it: `winget install NASM.NASM`.

Commands (PowerShell; do not run before approval):

```powershell
# 1. native file pins python3 (Store stub shadows it on PATH)
@"
[binaries]
python3 = '$((Get-Command python).Source -replace '\\','/')'
"@ | Out-File -Encoding ascii <repo>\build\native.ini

# 2. configure + build from an x64 VS 2019 dev shell
cmd /c "`"C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat`" && ^
  cd /d <repo> && ^
  python -m mesonbuild.mesonmain setup build\msvc --native-file build\native.ini ^
    --buildtype=debugoptimized -Dwrap_mode=forcefallback ^
    -Daqtion-build=true -Ddiscord-sdk=false -Davcodec=disabled ^
    -Dsdl2=disabled -Dx11=disabled -Dwayland=disabled -Dwindows-egl=true ^
    -Dtests=false -Dwerror=false && ^
  python -m mesonbuild.mesonmain compile -C build\msvc"
```

Outputs: `build\msvc\q2pro.exe`, `q2proded.exe`, `gamex86_64.dll`. This
should become `scripts\build.ps1`. Drop `--auto-features=enabled` and
`--fatal-meson-warnings` locally so a missing optional dep does not abort
the build. If VS 2019 cannot compile HEAD, the fallback is VS 2022 Build
Tools (`Microsoft.VisualStudio.2022.BuildTools` + C++ workload), which is
exactly what CI uses. That is a system-wide install and needs the
maintainer's approval.

### Recipe B: MinGW cross-build on the Linux box (Bazzite)

Upstream does build this way (`build.yml:45-80`). Bazzite is an immutable
OS, so build inside a Fedora toolbox/distrobox:

```sh
toolbox create aqbuild && toolbox enter aqbuild
sudo dnf install -y mingw64-gcc mingw64-winpthreads-static nasm meson ninja-build git python3
cd <repo>
meson setup --cross-file=.ci/x86_64-w64-mingw32.txt --buildtype=debugoptimized \
  -Dwrap_mode=forcefallback -Daqtion-build=true -Ddiscord-sdk=false \
  -Davcodec=disabled -Dsdl2=disabled -Dx11=disabled -Dwayland=disabled \
  -Dwindows-egl=true -Dtests=false -Dwerror=false build/mingw
meson compile -C build/mingw
```

Then copy the three binaries back to Windows for testing. This path is
good as a second CI-equivalent check and for a native Linux build later,
but Recipe A keeps edit-build-test on one machine and debuggable with
VS. **Recipe A is recommended.**

### Recipe L: native Linux x86_64 build (0.0.0.32+, branch `linux`)

```sh
distrobox create -n aqdev -i registry.fedoraproject.org/fedora:43 -Y
distrobox enter aqdev -- sudo dnf install -y gcc meson ninja-build pkgconf-pkg-config \
  git python3 SDL2-devel libcurl-devel zlib-devel libjpeg-turbo-devel libpng-devel \
  openal-soft-devel libogg-devel libvorbis-devel wayland-devel wayland-protocols-devel \
  libX11-devel libXi-devel libXext-devel mesa-libGL-devel mesa-libEGL-devel \
  jansson-devel libxkbcommon-devel libdecor-devel libuuid-devel
scripts/build.sh            # from the host; re-enters aqdev -> build/linux/q2pro-splitscreen
scripts/package.sh          # clean release build -> build/aqtion-splitscreen-<VERSION>-linux-x86_64.tar.gz
export AQ_BASEPATH=<AQtion install>
scripts/run.sh --map urbanjungle --timeout 90 -- +set am 1 +set am_botcount 4
scripts/run.sh --gamescope ...   # nested Gamescope window at Steam Deck size (1280x800)
```

`build.sh` keeps the Meson dir (incremental; `--clean` for a fresh one,
about 10 s) and writes `build/linux/BUILD-INFO.txt` (version, git,
configuration, options); `package.sh` refuses a build whose BUILD-INFO
is not `version: <VERSION>` / `configuration: release`. The tarball holds
`q2pro-splitscreen` (0755) and `README.txt` (`@VERSION@` substituted, as
`package.ps1` does) at its root, owner 0:0. The Windows counterpart,
`scripts\package.ps1` (clean MSVC build), writes
`build\aqtion-splitscreen-<VERSION>-windows-x86_64.zip`:
`q2pro-splitscreen.exe` and the same `README.txt` with CRLF line ends (the
zip was `aqtion-splitscreen-<VERSION>.zip` up to 0.0.0.33).

**glibc:** a Fedora 43 build needs GLIBC_2.38 (only gcc 15's C23
`__isoc23_sscanf` / `strtol` / `strtoul`), no libstdc++. Release binaries
are built by CI on Ubuntu 22.04 (glibc 2.35), which runs on any SteamOS 3
and current desktop distros. All other libraries are the system's own
(SDL2, libcurl, libpng, libjpeg-turbo, zlib, libGL/libEGL, Wayland,
libdecor, X11/Xi). Steam puts its old runtime libcurl first for games and
non-Steam shortcuts: the game starts, but an HTTP download (online, missing
files) would stop it on `curl_multi_poll` (`ldd -r`, not run; AQtion's own `launch.sh` works
around the same thing). An Ubuntu build links `libjpeg.so.8` (Steam's
runtime and SteamOS have it; Fedora / Bazzite only `.so.62`, so it would
not start there outside Steam, like AQtion's own `q2pro`): STATUS Open
questions.

## 3. SDL2 and input on Windows

- `src/unix/video/sdl.c` is generic SDL2 code (includes `<SDL.h>` plus
  engine headers only, `sdl.c:23-36`). Meson compiles it whenever
  `dependency('sdl2')` is found, with no OS restriction
  (`meson.build:385-389`). It registers `vid_sdl` after `vid_win32wgl` /
  `vid_win32egl` in `src/client/refresh.c:242-258` (`vid_driver` cvar
  picks one). SDL software sound uses the same gate (`meson.build:525-532`).
  On Windows, CI forces `-Dsdl2=disabled`, and there is no `sdl2.wrap`, so
  enabling it would need a wrap (wrapdb has `sdl2`) or prebuilt SDL2 dev
  libs. The Windows+SDL combination is untested upstream.
- **There is no gamepad or joystick support at all.** No `joy`, `gamepad`,
  `SDL_GameController`, `XInput`, or `DirectInput` exists anywhere in
  `src/` or `inc/` outside game code. `sdl.c` initialises only
  `SDL_INIT_VIDEO` (`sdl.c:281`), and its event loop handles only
  keyboard/mouse/window events (`sdl.c:484-510`). The key space ends at
  the mouse buttons and wheel (`inc/client/keys.h`, `K_MWHEELUP 211`),
  with no `K_JOY*`/`K_PAD*` codes. Input cvars are `in_enable`, `in_grab`
  (`src/client/input.c:188-201`) and `m_*`/`m_autosens`
  (`input.c:719-725`).
- Native Windows input: the window procedure in `src/windows/client.c`
  handles keyboard through `WM_KEYDOWN`/`WM_SYSKEYDOWN` (`client.c:854-855`). The mouse uses
  **Raw Input** (`RegisterRawInputDevices`, HID generic mouse,
  `client.c:1081-1094`; `WM_INPUT` → `raw_input_event`, `client.c:710-725, 815-817`).
  There is no DirectInput. So gamepad support is new code (an `in_gamepad.c`
  equivalent, XInput or SDL GameController), as in the Quake3e fork.

### As built, Spike P (2026-10-08)

- **Route that worked: the WrapDB `sdl2` wrap, first try.** `subprojects/sdl2.wrap`
  = WrapDB `2.32.8-1` (SDL2 2.32.8 release tarball + WrapDB's Meson port;
  it is a native Meson build; CMake 4.4 is only used by Meson to probe
  for the optional `libusb-1.0`, which is not found and not needed on
  Windows). It builds with MSVC 2019 14.29 x64 as a static lib
  (`default_library=static` via `fallback_opt`), 0 compiler warnings
  inside SDL; that libusb probe prints 5 harmless Meson "Unknown
  generator expression" warnings. No SDL2main is linked.
- **Option:** `-Dgamepad=auto|enabled|disabled` (`meson_options.txt`),
  independent of `-Dsdl2` (which stays `disabled` on Windows, so
  `vid_drivers` and the sound backends are unchanged). `meson.build`
  reuses the engine's SDL2 when `sdl2` found one (Linux), otherwise does
  its own `dependency('sdl2')` with `use_video=disabled`,
  `use_audio=disabled`, `use_render=disabled`, `test=false`. SDL's
  `SDL_config.h` confirms `SDL_VIDEO_DISABLED`, `SDL_AUDIO_DISABLED`,
  `SDL_RENDER_DISABLED`, and joystick drivers WGI, DInput, XInput,
  RawInput, HIDAPI and virtual; haptic (DInput/XInput), sensor, threads
  and timers stay on. The lib is linked into `q2pro.exe` only, and the
  config define is `USE_GAMEPAD`. `scripts\build.ps1` passes
  `-Dgamepad=enabled`.
- **Sizes:** `libSDL2.a` 8,345,042 bytes (static archive). `q2pro.exe` is
  3,307,520 bytes with gamepad support and 2,230,784 without, so SDL adds
  1,076,736 bytes (about 1.0 MiB). `q2proded.exe` and the game DLL are
  unchanged.
- **Runtime:** `src/client/split/in_gamepad.c` (`PAD_Init` /
  `PAD_Shutdown`, hooked after `IN_Init` / `IN_Shutdown` in
  `src/client/main.c`) sets the Quake3e fork's hints (background events,
  joystick thread, positional buttons, no signal handlers), calls
  `SDL_SetMainReady` and `SDL_InitSubSystem(JOYSTICK | GAMECONTROLLER)`
  only, with the event queues set to ignore (no `SDL_PumpEvents`, nothing
  touches the Win32 window), logs the pads, and registers `padlist`.
  `shared.h` and `SDL_config.h` both define `HAVE__STRICMP`, so
  `in_gamepad.c` undefines it before `<SDL.h>` (C4005 otherwise).
- **Fallbacks not needed:** runtime `LoadLibrary("SDL2.dll")` and XInput
  stay documented in the design doc (section 5.2) but are not implemented.

## 4. Steam and Discord

- **Launcher:** `aqtion.exe` (25 KB, MSVC 2022, imports `steam_api64.dll`)
  is an icculus-style **steamshim parent**. Its strings include `Parent
  starting mainline`, `STEAMSHIM_READHANDLE/WRITEHANDLE`, and
  `SHIMEVENT_SETACHIEVEMENT…`. It initialises Steamworks, creates pipes,
  and launches the child. Its source is **not in this repo**: the PDB
  points to an out-of-tree "steamshim … msvc_2022" project. The only
  in-repo shim is the unrelated toy `steamshim/steamshim.c` on
  `feat/steamshim`. The shipped `q2pro.exe` contains no `STEAMSHIM`
  strings and does not import steam_api. Steam identity reaches the engine
  only through the NOSET cvars `steamid`, `steamcloudappenabled`, and
  `steamclouduserenabled` (`src/common/common.c:928-930`, userinfo copies
  at `src/client/main.c:2881-2883`). These presumably arrive as `+set` on
  the command line.
- **SteamCloud:** if all three are non-zero, the FS adds the game dir
  `./SteamCloud/<steamid>/<game>`, relative to the **current directory**
  (`src/common/files.c:3551-3569`). With defaults (`0`) it is skipped.
- **Discord:** `src/client/discord.c` plus `#if USE_DISCORD && USE_CURL &&
  USE_AQTION` blocks in `src/client/main.c:136, 948, 2786, 3466` and
  `client.h:1135-1150`. The SDK is under `extern/discord/` (headers and
  import libs). Meson option `discord-sdk` defaults to **false**
  (`meson_options.txt`, `meson.build:585-601`). Current CI builds with it off.
- **What breaks with neither:** nothing functional. Gameplay, servers,
  and data are unaffected. We lose Discord rich presence/join,
  achievements/stats, SteamCloud config sync, and the `steamid` userinfo
  key. Some servers or stats might read `steamid`; that is unverified. Our
  exe is launched directly, not through `aqtion.exe`, and must not be
  named `q2pro.exe` inside the Steam folder (we never write there anyway).

## 5. Game data and paths (Windows)

- `Sys_Main` **always chdirs to the exe's directory**
  (`fix_current_directory`, `src/windows/system.c:1136-1156, 1167`).
- `basedir` = `"."`, `libdir` = `"."`, `homedir` = `""`, all `CVAR_NOSET`
  (`system.c:903-907`), so they can be set only on the command line (`+set`
  is processed as an early command, `src/common/common.c:784-795, 986-990`).
- Search paths: `<basedir>/baseaq` (+ `.kpf`), then `<homedir>/baseaq`, then
  `<basedir>/<game>` and `<homedir>/<game>`, with home overriding base
  (`files.c:3539-3584`). BASEGAME is `baseaq` and the default game is
  `action` (meson options `base-game` / `default-game`,
  `meson.build:660-661`). The write dir `fs_gamedir` is `<homedir>/…` when
  homedir is set and **`<basedir>/…` otherwise** (`files.c:3593-3600`).
  Configs, `logs/console.log` (`common.c:239-249, 921-923`), screenshots,
  demos, and `.conhistory` all go there. The Steam install already
  contains `action/.conhistory` and `items.txt`, which shows the default
  build writes into basedir.
- **Run line for our build (read-only install):**

  ```
  build\msvc\q2pro.exe +set basedir "<AQtion install>" +set libdir "<AQtion install>"
      +set homedir "<repo>\work\aqhome" +set game action
      +set logfile 2 +set vid_fullscreen 0 +set vid_geometry 1280x720
  ```

  With `homedir` set, nothing is written into basedir by the engine.
  Remaining writes outside homedir:
  1. Crash reports `Q2PRO_CrashReportNN.txt` go next to the **exe**
     (`src/windows/debug.c:268-291`).
  2. SteamCloud dir relative to CWD, only if `steamid` is set.
  3. **The game DLL's bot code uses CWD-relative paths.** Examples are
     `.\action\bots\nav\<map>.nav` (`src/action/botlib/botlib_nodes.c:2889-2911`,
     the same at 3008/3279/3565/3727) and `items.txt`
     (`acesrc/acebot_items.c:935`, DEBUG only). Because CWD is the exe dir,
     bot navmeshes are read from and written to `build\msvc\action\bots\…`,
     not the install. That is harmless to the install, but bots will lack
     nav data unless it is copied or junctioned into the build dir.

### Agent testing: offscreen window (0.0.0.24)

The maintainer plays on this box's only physical display, so agent test runs
never take focus there. `vid_noactivate 1` (Windows only,
`src/windows/client.c`; read when the window is created, default 0 =
upstream behaviour) makes a `WS_EX_NOACTIVATE` window placed with
`SWP_NOACTIVATE` at the bottom of the z-order, never calls
`SetForegroundWindow` / `SetFocus`, never grabs, warps or hides the mouse,
ignores `vid_fullscreen` and never calls `ChangeDisplaySettings`, installs no
alt-tab hotkeys / keyboard hook / gamma ramp, ignores mouse moves over it,
and tells the client it is active (`CL_Activate(ACT_ACTIVATED)`), so the
frame loop runs at full rate (120 fps measured while never focused) and
screenshots render even when the window is covered. It receives no keyboard
input: drive it with cfg scripts, `padinject` and console commands.
`scripts\run.ps1 -Offscreen` adds `vid_noactivate 1`, `vid_fullscreen 0`,
`s_enable 0` (no sound device) and a 1280x720 `vid_geometry` on an attached
virtual display (Sunshine / Parsec / SudoMaker / IDD); with none attached it
lists the adapters and stops, or with `-AllowPrimary` puts the window at the
bottom-right corner of the primary display. It never changes display
settings. Every run.ps1 run adds `pad_enable 0` (no real pad is opened; since
0.0.0.24 SDL's controller code is not even started) unless `-Pads`. With
`-Wait` it samples the foreground window every 100 ms and prints whether the
game ever had focus. `-Exe` / `-SelfLocate` / `-HomeDir` / `-WorkDir` /
`-LogName` cover the Steam-folder placement tests (a test-named exe copied
into `$AQ_BASEPATH`, removed afterwards). Note for cfg scripts: a command a
menu item runs (e.g. AQtion's native "Play") is queued behind the rest of
the running script; continue from `cl_beginmapcmd` instead
(`work/menus-cwd22-steam-root.cfg` + `-2.cfg`).

## 6. The Action game DLL

- **Source is in this repo:** `src/action/` (89 files plus `acesrc/` LTK
  bots and `botlib/`). Meson builds it as `game<cpu>` → `gamex86_64.dll`
  (`meson.build:629-639`, sources `meson.build:165-214`, with
  `ltk_src`/`botlib_src` added under `aqtion-build`,
  `meson.build:566-570`). The version string is `"2.83 git"`
  (`src/action/a_game.h:89`). The legacy `src/game/` (baseq2) is also
  present but not built into the shipped DLL.
- **Steam `action/gamex86_64.dll`** (1.1 MB, 2023-11-09, GNU ld 2.34,
  imports `msvcrt`/`WS2_32`, so it is a **MinGW** build) carries
  `TNG 2.82 2023-10-25 4729572`. That is commit `4729572` of the then-separate
  `actionquake/aq2-tng` repo, whose code has since been merged into
  `src/action` here. The root-level `<AQtion install>\gamex86_64.dll`
  (640 KB, exports only `GetGameAPI`, from the 2023-11-03 MSVC run) is
  **never loaded**, because the engine only looks in `<dir>/<game>/`.
- **Lookup order** (`src/server/game.c:1142-1160`, path built at
  `game.c:971-987` as `<dir>\<game>\game` CPUSTRING LIBSUFFIX):
  `sys_forcegamelib` (NOSET, `common.c:962`) → `<homedir>\action\` →
  `<libdir>\action\` → `<homedir>\baseaq\` → `<libdir>\baseaq\`. Note
  that it uses **libdir, not basedir**. With default `libdir="."` (the exe
  dir) a listen server would load `build\msvc\action\gamex86_64.dll`, not
  the install's. `+set libdir "<AQtion install>"` selects the Steam DLL,
  which crashed a HEAD engine until 0.0.0.20 (see 1, Spike A); dev runs
  still default to our own build (`scripts\run.ps1 -GameLib steam` for the
  Steam one): `+set sys_forcegamelib "<repo>\build\gamex86_64.dll"`
  (`scripts\run.ps1` does this by default). Accepted
  API versions are 3, 4 (AQtion), and 3302 (`game.c:1176`), and the
  optional `GetGameAPIEx` (`game.c:1182-1192`).

## 7. AQtion-specific client features

The AQtion protocol (major 38, minors 3011-3017) adds server→client
`svc_ghudupdate` (game-defined HUD elements), `svc_extend`,
`svc_userstatistic`, and `svc_cvarsync` (`inc/common/protocol.h:183-186`).
Cvarsync lets the game DLL read selected client cvars such as `cl_xerp`.
Client extras under `USE_AQTION`/`AQTION_EXTENSION` live in
`src/client/{ghud,main,screen,view,parse,entities,effects}.c`. They
include the ghud renderer, `cl_indicators`, zoom sensitivity
(`cl_zoom_2x/4x/6x`, `cl_zoom_autosens`), crosshair cvars (`xhair_*`,
`ch_hide_while_zoomed`), and the hit marker (`scr_hit_marker_time`).
Extrapolation (`cl_xerp` 0/1/2) and its server overrides `use_xerp` and
`force_cl_xerp` are game-DLL logic fed by cvarsync (`doc/action.md:925-927`,
`src/action/g_ext.c`). Lag compensation is game-side (`src/action/p_antilag.c`).
Movement hacks (strafe/water jump) are game-side TNG code
(`src/action/tng_jump.c`), and the engine pmove is skullernet's
(`src/common/pmove/`). For splitscreen, the important part is that ghud
state, cvarsync replies and userinfo (`steamid`, etc.) are per-connection
and must become per-virtual-client, alongside the usual
`client_state`/`client_static` split.

## Prerequisites for the maintainer to approve

1. `python -m pip install --user meson` into the existing CPython 3.13
   (or `winget install mesonbuild.meson`). Ninja 1.13.2 is already present.
2. Only if configure demands it: `winget install NASM.NASM`.
3. Network access on first `meson setup` to fetch wraps (github.com,
   wrapdb.mesonbuild.com, curl.se, raw.githubusercontent.com). Wraps are
   then cached in `subprojects/packagecache`, which should be gitignored.
4. Fallback only if VS 2019 fails on HEAD: `winget install
   Microsoft.VisualStudio.2022.BuildTools` with the C++ x64 workload.
5. Optional, Linux box: a Fedora toolbox with `mingw64-gcc
   mingw64-winpthreads-static nasm meson ninja-build` for Recipe B.

## Risks

- **VS 2019 is untested upstream.** CI uses VS 2022, and C11 or
  newer-SDK assumptions may break. The mitigation is VS 2022 Build Tools.
- **The `python3` Store stub** breaks `meson.build:3` unless it is pinned
  via the native file (or App execution aliases are disabled).
- **Wrap URLs can rot.** `jansson.wrap` pulls its patch from the upstream
  `aqtion` branch on raw.githubusercontent; the in-tree copy is
  `extern/jansson_patch.zip`. Keep `packagecache` and record provenance.
- **CWD-relative writes:** the bot nav files and crash reports land in
  the exe dir, so run the exe from `build\` and never place it in the
  install.
- **Game DLL selection** depends on `libdir`, not `basedir`. Forgetting
  `+set libdir` or `+set sys_forcegamelib` gives "Failed to load game
  library" or a mismatched DLL.
- **The Steam DLL (TNG 2.82) is older than `src/action` (2.83 git)** and
  the Steam engine (minor 3014) is older than HEAD (3017). This is fine
  for online play thanks to negotiation, but test listen-server and bots
  with our own DLL.
- **Upstream churn:** `aqtion-alpha` is 199 commits ahead and is merged
  into `aqtion` in large squash-style PRs, so expect conflicting merges in
  `src/client/main.c`/`parse.c`. Keep splitscreen in new files with thin
  hooks.
- **Per-client AQtion state** (ghud, cvarsync, userinfo) must be
  duplicated per virtual client, or the mod/server will see crossed
  cvars/HUDs between local players.
- There is no existing gamepad layer and no SDL on Windows. Input is
  entirely new work (XInput supports 4 pads; SDL GameController would be
  needed beyond 4).
