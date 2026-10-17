# Splitscreen design — AQtion client (Q2PRO), multi-client-instance

Status: design draft, 2026-10-08, against upstream `39f038ff`
(actionquake/q2pro). Line numbers are for that commit and will drift;
function names are the stable anchor. Nothing under `src/` was changed to
write this. Build setup and prerequisites are in
`docs/BUILD-AND-PROVENANCE.md` (Spike A).

Template: the finished Quake3e-splitscreen design (cited below as
**"Q3 §n"**, `docs/SPLITSCREEN-DESIGN.md` in that repo). Same
architecture: every local player is a complete virtual client inside one
process — its own `cl`, its own connection slice of `cls`, its own netchan
on its own UDP source port + qport, its own viewport. The server and the
Action game DLL see N ordinary players, so unmodified servers work.
Renderer, sound device, filesystem, console, engine UI, the keyboard's
bindings, HTTP downloads and the console command buffer stay single and
shared.

**What is easier than Q3:** Quake 2 has no client-side game module. HUD,
scoreboard, inventory and Action's team/weapon/item menus are drawn by
the engine from server-sent data that is already per connection
(`cl.configstrings[CS_STATUSBAR]`, `cl.layout`, `cl.inventory`,
`cl.frame.ps.stats`, AQtion's `cl.ghud`). There is no VM to instance, no
"told glconfig" to lie about, no cgame restart on layout change, and
Action's in-game menus (server `PMenu` layouts, `src/action/a_menu.c`)
are per player by construction.

**What is harder than Q3:** Q2PRO keeps more per-player state in file
statics (effects, temp entities, scene arrays, centerprint/chat/lag,
crosshair animation, the stufftext buffer), the 2D renderer has no notion
of a viewport origin, and `Com_Error` tears down server *and* client
together.

Terms as in Q3: **context** = one virtual client (`clx[i]`, i = 0..N-1;
context 0 = primary = P1 = host). **Active context** = the one `cl` and
the connection fields of `cls` currently resolve to. `MAX_SPLITVIEW` = 8
from day one.

---

## 1. Client context model

### 1.1 `client_state_t cl` (`src/client/client.h:163-331`, defined `main.c:122`)

`cl` is "wiped completely at every server map change" (comment
`client.h:158-160`; `memset` in `CL_ClearState`, `main.c:705`). By
definition every field is per connection, so **all of `cl` is per
player**. The third column says which fields hold content that is
*identical* across contexts on the same server — useful to skip work for
extras.

| Fields (`client.h` line) | Per player | Content identical across contexts? |
|---|---|---|
| `timeoutcount`, `lastTransmit*`, `sendPacketNow` (164-169) | yes | no |
| `cmd`, `cmds[CMD_BACKUP]`, `cmdNumber`, `predicted_origins`, `history`, `initialSeq` (171-176) | yes | no — usercmds |
| `predicted_*`, `prediction_error` (178-187) | yes | no |
| `solidEntities`, `numSolidEntities` (190-191) | yes | no (PVS-dependent) |
| `baselines[MAX_EDICTS]` (193) | yes | yes (server baselines) |
| `entityStates[MAX_PARSE_ENTITIES]`, `numEntityStates`, `esFlags/psFlags` (195-199) | yes | no |
| `frames[UPDATE_BACKUP]`, `frame`, `oldframe`, `servertime/delta`, `keyframe*` (201-214) | yes | no |
| `dcs[]` (216) | yes | yes |
| `viewangles`, `localmove`, `mousemove` (223-231) | yes | no — **input target** |
| `time`, `lerpfrac`, `keytime`, `keylerpfrac` (233-240) | yes | no |
| `refdef`, `fov_x/y`, `lightlevel`, `v_forward/right/up`, `thirdPersonView`, `playerEntity*` (242-254) | yes | no — **view** |
| `layout[MAX_NET_STRING]`, `inventory[MAX_ITEMS]` (258-259) | yes | no — **HUD and menus** |
| AQtion `ghud_3dlist`, `ghud[MAX_GHUDS]` (262-263) | yes | no |
| `serverstate`, `servercount`, `gamedir`, `clientNum`, `maxclients`, `max_stats`, `pmp`, `frametime*`, `view_*` (268-284) | yes | all identical except `clientNum` |
| `baseconfigstrings`, `configstrings` (286-287), `csr`, `mapname` | yes | yes (server-global) |
| `reply_time/delta` (autoreply, 292-295) | yes | n/a — run for P1 only |
| `bsp` (300) | yes | yes — `BSP_Load` is ref-counted, same pointer |
| `model_draw/model_clip[MAX_MODELS]`, `sound_precache`, `image_precache` (302-306) | yes | yes — renderer/sound handles are name-keyed |
| `clientinfo[MAX_CLIENTS]`, `baseclientinfo`, `weaponModels`, `numWeaponModels` (308-312) | yes | yes |
| `cvarsync[CVARSYNC_MAX]` (314), `need_powerscreen_scale` | yes | no — AQtion per-client cvar sync (§8) |
| `weapon.muzzle`, `hit_marker_time/count`, `custom_fog` (318-330) | yes | no |

Consequence: extras can **copy** `model_draw`, `model_clip`,
`image_precache`, `sound_precache`, `clientinfo`, `baseclientinfo`,
`weaponModels` from context 0 instead of re-registering (§7.3), and must
never call `R_BeginRegistration`.

### 1.2 `client_static_t cls` (`client.h:400-519`, defined `main.c:121`)

| Field (line) | Class | Notes |
|---|---|---|
| `state` (401) | **per connection** | 133 uses; also read by shared code (see 1.4) |
| `key_dest` (402) | shared | keyboard/console/menu focus; pad players bypass it (§5) |
| `active` (404), `ref_initialized` (406), `disable_screen` (407) | shared | window focus, renderer up, loading plaque |
| `userinfo_modified`, `userinfo_updates[]` (409-410) | **per connection** | pending userinfo deltas |
| `realtime`, `frametime`, `measure` (414-431) | shared | `measure.ping` is P1's |
| `serverAddress`, `servername`, `connect_time`, `connect_count`, `passive` (434-438) | **per connection** | |
| `z` (441, zlib inflate for compressed packets) | **per connection** | cannot be swapped by `memcpy` (zlib checks `state->strm == strm`); see 1.4 |
| `quakePort`, `netchan`, `serverProtocol`, `protocolVersion`, `challenge`, `errorReceived` (444-454) | **per connection** | `netchan_t` buffers are heap pointers (`inc/common/net/chan.h:30-67`), so the struct is safe to copy |
| `recent_addr[]`, `recent_head` (459-460) | shared | connect history for completion |
| `stufftextwhitelist` (462) | shared | |
| `download` (464-476) | **P1 only** | extras never download (P1 already has every file) |
| `demo` (479-504) | **P1 only** | record/playback context 0 only |
| `gtv` (506-518, `USE_CLIENT_GTV`) | **P1 only** | client GTV relay |

Reference counts in `src/client` (`grep -ow`): `cls.demo` 215,
`cls.state` 133, `cls.gtv` 91, `cls.download` 80, `cls.netchan` 76,
`cls.serverProtocol` 69, `cls.realtime` 52, `cls.key_dest` 47,
`cls.serverAddress` 25, `cls.protocolVersion` 20; `cl.` ≈ 1180.

Also outside `cls`: `cl_cmdbuf` / `cl_cmdbuf_text` (`main.c:127-128`;
exec callback `exec_server_string` `main.c:2563`, wired `main.c:3519-3521`;
filled by `CL_ParseStuffText` `parse.c:1224-1230`). The **stufftext buffer
is per connection**: `changing`, `reconnect`, `precache N`, `cmd …` and
the server's probes (`%cpsi`, `%!fc`, `src/action/g_cmds.c:1984-1985`)
must execute as the player whose server stream sent them.

### 1.3 File statics in `src/client` (and the few outside it)

"Per player" = must live in the context. "Shared" = stays a global.

| File:line | Static | Class | Notes |
|---|---|---|---|
| `main.c:124` | `cl_entities[MAX_EDICTS]` | **per player** | 8192 × `centity_t`; `memset` by `CL_ClearState` (`main.c:706`) |
| `main.c:127-128` | `cl_cmdbuf`, `cl_cmdbuf_text` | **per player** | see 1.2 |
| `main.c:152-153` | `clientRequests`, `nextRequest` | shared | server browser / status pings |
| `main.c:1706` | `precache_spawncount` | **per player** | set from that connection's `precache N` |
| `main.c:1863-1864` | `cl_ignore_text/nick` | shared | |
| `main.c:3186-3188` | `ref/phys/main_msec/extra`, `sync_mode` | shared | one frame clock |
| `input.c:70` | `input` (mouse filter `old_dx/dy`) | KBM owner | only the keyboard/mouse player reads the mouse |
| `input.c:240-247` | kbuttons `in_klook … in_down`, `in_impulse`, `in_mlooking` | **per player** | as Q3 `cl_input.c:57-61` |
| `input.c:440-441` | `autosens_x/y` | **per player** | zoom auto-sensitivity follows that player's fov |
| `keys.c:21-41` | `keybindings[256]`, `keydown`, `buttondown`, `consolekeys`, `keyshift`, `anykeydown`, `key_wait_cb` | shared | the physical keyboard; pads get their own tables (§5) |
| `predict.c:131` | `pm_clipmask` | shared | set per call |
| `effects.c:25` | `avelocities[]` | shared | random table |
| `effects.c:40` | `cl_lightstyles[MAX_LIGHTSTYLES]` | per player | from configstrings; cheap; simplest per context |
| `effects.c:89` | `cl_dlights[MAX_DLIGHTS]` | **per player** | muzzle flashes/explosions seen by that client |
| `effects.c:823-825` | `active_particles`, `free_particles`, `particles[MAX_PARTICLES]` | **per player** | a shared pool would spawn every event N times |
| `tent.c:23-58` | `cl_sfx_*`, `cl_mod_*`, `cl_img_flare` | shared | registered handles |
| `tent.c:70-72` | `cl_footstep_sfx`, `cl_num_footsteps`, `cl_last_footstep` | shared | registered list; `cl_last_footstep` is a "don't repeat" memory, harmless shared |
| `tent.c:408,666,755-756,1081` | `cl_explosions`, `cl_lasers`, `cl_beams`, `cl_playerbeams`, `cl_sustains` | **per player** | |
| `tent.c:1172-1173` | `railcore/railspiral_color` | shared | cvar-derived |
| `parse.c:848-850` | `te`, `mz`, `snd` | shared | transient parse scratch, consumed within one packet |
| `view.c:26-27` | `gun_frame`, `gun_model` | shared | debug tools |
| `view.c:55-63` | `r_numdlights/r_dlights`, `r_numentities/r_entities`, `r_numparticles/r_particles`, `r_lightstyles` | **per player** | `V_RenderView` reuses the previous refdef on an invalid frame (`view.c:391-393`); its `entities` pointer must still be *this* player's list |
| `screen.c:35-65` | `scr` | split | assets (`crosshair_pic`, `sb_pics`, `font_pic`, …) shared; `hud_x/y/width/height/scale` per cell; `currently_scoped_in` per player |
| `screen.c:134` | `scr_vrect` | per cell | |
| `screen.c:374` | `graph` (debug graph) | shared (P1) | |
| `screen.c:528-529` | `scr_centerprints`, `scr_centerhead/tail` | **per player** | each server stream sends its own centerprints |
| `screen.c:651-669` | `DRAW_LAG_*`, `lag` | per player | `SCR_LagSample` (`screen.c:676`) reads `cls.netchan` + `cl.history` |
| `screen.c:787` | `scr_objects` | shared | user `draw` list |
| `screen.c:1005-1006` | `scr_chatlines`, `scr_chathead` | **per player** | chat HUD inside each cell |
| `screen.c:2294-2296`, `2377-2396` | `deltatime_factor`, `last_rtime`, `gap`, `length`, `xh_elasticity` | **per player** | AQtion dynamic crosshair spread animation state (§3.4 item 8) |
| `screen.c:2358` | `spray` (local static in `XHAIR_ApplyFiringInaccuracy`) | **per player** | found in M1-T1; same crosshair animation |
| `screen.c:2300` | `xhair_weapon_cfgs` | shared | constant weapon table (never written) |
| `screen.c:27` | `r_viewmatrix` | shared | found in M1-T1; scratch, written and read inside one 3D-ghud projection call |
| `screen.c:654-655` | `DRAW_LAG_WIDTH/HEIGHT` | shared | found in M1-T1; derived from `scr_lag_draw_scale` |
| `entities.c:27` | `cl_laststep` | shared | found in M1-T1; unused (dead global) |
| `download.c:37` | `precache_sexed_total` | P1 only | found in M1-T1; part of the download state machine (row above) |
| `keys.c:22,41` | `key_wait_arg`, `key_overstrike` | shared | found in M1-T1; keyboard/console |
| `ffcin.c:76-77`, `gtv.c:27-30`, `http.c:63-78` | codec list, GTV buffers, curl/download state | shared / P1 only | found in M1-T1 |
| function-local | `warned` (`main.c`), `isdown` (`CL_Shutdown`), `recursive` (`SCR_UpdateScreen`), `tinystr` (`keys.c`), `buffer` (`http.c`) | shared | found in M1-T1; once-per-process flags and scratch |
| `console.c:84` | `con` | shared | one console (P1/host) |
| `cin.c:57` | `cin` | P1 only | cinematics are single-player only |
| `demo.c:25` | `demo_buffer` | P1 only | |
| `download.c:35-40` | `precache_check`, `precache_sexed_sounds`, `map_crc_retry_attempted` | P1 only | extras skip the download state machine (§7.3) |
| `refresh.c:38` | `mode_changed` | shared | |
| `locs.c:31` | `cl_locations` | shared | same map for all; `CL_ClearState` → `LOC_FreeLocations` (`main.c:700`) must be P1-only |
| `ui/ui.h:276-309` | `uis` | shared (host) | engine menus (§7) |
| `sound/main.c:35-39` | `listener_origin/forward/right/up/entnum` | → per-player array | §6 |
| `src/refresh/main.c` | `glr` (per scene), `draw_strings` (debug) | shared | `glr.fd` is replaced per `R_RenderFrame` |

**As built (M1-T1, 0.0.0.4).** Every "per player" row above lives in
`client_context_t` (`src/client/client.h`, end of file): `cl`, `entities`,
the `cls` connection slice (`conn`), `z`, `cmdbuf`/`cmdbuf_text`,
`precache_spawncount`, and the sub-structs `fx` (`effects.c` lightstyles,
dlights, particle pool; `tent.c` explosions, lasers, beams, player beams,
sustains), `scene` (`view.c` `r_*` arrays; `effects.c` reads
`r_particles` through the same macros), `scr` (centerprints, chat HUD,
lag samples, `scr.currently_scoped_in` moved out of `scr`, the crosshair
animation incl. `spray`) and `in` (kbuttons, `in_impulse`, `in_mlooking`,
`autosens_x/y`). `scr_vrect` and `scr_cell` stay globals (recomputed per
cell before drawing). The private types those arrays need
(`clightstyle_t`, `explosion_t`, `laser_t`, `beam_t`, `centerprint_t`,
`chatline_t`, `kbutton_t`, the lag struct as `scr_lag_t`, and their
`MAX_*` sizes) moved from the `.c` files into `client.h`.

### 1.4 Mechanism: context array + active-pointer macros

Same as Q3 §1.2, adapted. In `src/client/client.h` — private to the
client: the server includes `inc/client/client.h`, the renderer includes
neither, and there is no local variable or struct member named `cl` in
`src/client` — replace `extern client_state_t cl;` and
`extern centity_t cl_entities[]` with:

```c
#define MAX_SPLITVIEW 8

typedef struct {                    // per-connection slice of cls
    connstate_t state;
    int         userinfo_modified;
    cvar_t      *userinfo_updates[MAX_PACKET_USERINFOS];
    netadr_t    serverAddress;
    char        servername[MAX_OSPATH];
    unsigned    connect_time;
    int         connect_count;
    bool        passive;
    int         quakePort;
    netchan_t   netchan;
    int         serverProtocol, protocolVersion, challenge;
#if USE_ICMP
    bool        errorReceived;
#endif
} client_conn_t;

typedef struct client_context_s {
    client_state_t  cl;                     // ~4.7 MB
    centity_t       entities[MAX_EDICTS];   // ~2.3 MB
    client_conn_t   conn;                   // saved slice while not active
    z_stream        z;                      // never memcpy'd (zlib self-pointer)
    cmdbuf_t        cmdbuf;
    char            cmdbuf_text[MAX_STRING_CHARS];
    cl_fx_t         fx;                     // effects.c + tent.c statics (1.3)
    cl_scene_t      scene;                  // view.c r_* arrays
    cl_scr_t        scr;                    // centerprint, chat HUD, lag, xhair anim, vrect
    cl_input_t      in;                     // kbuttons, impulse, mlook, autosens (§5)
    int             precache_spawncount;
    vrect_t         cell;                   // real framebuffer pixels (§3)
    int             playerNum;              // 0-based slot, never renumbered
    netsrc_t        sock;                   // NS_CLIENT for P1, NS_CLIENT2.. for extras (§4)
    bool            inUse;
} client_context_t;

extern client_context_t *clx[MAX_SPLITVIEW];
extern client_context_t *cla;               // active, never NULL
#define cl          (cla->cl)
#define cl_entities (cla->entities)
```

- The ≈ 1180 `cl.` uses and every `cl_entities[...]` /
  `memset(&cl_entities, 0, sizeof(cl_entities))` compile unchanged (the
  member is an array, so `sizeof` stays right).
- **cls slice.** Move the per-connection fields of `client_static_t` into
  `client_conn_t` and include it *anonymously* —
  `union { client_conn_t; client_conn_t conn; };` — the Microsoft-
  extension trick the same header already uses for `centity_state_t`
  (`client.h:60-71`; `-fms-extensions` in `meson.build:321`, native on
  MSVC). Every existing `cls.state`, `cls.netchan`, `cls.serverProtocol`
  … (≈ 400 uses) compiles unchanged, and `CL_SetContext` swaps the slice
  with one `memcpy` (≈ 1.5 KB) — no field can be forgotten. `cls.download`,
  `cls.demo`, `cls.gtv` stay outside the slice (P1 only); every path that
  touches them is guarded `if (cla->playerNum == 0)` or is unreachable for
  extras (demo playback disables splitscreen; downloads are skipped,
  §7.3).
- **zlib:** `cls.z` (8 uses in `parse.c`/`main.c`) becomes
  `#define CL_Z (cla->z)`; never part of the swapped slice.
- **File statics** (1.3): file-local macros at the top of each file
  (`#define particles (cla->fx.particles)`, `#define in_forward
  (cla->in.forward)`, …) so function bodies stay untouched — the Q3
  `cl_input.c` technique.
- **Switching:** `CL_SetContext(i)` = save the `cls` slice into
  `cla->conn`, `cla = clx[i]`, load its slice into `cls`.
  `CL_PushContext(i)` / `CL_PopContext()` with a depth-2 stack (screen
  drawing can be entered from per-context parsing, §2). Outside an
  explicit loop the active context is **context 0**, so shared code that
  reads `cls.state` (`SCR_DrawActive` `screen.c:2837`,
  `IN_GetCurrentGrab` `input.c:75-107`, `CL_UpdateFrameTimes`
  `main.c:3248`, the UI) sees P1 — the desired behaviour, as Q3 §1.3.
- Context 0 is a static object, so N=1 behaves exactly as upstream; extras
  are `Z_Mallocz`'d on join and freed on leave.
- Rejected (as in Q3): threading a context pointer through every function
  (huge, unmergeable diff); splitting `cls` into a new named struct
  (≈ 400 renamed use sites for nothing the anonymous member does not
  already give).

### 1.5 Memory per context

With `USE_PROTOCOL_EXTENSIONS` (always on, `meson.build:691`):
`MAX_EDICTS` 8192, `MAX_PARSE_ENTITIES` 512 × 16 = 8192 (`protocol.h:129-130`),
`MAX_CONFIGSTRINGS` ≈ 13 630 × 64 B (`inc/shared/shared.h:1641-1651`),
`centity_state_t` ≈ 104 B.

| Part | Size |
|---|---|
| `cl`: baselines + entityStates (2 × 8192 × 104 B) | 1.7 MB |
| `cl`: configstrings + baseconfigstrings | 1.75 MB |
| `cl`: clientinfo[256], model/sound/image handles, frames, cmds, layout, ghud, rest | ≈ 1.2 MB |
| `cl_entities` (8192 × ≈ 280 B) | 2.3 MB |
| effects (8192 particles × 64 B, dlights, explosions, lasers, beams, sustains) | ≈ 0.6 MB |
| scene arrays (`r_entities[2048]`, `r_particles[8192]`, dlights) | ≈ 0.5 MB |
| netchan buffers (heap, `Netchan_Setup`), cmdbuf, screen state | ≈ 0.1 MB |
| **Total** | **≈ 8 MB per context** → 8 players ≈ 64 MB; no hunk/zone limit concern (Q3's UrT case needed `com_hunkMegs`; Q2PRO has no fixed hunk for this) |

**Measured (M1-T1, MSVC x64, logged at `CL_Init` as `client context: …`):**

| Part | Bytes |
|---|---|
| `client_context_t` total | 7 226 880 (6.9 MiB) |
| `cl` (`client_state_t`) | 3 940 896 |
| `entities` (8192 × 264 B) | 2 162 688 |
| `fx` (particles, dlights, lightstyles, explosions, lasers, beams, sustains) | 715 024 |
| `scene` (`r_entities`, `r_particles`, `r_dlights`, `r_lightstyles`) | 396 816 |
| `scr` (centerprints, chat, lag, crosshair) | 9 324 |
| `in` (kbuttons, impulse, mlook, autosens) | 316 |
| `conn` (the `cls` slice swapped by `CL_SetContext`) | 624 |

8 contexts ≈ 58 MB. Context 0 is zero-initialised BSS (the exe grew by
1 KB); its non-zero start values (crosshair animation, `inUse`) are set by
`CL_InitContexts` at the top of `CL_Init`, and `cla` is a static initialiser,
so code running before `CL_Init` already sees context 0.

---

## 2. Frame flow

`Com_Frame` (`src/common/common.c:1094-1152`): `NET_Sleep` → spin
`CL_ProcessEvents` until ≥ 1 ms has passed (`common.c:1105-1113`) →
`SV_Frame` → `CL_Frame`. `CL_Frame` is `main.c:3289-3439`,
`CL_ProcessEvents` `main.c:3446-3470`.

| Step (`main.c` line) | Once per frame | Per context | Notes |
|---|---|---|---|
| `cls.realtime += msec`, `main_extra` (3299-3300) | ✔ | | |
| `CL_RunRefresh`, `IN_Frame` (3452, 3454) | ✔ | | + pad poll hook (§5) |
| `NET_GetPackets(NS_CLIENT, CL_PacketEvent)` (3456) | | ✔ | one call per context socket inside `CL_PushContext(i)` (§4) |
| `Cbuf_Execute(&cmd_buffer)` (3459) | ✔ | | console/binds run as context 0 unless prefixed `p<N>` (§5) |
| `Cbuf_Execute(&cl_cmdbuf)` (3460) | | ✔ | stufftext per connection |
| `HTTP_RunDownloads`, `CL_GTV_Run`, `CL_RunDiscord` (3462-3468) | ✔ (P1) | | |
| sync-mode decision, `cls.frametime` (3304-3352) | ✔ | | the `cl.sendPacketNow` test (3338) and `CL_ProcessEvents`' return value become "any context" |
| `cl.time += main_extra` (3357-3361) | | ✔ | |
| `CL_DemoFrame` (3365-3366) | ✔ (P1) | | splitscreen off during demo playback |
| `CL_SetClientTime` (3369-3370) | | ✔ | |
| `CL_CheckForReply` (autoreply, 3374) | ✔ (P1) | | avoid N `!version` replies |
| `CL_CheckForResend` (3378) | | ✔ | extras: serialized join (§4) |
| `CL_UpdateCmd(main_extra)` (3381, `input.c:600`) | | ✔ | input source per context (§5) |
| `CL_FinalizeCmd` (3384-3395, `input.c:738`) | split | ✔ | its first lines `Cbuf_Frame(&cmd_buffer)` / `Cbuf_Frame(&cl_cmdbuf)` (`input.c:743-744`) move out: `cmd_buffer` once, each context's stufftext buffer per context |
| `CL_SendCmd` (3398) | | ✔ | own netchan → own socket |
| `CL_PredictMovement` (3401) | | ✔ | that context's `cl.bsp`, `cl_entities` |
| `Con_RunConsole`, `SCR_RunCinematic`, `UI_Frame` (3403-3407) | ✔ | | |
| `SCR_UpdateScreen` (3414) | ✔ | loops cells inside (§3.3) | |
| `S_Update` (3423) | ✔ | listeners from all contexts (§6) | the SLEEP_10 branch's `CL_CalcViewValues` (3426) per context |
| `CL_CheckTimeout` (3431) | | ✔ | for i>0 an error becomes a per-player drop (§7.4) |
| `CL_MeasureStats` (3435) | ✔ (P1) | | |

Implementation shape: one helper `CL_ForEachContext(fn)` in the new split
code; `CL_Frame` calls it around the per-context run of the existing
static functions. With one context the loop is a single iteration with
context 0 already active (no swap), so N=1 keeps upstream's order exactly.

**Re-entrancy.** `SCR_UpdateScreen` is called from inside per-context
code: `CL_PrepRefresh` (`precache.c:438`), `CL_Changing_f`
(`main.c:1049`), `CL_Reconnect_f` (`main.c:1101`), `CL_Skins_f`
(`main.c:1194`), anticheat load (`main.c:1424`). It already
refuses depth > 1 (`screen.c:2895-2897`). Rule: the screen loop pushes
and pops the context it draws and restores the caller's context on exit;
it never runs network or command code, so (unlike Q3's VM `callLevel`
problem) re-entry is safe.

*As built (M1-T2+T3, 0.0.0.5):* player 1's steps stay inline in `CL_Frame`
/ `CL_ProcessEvents` exactly as upstream. After player 1's
`Cbuf_Execute(&cl_cmdbuf)`, `CL_SplitProcessEvents` runs each extra's
`CL_ContextEvents` (own socket + own stufftext buffer); after player 1's
`CL_PredictMovement`, `CL_SplitFrame` runs each extra's `CL_ContextFrame`
(`cl.time`, `CL_SetClientTime`, `CL_CheckForResend`, `CL_UpdateCmd`,
`CL_FinalizeCmd` on the shared physics tick, `CL_SendCmd`,
`CL_PredictMovement`, `CL_CheckTimeout`) — one block per extra under its
error guard rather than step-interleaved. `cl.sendPacketNow` tests and the
`CL_ProcessEvents` return are `CL_SplitSendPacketNow()` (any context);
`Cbuf_Frame(&cmd_buffer)` runs for player 1 only. The mouse and
`BUTTON_ANY` are player 1's.

---

## 3. Viewport virtualization

### 3.1 What exists

- **3D** honours a rect: `V_RenderView` copies `scr_vrect` into
  `cl.refdef.x/y/width/height` (`view.c:422-425`); `GL_Setup3D` sets
  `glViewport(fd.x, r_config.height - (fd.y + fd.height), …)`
  (`src/refresh/state.c:300-306`) — absolute framebuffer pixels. FOV
  follows the rect aspect (`cl_adjustfov`, `view.c:428-434`), so a wide or
  tall cell gets the right projection for free.
- **2D does not.** `GL_Setup2D` resets the viewport to the full window and
  `GL_Ortho(0, r_config.width, r_config.height, 0)` (`state.c:204-223`);
  `R_SetScale` rebuilds the same full-window ortho (`src/refresh/draw.c:267-277`);
  `R_SetClipRect` clamps to and flips by `r_config` (`draw.c:190-230`).
  There is no 2D origin anywhere.
- HUD code is written against `scr.hud_width/hud_height` and starts its
  cursor at `scr.hud_x/hud_y` (layout interpreter from `screen.c:1868`:
  `xr` = `hud_width + n`, `xv` = `hud_width/2 - 160 + n`, `yb`,
  `yv` = `hud_height/2 - 120 + n`); `SCR_DrawActive` sets the HUD to the
  full window (`screen.c:2848-2849`), `SCR_Draw2D` scales it by
  `scr.hud_scale` (`screen.c:2776-2780`).
- Reads of the real window size that must read the cell instead:
  `SCR_DrawXhair` (`screen.c:2385-2386`), the black fill when not active
  (`screen.c:2838`), HUD init (`screen.c:2848-2849`). Full-screen by
  design and left alone: loading pic (`screen.c:2268-2269`, P1), console
  (`console.c:372-373`), cinematics (`cin.c:337-347`, `ffcin.c:423-433`),
  UI (`ui/ui.c:102-103,397`) except when drawn into P1's cell (§7.1).
- **Renderer facts that matter:**
  - `R_BeginFrame` clears the whole colour buffer once (`src/refresh/main.c:1906-1910`)
    — exactly the "clear once" we need.
  - `GL_Setup3D` clears depth+stencil for the whole framebuffer without a
    scissor (`state.c:318`). Harmless: each cell's 3D is finished and its
    2D drawn (no depth test) before the next cell's 3D clears depth.
  - `GL_Blend` (screen tint, damage vignette) draws at `glr.fd.x/y` in 2D
    coordinates (`draw.c:140-170`), and so do bloom/waterwarp
    post-process (`main.c:1775,1884`). These must become cell-relative
    once 2D has an origin.
  - The bloom/waterwarp FBO is sized to the refdef and **re-created
    whenever the refdef size changes** (`main.c:1817-1823`). Cells of
    different sizes in one frame (fill mode, N = 3/5/7) would re-create it
    every scene. Fix: size the FBO to the largest cell and render into a
    sub-rect, or skip post-process while cells differ in size. `gl_bloom`
    and `gl_waterwarp` default to 0 (`main.c:2071-2073`).
  - `R_RenderFrame` already runs more than once per frame (the UI
    player-model preview, `ui/playerconfig.c:131`); `glr.fd` is replaced
    per call (`main.c:1786`); the PVS cache `glr.viewcluster*` recomputes
    when the leaf changes (alternating views recompute every scene —
    correct, small cost). `MAX_ENTITIES` 2048 / `MAX_PARTICLES` 8192 /
    `MAX_DLIGHTS` 64 (`inc/refresh/refresh.h:24-26`) are **per scene**
    (the arrays come with the refdef), so N views do not share a cap —
    unlike Q3 §3.3, no renderer cap changes.
  - Flare occlusion queries (`GL_OccludeFlares`) are per entity across
    scenes: flares may flicker with N views — check in Spike C.
  - `R_BeginRegistration` does `memset(&glr, 0, …)` and
    `R_EndRegistration` frees everything not registered since
    (`main.c:2442-2470`): extras must never run registration (§7.3).
  - One renderer only (`src/refresh`, GL + GLES variant); nothing to keep
    in sync across renderers (Q3 had Vulkan + GL).

### 3.2 Design: a 2D draw rect in the renderer, the cell as the HUD's screen

One renderer addition, `void R_SetDrawRect(const vrect_t *rc)` (NULL =
whole window), in `inc/refresh/refresh.h` + `src/refresh/draw.c` /
`state.c` (≈ 40 lines):

- stores `draw.rect`; `GL_Setup2D` and `R_SetScale` set
  `glViewport(rect.x, H - rect.y - rect.height, rect.width, rect.height)`
  and `GL_Ortho(0, rect.width * scale, rect.height * scale, 0)`;
- `R_SetClipRect` offsets the clip by `rect.x/y` and clamps to the rect;
  with a rect set the scissor is always on and at most the rect, so wide
  layouts, centerprints and chat cannot bleed into a neighbour;
- `GL_Blend` / post-process use `glr.fd.x - rect.x`, `glr.fd.y - rect.y`;
- `R_RenderFrame` ends in `GL_Setup2D` (`main.c:1878`), which now restores
  the current rect, not the full window.

Client side, per cell: `scr.hud_width/height` = cell size,
`scr.hud_x/y` = 0, `R_SetDrawRect(&cell)`; `V_RenderView` adds the cell
origin to the refdef (`refdef.x = cell.x + scr_vrect.x`); the two
`r_config` reads in the crosshair (`screen.c:2385-2386`) become
`scr.hud_width/height`. Every existing HUD/layout/ghud/centerprint/chat
draw call then lands in the cell unchanged. `scr_viewsize` < 100 keeps
working inside a cell (`SCR_CalcVrect` `screen.c:1193`, `SCR_TileClear`
`screen.c:1634` are relative).

Rejected: adding the cell origin at every 2D call site in the client
(hundreds of draw calls, layout interpreter, UI) — the renderer rect is
one place.

**HUD scale per cell.** Q2 layout programs assume ≥ 320×240 virtual
pixels (`xv`/`yv` = centre ∓ 160/120; AQ2 scoreboards are wider). Per
cell: `hud_scale = min(global scr_scale, largest integer k with
cell.h / k ≥ 240 and cell.w / k ≥ 320)`. At 1280×720: 2 players
(1280×360) → ×1; 4 players (640×360) → ×1; 8 players fill 4×2
(320×360) → ×1 and wide layouts clip at the cell edge (scissor) —
measure Action's teamplay scoreboard width in Spike C; fallback is
`cl_splitHudScale` or a fractional 0.5 scale (`R_SetScale` takes floats;
fonts get soft).

**Layouts:** reuse Q3 §3.2 as is — the N → rect table (1 / 2 top-bottom
or `cl_splitVertical` / 3 fill or 2×2 with a black cell / 4 2×2 / 5–6 /
7–8), `cl_splitFill`, `cl_splitWidePlayer`, wide row = bottom row, unused
cells black. Not needed here: Q3's "told size" / `cl_splitAspect`
machinery (Q3 §3.2 R9/R10 notes). Q2 has no VM caching a screen size;
every frame recomputes from the cell, so join, leave, fill changes and
window resizes **never restart anything**.

### 3.3 Per-cell sequencing (`SCR_UpdateScreen`, `screen.c:2875`)

```
R_BeginFrame()                                  // clears the whole window once
if (split active && P1 state >= ca_active && P1 not in a cinematic):
    for each context i with a cell (slot order):
        CL_PushContext(i)
        R_SetDrawRect(&cla->cell)
        SCR_DrawActive()      // hud = cell; tile clear; V_RenderView (refdef += cell origin);
                              // SCR_Draw2D: crosshair, stats, layout, ghud, inventory,
                              // centerprint, net, chat HUD, pause — all in-cell
        split overlays for i  // "P3 connecting…", drop reason, per-player menu (§7.1)
        CL_PopContext()
    R_SetDrawRect(NULL)
    split: 1-px cell borders, join hint (§7.2)
else:
    SCR_DrawActive()          // upstream path, N = 1
UI_Draw(cls.realtime)         // full screen at the main menu; inside P1's cell in game (§7.1)
Con_DrawConsole()             // shared, full screen (notify lines: P1's cell, below)
SCR_DrawLoading()
R_EndFrame()
```

`SCR_DrawActive`'s early exit "full-screen menu is up → draw nothing"
(`screen.c:2833`, `UI_IsTransparent`) must not blank the other players:
with N>1 the in-game menu is drawn transparent inside P1's cell (§7.1).
A context still connecting draws only its overlay text in its cell (no
loading plaque; `cls.disable_screen` stays P1's).

*As built (M1-T4, 0.0.0.6):* `SCR_DrawCells` (`screen.c`) runs when more
than one context is in use and player 1 is not in a cinematic; layouts live
in `split/split_view.c` (`CL_SplitLayout`: 1 full, 2 top/bottom or
`cl_splitVertical 1` left/right, 3-4 a 2×2 grid with unused cells black,
5-6 3×2 and 7-8 4×2 as placeholders until M4; all cells of a layout are
equal; assigned to contexts in use in slot order; recomputed when N, the
window size or `cl_splitVertical` changes, checked once per drawn frame and
forced on join/leave). Differences from the sketch above: no cell borders
or join hint yet (M2); the pushes go through `CL_SplitPushView`, which skips
a cell instead of asserting if the context stack is full
(`MAX_CONTEXT_STACK` is now 3: frame guard + one nested `p<N>` + the screen
loop; a second nested `p<N>` is refused); an extra is drawn only while
player 1 is `ca_active` on the same `servercount` (its handles are copies of
player 1's), otherwise its cell is black with "Player N: waiting for player
1"; a connecting extra shows "Player N: connecting..." / "loading...". A
full-screen engine menu still blanks every cell (§7.1, M2). Debug graphs
(`scr_timegraph`, `scr_debuggraph`, `scr_netgraph`) draw in player 1's cell
only. Registration copies are refreshed by `CL_SplitRegistrationChanged`
after `R_EndRegistration` in `CL_PrepRefresh` (vid_restart, `gl_shaders`
toggles) and after `CL_RegisterSounds`, for every other context on the same
`servercount` — before `CL_PrepRefresh`'s own `SCR_UpdateScreen`.
*Per-cell HUD scale as built:* with `scr_scale 0` (auto) each cell uses the
renderer's auto rule (`get_auto_scale`: short side ≥ 1080 → 2, ≥ 2160 → 4)
applied to the cell size, capped by the window's auto scale; the DPI floor
does not apply per cell. This differs from §3.2's "largest k with cell ≥
320×240" (which would give 2 for a 960×540 cell). On the test box the window
auto scale is already 2 at 1280×720 (Windows DPI floor), so 2- and 4-player
cells at 720p and 4-player cells at 1080p all draw at scale 1. An explicit
`scr_scale` applies unchanged to every cell. Flares: none of the 44 maps in
the install (loose and packed) contains `misc_flare`, so the per-scene
flare-occlusion flicker could not be exercised.

### 3.4 As measured (Spike C)

Spike C (0.0.0.3) implemented §3.2's renderer half and drew the **same**
`cl` into N cells (`cl_splitspike 1..4`; 2 = top/bottom, 3 = 2×2 with the
bottom-right cell black, 4 = 2×2) at 1280×720 on `wizs` with 2 bots.
Evidence: `work/spikeC-*.jpg`, `work/spikeC-run.log`.

**Renderer API as built (kept).** `void R_SetDrawRect(const vrect_t *rc)`
(`inc/refresh/refresh.h`, `src/refresh/draw.c`), NULL = whole window,
clamped to the window, reset to the whole window by every `R_BeginFrame`.
`draw.rect`/`draw.rect_set` in `drawStatic_t`; `GL_GetDrawRect()` returns
the effective rect; `GL_ScissorDrawRect()` keeps the scissor on (at most
the rect) whenever a rect is set. `GL_Setup2D` → viewport + ortho of the
rect; `R_SetScale` → ortho of rect×scale; `R_SetClipRect` → clip offset by
the rect origin and clamped to the rect; `R_RenderFrame` turns the scissor
off for 3D and the post-process FBO passes, and after its closing
`GL_Setup2D` converts `glr.fd.x/y` to rect-relative 2D coordinates, which
fixes `GL_Blend` and both `GL_PostProcess` composites (bloom, waterwarp)
in one place. With no rect set every path is arithmetically identical to
upstream (rect = 0,0,`r_config`), so N = 1 is unchanged. Client side:
`scr_cell` (window pixels) drives `scr.hud_width/height`, the refdef
origin in `V_RenderView` (`refdef.x = scr_cell.x + scr_vrect.x`) and the
AQtion `xhair_*` crosshair (`SCR_DrawXhair`, was `r_config`).

**Rendered correctly per cell** (2, 3, 4 cells): 3D view with the right
viewport; status bar, ammo/weapon icons, frag counter, classic crosshair
and the AQtion `xhair_*` crosshair centred in each cell; the DM layout
scoreboard, inventory pickup line, Action centerprints; screen blend /
damage tint (`ps.blend`, grenade self-damage) filling exactly each cell
including the right-hand cells (`spikeC-2cells-damage.jpg`,
`spikeC-4cells-damage.jpg`); `viewsize 80` inside each cell with tile
border (`spikeC-4cells-viewsize80.jpg`); bloom composite (`gl_shaders 1`,
`gl_bloom 1`, `gl_showbloom 1` paints exactly each cell,
`spikeC-4cells-showbloom.jpg`). Console, notify lines and chat stay full
screen, drawn once over all cells (by design; since 0.0.0.17 the notify
lines are drawn in player 1's cell, see "As built (M4 fixes)" below). No neighbour bleed: the
scissor cut every oversized layout at its own cell edge.

**2D assumptions that broke or need policy:**

1. *Action teamplay scoreboard is wider than 640 virtual px.* In a
   640×360 cell its header (Frags … Team) is clipped on both sides and the
   rows above y = 0 are cut (`spikeC-4cells-tpscoreboard-640x360.jpg`); the
   team header bars at the cell top are cut too
   (`spikeC-4cells-tpheader-640x360.jpg`). In a 360×270 cell (4 cells in a
   720×540 window; the closest the spike got to 320 wide) it is
   unreadable: only the right-hand fragment of each row shows
   (`spikeC-4cells-tpscoreboard-360x270.jpg`). The DM scoreboard fits
   1280×360 (`spikeC-2cells-scoreboard.jpg`) and is readable in 640×360
   except that its first entry starts above the cell top
   (`spikeC-4cells-scoreboard.jpg`). Consequence: the per-cell HUD scale
   of §3.2 is not enough for 8 players or teamplay at 640 wide; M2/M4
   need either a fractional scale (0.5–0.75, soft fonts) or a dedicated
   "fit layout to cell" transform for layouts only. Default for M1:
   integer scale, accept the clip.
2. *Centerprints do not wrap*: "You joined Team 1 (RED)." overflows a
   360-wide cell and is clipped at its right edge.
3. *`hud_scale` is global*: at 720p it is 1 for every cell. At 1080p the
   auto scale is 2 and a 960×540 cell becomes 480×270 virtual pixels;
   the per-cell scale rule of §3.2 is required, not optional.
4. *FOV is Hor+* (`cl_adjustfov 1`, fov 90 → fov_y 73.7°): a 1280×360
   cell gets fov_x ≈ 139°, a 640×360 cell ≈ 106° (same as full 16:9).
   Top/bottom 2-player is very wide; same trade-off as the Q3 fork
   (`cl_splitVertical`).
5. *AQtion ships `gl_polyblend 0` and `gl_shaders 0`* (the install's
   config): out of the box there is no damage tint, no bloom and no
   waterwarp, so the post-process paths are opt-in for players.
6. *Bloom/waterwarp FBO is sized to the refdef* and re-created whenever
   the refdef size changes (`R_RenderFrame`, `GL_InitFramebuffers`). The
   spike's layouts have equal cells, so no re-creation per frame was
   triggered; unequal cells (3-player fill, a `viewsize` per player)
   would re-create it every scene. Cost noted, not fixed.
7. *Waterwarp not exercised*: no water reachable on `wizs` without a
   `setpos`, which neither the engine nor Action has. It uses the same
   `GL_PostProcess` call and offset as the bloom composite.
8. *Per-player statics in 2D found by drawing twice*: `SCR_DrawXhair`
   keeps `last_rtime`, `gap`, `length`, `xh_elasticity` and the global
   `deltatime_factor` as statics (the second cell sees delta 0); add to
   the §1.3 inventory. The rerelease vignette (`GL_DrawVignette`,
   `gl_damageblend_frac`) draws at 2D origin 0,0 with the refdef size:
   right for a full-cell refdef, misplaced with `viewsize` < 100 (an
   upstream quirk, also at N = 1; Action does not send `damage_blend`).
9. Full-screen by design and untouched: console, loading plaque,
   menus (a full-screen menu still blanks every cell via
   `UI_IsTransparent`, §3.3), the debug-only text projection in
   `src/refresh/main.c` (`DEBUG_DRAWING`, uses `r_config`/2).
10. Flare flicker: not observed on `wizs` (no flares in view); still to
    check with two different POVs in M1.

**Frame cost** (RTX-class GPU, windowed 1280×720, `timedemo 1` to
uncap (note: `timedemo` is cheat-protected on a normal listen server;
`gl_swapinterval 0` alone uncaps, see the perf recipe below), `$r_fps` samples; CPU/driver-bound, rough): legacy GL
(`gl_shaders 0`, the AQtion default) about 800–1200 fps at 1 cell (noisy), 820 at
2, 650 at 3, 615 at 4 (≈ +0.2 ms per extra cell); GLSL (`gl_shaders 1`)
about 815 / 690 / 665 / 670; `gl_bloom 1` changed nothing measurable.
V_RenderView runs once per cell (scene rebuild + `CL_AddEntities` per
cell); nothing double-spawned visibly (trails are distance-based).

*As built (M4-T1, 0.0.0.16):*

- **Layouts 5–8** (`split_view.c` `CL_SplitRows` / `CL_SplitLayout`) are
  the Q3 §3.2 table: rows of cells, each row stretched to the window
  width. `cl_splitFill` 1 (default): 3 = 2+1, 5 = 3+2, 7 = 4+3, 6 = 3×2,
  8 = 4×2; `cl_splitFill 0`: 3 = 2×2, 5 = 3×2, 7–8 = 3×3 (cells keep the
  window aspect), unused cells black. The wide row is the bottom row;
  `cl_splitWidePlayer N` puts player N in its first cell, 0 (default) = the
  player who joined last (join order tracked per slot in the layout code);
  the rest follow in slot order. Changing N, the window, `cl_splitVertical`,
  `cl_splitFill` or `cl_splitWidePlayer` re-runs the layout on the next
  drawn frame. Note: the default fill also changes **3 players** from
  2×2-with-a-black-cell (0.0.0.6–0.0.0.15) to 2+1. Cell borders are drawn
  per cell (right / bottom edge inside the window), so rows of different
  widths get their own lines. `splitplayers` prints each player's cell.
  At 1280×720 the smallest cells are 320×360 (4×2) and 426×240 (3×3).
- **Item 1 / 3, per-cell HUD scale** (`screen.c` `SCR_CellScale`, the one
  place): the M1-T4 integer rule, then if the cell would be narrower than
  640 virtual pixels a fractional scale makes it exactly 640 wide
  (320×360 → ×0.5, 640×720 virtual; 426×240 → ×0.67, 640×360 virtual).
  `R_SetScale` / `GL_Setup2D` / `R_SetClipRect` take the float unchanged;
  the scissor still cuts at the cell edge. It covers everything
  `SCR_Draw2D` draws (status bar, layouts, ghud, centerprints, crosshair,
  chat HUD, cell status lines). Not covered, on purpose: our overlay menu
  (it already lays itself out to the cell width with the console font at
  ≥ 1×, readable at 320×360, `m4t1-e02`) and the engine UI in P1's cell
  (`CL_SplitUIScale`, unchanged). Result: the DM and the teamplay
  scoreboard, Action's team menu and the status bar fit inside every cell
  at 320×360 and 426×240 (`m4t1-a10`, `a11`, `b01`, `b03`, `b04`), but at
  320 wide the text is 4 px high and **not readable**; at 426×240 it is
  small but legible. The teamplay scoreboard of this TNG build fits a
  640×360 cell at scale 1 (`m4t1-e01`), unlike Spike C's note above.
  Centerprints still do not wrap (item 2), but at the fit scale they fit.
- **Item 6, FBO sizing** (`refresh/main.c` `GL_SizeFramebuffers`): the
  post-process FBO is sized to the largest scene that needed it in this
  and the previous frame, so it is re-created only when the layout or the
  window changes (run C, `set developer 1`: one `GL_SizeFramebuffers` line per
  layout change, none in 300 steady frames at N = 7). Smaller scenes render
  into its bottom-left corner; `GL_PostProcess` takes the texture
  coordinate extent, the blur passes use the blur textures' real texel
  size, and when a scene is smaller than the FBO the blur FBOs are cleared
  first so no other cell's bloom bleeds in. The bloom sigma follows the FBO
  height. One view: identical sizes to upstream (a shrink lands one frame
  later). Since 0.0.0.17 every world scene counts toward the size, not only
  the ones that need the FBO, so cells that are only partly under water
  (waterwarp without bloom) cannot flip it between sizes. Waterwarp uses the same sub-rect coordinates; in a cell smaller
  than the FBO its ripple is a little wider (the warp runs in texture
  space) and it may sample a few texels past the cell's part. Not
  exercised (item 7: no water reachable on `wizs`).
- **Found and fixed:** the 2D draw rect's scissor (window coordinates) was
  still on during the bloom passes into the blur FBOs, so for every cell
  except the bottom-left one the blur was cut away and those cells
  composited the bottom-left cell's bloom (present since Spike C; invisible
  in its same-view test and with AQ's maps, which barely glow).
  `GL_DrawBloom` now turns the scissor off for those passes. Verified with a
  test-only shader patch (bloom = the whole scene, not committed) and
  cheats on (`gl_showbloom` is cheat-protected on a normal listen server):
  `gl_showbloom 1` shows each cell's own blurred view, aligned, at N = 3, 5
  and 7 (`m4t1-t02`, `t04`, `t06`).
- **Perf pass** (RTX 5090, windowed, `gl_swapinterval 0` to uncap; the
  run also had `timedemo 1`, which a normal listen server refuses as
  cheat-protected, so the recipe is `gl_swapinterval 0` + `$r_fps` /
  `cl_splitperf 1`;
  `wizs` with 4 bots, players spread; `cl_splitperf 1` prints CPU time per
  phase every 2 s; fps, ±10 % run to run):

  | views | 720p sh0 | 1080p sh0 | 1440p sh0 | 720p sh1 | 1080p sh1 | 1440p sh1 |
  |---|---|---|---|---|---|---|
  | 1 | 929 | 675 | 702 | 923 | 707 | 667 |
  | 2 | 628 | 559 | 551 | 629 | 512 | 534 |
  | 4 | 511 | 508 | 502 | 536 | 521 | (not sampled) |
  | 6 | 538 | 477 | 475 | 480 | 486 | 521 |
  | 8 | 450–472 | 483 | 412–428 | 413–436 | 426 | 460 |

  `gl_shaders 1` + `gl_bloom 1`: 4 views 373 (1440p) / 432 (720p), 8 views
  281–331 at both. Per view the CPU submits `R_RenderFrame` in 0.12–0.19 ms
  and builds the scene (`CL_AddEntities` …) in 0.005 ms; resolution barely
  matters, so the frame is CPU / driver bound, not GPU bound. The ≥ 120 fps
  target with 8 views at 1440p holds with a wide margin (≈ 3.5×); no
  optimisation was needed. Evidence: `work/m4t1-run-perf.log`. Note: the
  release build has no `developer` command; diagnostics that read it
  (`GL_SizeFramebuffers`' line, `split_stufftest`) need `set developer 1`
  (plain `developer 1` goes to the server as chat).

*As built (M4 fixes, 0.0.0.17):*

- **Notify lines in player 1's cell** (`console.c` `Con_DrawNotify`,
  `screen.c` `SCR_SplitNotifyCell`): with 2+ cells player 1's console
  notify lines and its chat prompt draw inside player 1's cell, at the
  cell rule applied to the console scale (`SCR_CellScaleFor`, the same
  fit-to-640 rule as the HUD), cut at the cell's width. The console itself
  stays full screen. At 320-wide cells the text is as small as the HUD's
  (`m4fix-a01`, `a02`).
- **Chat vs layouts in short cells** (`SCR_DrawChatHUD`): while drawing
  cells, if a layout or the inventory is up and the chat lines would run
  into the layout's 320×240 box (centred in the cell's virtual size), the
  chat moves bottom-left just above the status bar numbers (`yb -24`) and
  keeps only the lines below the box (3 at 426×240 / 640×360 virtual).
  Nothing changes without a layout or with one view (`m4fix-a04`, `a05`,
  `b01`, `b02`).
- **Notices** (`split_view.c` `CL_SplitLayout`): players in use get their
  cells first (slot order, wide-cell rule among them), notices only the
  cells after them; the wide row's first cell is the sum of the rows above
  it; the layout re-runs when the set of slots changes, not only their
  number (a notice expiring in the frame a player joins). `m4fix-a08` ..
  `a11`.

---

## 4. Networking

### 4.1 What exists

- **Sockets.** `netsrc_t` = `NS_CLIENT`, `NS_SERVER`, `NS_COUNT`
  (`inc/common/net/net.h:62-66`). Unlike Quake3e, Q2PRO already has a
  separate client UDP socket: `udp_sockets[NS_COUNT]` /
  `udp6_sockets[NS_COUNT]` (`src/common/net/net.c:116-119`); the client
  one is bound to `net_clientport` (default any) by `NET_OpenClient`
  (`net.c:1180-1205`), the server one to `net_port` by `NET_OpenServer`
  (`net.c:1138-1151`; on a busy port it binds another and rewrites
  `net_port`). All sockets sit in one poll set, so `NET_Sleep`
  (`net.c:715`) wakes for any of them.
- **Loopback.** `loopbacks[NS_COUNT]` (`net.c:86`); `NET_SendLoopPacket`
  writes to `loopbacks[sock ^ 1]` (`net.c:584`); `NET_GetPackets` drains
  loopback then UDP v4/v6 of one `netsrc_t` (`net.c:800-815`), stamping
  `NA_LOOPBACK`.
- **How P1 reaches its own server.** `CL_CheckForResend`
  (`main.c:406-500`): when `sv_running > ss_loading` and the client is not
  connected, it sets `servername "localhost"`, `NA_LOOPBACK`, protocol
  `PROTOCOL_VERSION_AQTION`, and goes straight to `ca_connecting` — no
  challenge on loopback.
- **Server socket only in multiplayer.** `SV_InitGame`: deathmatch →
  `maxclients` 8 if ≤ 1, coop → 4, otherwise forced to **1**
  (`src/server/init.c:428-438`); `NET_Config(NET_SERVER)` only if
  `maxclients > 1` (`init.c:441-443`).
- **qport.** Cvar `qport` (`net_qport`, random 16-bit, `chan.c:129`). The
  client sets `cls.quakePort = net_qport & 0xff` for R1Q2/Q2PRO/AQtion and
  the full 16 bits for protocol 34 (`main.c:474-495`), sends it in
  `connect` (`main.c:499-500`), and `Netchan_Setup` stores it per channel
  (`chan.c:726-742`). The netchan writes `chan->qport` — already per
  channel — but only `if (chan->sock == NS_CLIENT)` (`chan.c:216-220,
  391, 507`).
- **Protocols** (`inc/common/protocol.h:27-75`): 34 default, 35 R1Q2, 36
  Q2PRO, 38 AQtion (minor 3011 minimum; with `AQTION_EXTENSION`: GHUD
  3012, CVARSYNC 3013, GHUD2 3014, CLIENTNUM_SHORT 3015, CINEMATICS 3016,
  EXTENDED_LIMITS 3017). The server offers `p=34,35,36,38` in its
  challenge (`server/main.c:1054-1055`); the client picks with
  `cl_protocol` (default 0 = best, `client/main.c:2855`).

### 4.2 Server-side rules that affect extra players (`src/server/main.c`)

| Rule | Where | Effect on N players from one machine |
|---|---|---|
| Loopback skips every check | `permit_connection` `:1114-1116` (`NET_IsLocalAddress` = `NA_LOOPBACK` only, `net.h:105`) | P1 only; extras arrive as `NA_IP` 127.0.0.1 |
| Challenge table keyed by **base address (IP, no port)** | `SVC_GetChallenge` `:1030-1052` overwrites the challenge of the same IP; `permit_connection` `:1119-1131` takes the first entry with that IP and rejects a mismatch, "Bad challenge." | two extras challenging at once overwrite each other → **joins must be serialized** |
| `sv_iplimit` default **3** | cvar `:2858`; enforced `:1153-1176`, counts every client with the same IPv4 (zombies count half): "Too many connections from your IP address." | local: P1 (loopback) is not counted, so P2–P4 fit and P5 is refused; remote: all players share the public IP → the 4th is refused |
| Slot reuse on exact `ip:port` | `find_client_slot` `:1427-1440` drops the old client ("reconnected") | distinct source ports per extra avoid it |
| Packet demux = base address + **8-bit qport** (enhanced protocols) / 16-bit (34) | `SV_PacketEvent` `:2116-2145`; a mismatched port is "fixed up" onto the matching client | extras' qports must differ **mod 256**, or packets reach (and steal the port of) the wrong client |
| Rate | `SV_UserinfoChanged` `:2646-2662`: loopback → unlimited; LAN only with `sv_lan_force_rate` (default 0, `:2830`) | extras on 127.0.0.1 get their userinfo `rate` (default 15000, `client/main.c:2872`) |
| Timeouts | `SV_CheckTimeouts` `:2266-2269` never times out loopback only | extras time out like remote clients (fine) |
| `sv_reserved_slots`, passwords | `:1353-1372`, `:1445-1456` | extras send P1's `password` userinfo |
| `sv_force_reconnect` (latched, default "", `:2813`) | `user.c:381,473,589` | handled by per-connection stufftext |
| Anticheat (`ac=` in `client_connect`, `client/main.c:1385-1389,1413-1431`) | r1ch AC servers | the AC module is per process; extras connect without it, AC-required servers refuse them (§8) |
| Bots | `src/action/botlib`, `sv addbot` / `addbots` (`g_svcmds.c:1127-1143`) | server-side, not counted by `sv_iplimit` |

**Action game DLL** (`src/action`, `ClientConnect` `p_client.c:3789-3905`):
IP ban list `SV_FilterPacket` (same IP for all — banned is banned),
`password`, and **no** per-IP or duplicate-name refusal. State keyed on
**ip + netname**: ghosts (stats restored on reconnect,
`a_cmds.c:1282,1306`, `p_client.c:4025-4038`), the LRCON claimer
(`p_client.c:3857-3868`, `g_lrcon.c:68,135`), silence-ban
(`p_client.c:3871`) — harmless as long as each player has a distinct
name. `steamid` / `discordid` userinfo (`p_client.c:3831-3836`; client
`main.c:2796,2881-2883`) would be duplicated → extras send them empty.

### 4.3 Design: one socket path for local and online (= Q3 §4.2)

- `netsrc_t`: add `NS_CLIENT2 … NS_CLIENT8` **after** `NS_SERVER`, so
  `NS_CLIENT ^ 1 == NS_SERVER` still holds for P1's loopback. Extras never
  use loopback (assert in `NET_SendLoopPacket`).
- `net.c`: sockets for `NS_CLIENT2..8` opened on join with
  `UDP_OpenSocket(net_ip, PORT_ANY, AF_INET)` (+ v6 twin only when the
  server address is v6), closed on leave and by `NET_Config(NET_NONE)`.
  `NET_GetPackets(sock, cb)` already takes the socket and `NET_SendPacket`
  already indexes `udp_sockets[sock]` (`net.c:823-850`). Fix the
  `sock == NS_CLIENT` tests: qport writes `chan.c:216,391,507` →
  `sock != NS_SERVER`; rate stats `net.c:564,594`; ICMP dispatch
  `net.c:627-640` → route `NS_CLIENTn` to `CL_ErrorEvent` of that context.
- **qport per context:** `cls.quakePort = (net_qport + 37 * playerNum) &
  0xff` (distinct mod 256 by construction; the 16-bit form for protocol
  34) in `CL_CheckForResend`; every `OOB_PRINT(NS_CLIENT, …)` /
  `Netchan_OutOfBand(NS_CLIENT, …)` / `Netchan_Setup(…, NS_CLIENT, …)` in
  the connect path (`main.c:456,499,1411`) uses `cla->sock`.
- **Target:** P1 on loopback → extras connect to `127.0.0.1:<bound port
  of NS_SERVER>` (`NET_GetAddress(NS_SERVER, …)`, `net.c:1268`) with the
  full challenge flow. P1 remote → extras copy P1's `serverAddress`.
  Needs the listen server's UDP socket, i.e. `maxclients > 1`
  (deathmatch): a join on a single-player server is refused with a hint
  (decision §11.2).
- **Our listen server treats 127/8 as local** (never a dedicated server):
  extend the loopback tests in `permit_connection` (skip ban/iplimit; the
  challenge stays), `SV_UserinfoChanged` rate and `SV_CheckTimeouts` to
  "`NA_LOOPBACK` or (`!COM_DEDICATED` and `NA_IP` 127.x.x.x)". Same rule
  as Q3 R11.
- **Serialized joins:** extra i+1 sends `getchallenge` only after extra i
  reached `ca_connected` (per-IP challenge overwrite) and — as Q3 R9 —
  only after i is `ca_active`, so a server that counts connecting clients
  late still applies its limit to the next one.
- **Refusals:** today an OOB `print` while challenging/connecting only
  prints and loops back to `ca_challenging` forever (`CL_ParsePrintMessage`
  `main.c:965-972`). For extras: "Bad challenge." → retry; anything else
  → `CL_SplitDrop(i, text)` with the text in that player's cell; "Too many
  connections from your IP address." is reworded as Q3 R11 ("this server
  allows only K players from one connection").
- `client_connect` side effects skipped for extras: `HTTP_SetServer`
  (`main.c:1396-1408`, would reset P1's download server), anticheat load,
  `Con_Popup` / `UI_OpenMenu`.

- *As built (M1-T2+T3, 0.0.0.5):* as above, plus: "No challenge for
  address." is retried like "Bad challenge." (at most 5 retries); an extra
  gives up after 10 unanswered connect attempts (~30 s); the extra's own
  `getchallenge` waits while another extra has one in flight or sits
  between connecting and active, lower slots first. Extras send
  `name "Player N"`, no `steamid` / `discordid`, player 1's other userinfo
  (incl. `password`); the userinfo cvars stay player 1's (an extra never
  queues userinfo deltas until T5). Rate statistics in `net.c` needed no
  change (only the loopback path is counted per socket, and extras never
  use loopback). Every `netchan_t` buffer (`message`, `reliable_buf`,
  `fragment_in/out`) is one `Z_TagMalloc` block from `Netchan_Setup`, so
  the `conn` memcpy swap is safe. Server `status` has no qport column; the
  client's `splitplayers` lists socket, qport, client number and origin.

### 4.4 Online: what an unmodified remote server may reject

| Check | Effect | Mitigation |
|---|---|---|
| `sv_iplimit` (default 3, all same-IP clients) | 4th player from one IP refused | none client-side; message in cell (Q3 §4.3 wording) |
| challenge per IP | simultaneous joins → "Bad challenge." | serialize (4.3) |
| 8-bit qport demux | colliding qports cross-deliver packets | distinct mod 256 (4.3) |
| anticheat-required servers | extras refused | none; document |
| ban / silence / ghost / LRCON by ip + name | shared IP is honest; distinct names avoid cross-talk | distinct default names "Player N" |
| `steamid` / `discordid` duplicates | stats / admin tools may merge players | blank for extras |
| server-side admin mods with per-IP policies | policy | none — do not evade |

Online play is honest multi-connection from one IP, like N PCs behind one
NAT (Q3 §4.3 stance).

- *As built (M3-T1+T2, 0.0.0.15):* extras copy player 1's `serverAddress`
  (no code change was needed for the connect itself). A refusal or a
  server-side drop keeps the player's cell for 10 s with the reason
  (`CL_SplitSetNotice`, split_view.c; the layout counts it as a cell):
  "can't join: this server allows only K players from one connection" (K =
  local players at `ca_connected`+, the server does not say its number),
  "can't join: <server text>", "Player 3 was kicked" (the server print that
  came right before `svc_disconnect`), "Server connection timed out.", "can't
  join: no answer from the server (10 tries)". While player 1 hears nothing
  from a remote server for 1 s, extras do not run their own timeout (player
  1's decides). When a remote server drops player 1 by an error (quit,
  crash, timeout, kick: not player 1's own `disconnect`), the extras and
  their profiles are remembered and rejoin, serialized, once player 1 is
  active on the same address again within 10 minutes (`reconnect`).
  `sv_force_reconnect` works per extra: its `connect <addr>` stufftext
  reconnects only that player (same address) and the `svc_disconnect` that
  comes with it waits until that stufftext has run. A q2pro dedicated
  server's `map` is a full restart (`svc_reconnect`); `gamemap` is the
  `changing` / `reconnect` path; both verified. Test rig:
  `scripts\run-ded.ps1` (this machine's LAN IPv4, port 27920).

---

## 5. Input

### 5.1 What exists

- **Windows** (`src/windows/client.c`): keyboard through `WM_KEY*`
  (`legacy_key_event` `:591-606`) and the low-level hook (`:506-531`);
  mouse through Raw Input (`register_raw_mouse` `:1081-1093`,
  `raw_mouse_event` `:669-708`, `Win_GetMouseMotion` `:1068`). **No
  joystick or gamepad code on any platform** (grep for joy /
  GameController / XInput is empty).
- **SDL** (`src/unix/video/sdl.c`) is a *video driver*
  (`SDL_InitSubSystem(SDL_INIT_VIDEO)` `:281`), compiled only when the
  `sdl2` option finds SDL (`meson.build:386-390`). Linux CI enables it;
  **the Windows release is MSVC x64 with `-Dsdl2=disabled
  -Dwrap_mode=forcefallback`** (`.github/workflows/build.yml:24-29,81-110`),
  and there is no `subprojects/sdl2.wrap`. Turning `sdl2` on for Windows
  would also add the SDL video driver to `vid_drivers`
  (`client/refresh.c:242-252`) — not wanted.
- **Keys:** 256 key numbers (`inc/client/keys.h`; `Key_Event` asserts
  `key < 256`, `keys.c:614`); `keybindings[256]` etc. (`keys.c:24-39`).
  Dispatch: a `+cmd` binding queues `"+cmd <key> <time>"` on `cmd_buffer`
  (`keys.c:709-753`) → `KeyDown(&in_forward)` reads key and time from
  argv (`input.c:249-283`).
- **usercmd:** `CL_UpdateCmd` (`input.c:600-632`: `CL_AdjustAngles` +
  `CL_BaseMove` + `CL_MouseMove` + accumulated mouse move, clamps) every
  client frame; `CL_FinalizeCmd` (`input.c:738-821`: buttons from
  `in_attack`/`in_use`, `BUTTON_ANY` from `Key_AnyKeyDown`, move vector,
  impulse, store in `cl.cmds`, clear kbuttons) every physics frame;
  `CL_SendCmd` (`input.c:1130-1168`) default or batched (`cl_batchcmds 1`)
  moves. Instant packets on `+attack`/`+use` set `cl.sendPacketNow`
  (`cl_instantpacket 1`, `input.c:702`).
- **userinfo:** built from the global `CVAR_USERINFO` cvars
  (`Cvar_BitInfo` in `CL_CheckForResend` `main.c:498` and
  `CL_SendUserinfo` `input.c:1080-1111`); live deltas via
  `CL_UpdateUserinfo` (`main.c:1600-1645`). Userinfo cvars: `password,
  spectator, name, skin, rate, msg, hand, fov, gender, uf, steamid…,
  version, cl_discord_id` (`main.c:2796,2868-2885`).
- **AQtion cvar sync:** the game registers client cvars (`cl_antilag`,
  `cl_indicators`, `cl_xerp`, `cl_spectatorhud`, `cl_spectatorkillfeed`,
  `src/action/g_save.c:828-832`); the client answers from global cvars
  (`CL_SendCvarSync` / `CL_ParseCvarSync`, `parse.c:1378-1440`).

### 5.2 Design (Q3 §6.2, §12–14 and §17.1 apply unless noted)

- **Per-player button state:** the kbutton statics (`input.c:240-247`) and
  `autosens_x/y` move into `cla->in` behind file-local macros.
  Keyboard/mouse drive the KBM player (`cl_splitP1Input`, Q3 §14.2: `pad`
  default — first pad is P1 and keyboard/mouse also drive P1; `kbm` —
  keyboard is P1, the first pad is P2). `CL_MouseMove` runs only for the
  KBM context; other contexts get the pad move instead.
- **Per-player commands for free:** register `p1`..`p8`
  (`CL_PushContext(N-1); Cmd_ExecuteString(&cmd_buffer, args);
  CL_PopContext()`). Every client command, `+attack`, `weapnext`,
  `invnext`/`invuse`, and every command forwarded to the server
  (`CL_ForwardToServer` `main.c:340` uses the active `cls.netchan`) becomes
  that player's. Pad key numbers are `256 + padKey` in the `+cmd <key>
  <time>` suffix, so `KeyDown`'s two-key tracking cannot collide with
  keyboard keys.
- **Pad layer:** new `src/client/split/pad.c` with its own virtual key set
  and per-player bind tables (`pbind` / `punbind` / `pbindlist`, written by
  a hook in `Key_WriteBindings` `keys.c:458`), polled from `IN_Frame`
  (`input.c:134`). Analog move/look goes straight into the owning context
  (`CL_GamepadMove`; aim curve, turn boost, zoom scale per Q3 §14.4 — the
  zoom scale follows Action's sniper zoom like `autosens`). Device
  identity, auto-assignment, hot-plug, hold-to-join, join hint, leave =
  hold Back+Start: Q3 §6.2 "Join / drop flow", §12.1, §14.1, §17.1.
  *As built (M2-T1, 0.0.0.9):* one file, `src/client/split/in_gamepad.c`
  (device layer, binds, analog, `padinject`), not `pad.c` + `pad_sdl.c`.
  Hooks: `PAD_PreInit` in `CL_PreInit` (the `pbind` commands must exist
  before `config.cfg` runs), `PAD_Frame` in `IN_Frame` (SDL polled at most
  every 2 ms; new devices looked for every 500 ms by
  `SDL_NumJoysticks`, no SDL event queue), `CL_GamepadMove` at the end of
  `CL_BaseMove`, `CL_GamepadLook(msec)` after `CL_MouseMove`,
  `PAD_WriteBindings` in `Key_WriteBindings`. A bind runs as
  `p<N> +cmd <256+padKey> <time>` through `cmd_buffer` (inserted at the
  front when it comes from `padinject`). Cvars use Q3's `joy_*` names and
  values, except `joy_zoomScale` (default 1): the look rate while zoomed is
  `fov_x / 90 × joy_zoomScale` (Action's 2x/4x/6x = 0.5/0.22/0.11), like
  `m_autosens`, instead of Q3's fixed 0.5. Left stick: magnitude ×
  `cl_forwardspeed` / `cl_sidespeed`, doubled when running (`cl_run` xor
  +speed), no walk bit (Q2 has none). Device identity: `guid#n` (n-th
  device with that GUID); a replugged device with the same key gets its
  player back. `padinject <player> <padkey|LX..RT|disconnect> <value>`
  drives a per-player virtual pad (created and assigned on first use).
  Player 1's pad is muted while `cls.key_dest != KEY_GAME` (keys) or a
  menu / the console is up (sticks); extras' pads are not.
- **Backend recommendation: SDL2 GameController, pads only.** Reasons as
  Q3 §6.2: 8+ pads, PlayStation/Switch/8BitDo via HIDAPI, hot-plug, GUIDs,
  rumble, and the same code on Linux where SDL is already linked. Since
  the shipped Windows exe has no SDL:
  - Windows: add `subprojects/sdl2.wrap` (WrapDB) and a new option
    `-Dgamepad=enabled` that links SDL2 **statically with only the
    joystick / gamecontroller / hidapi subsystems**, independent of the
    existing `sdl2` (video/audio) option, so `vid_drivers` is unchanged.
    Spike P must prove the WrapDB SDL2 port builds under MSVC with
    HIDAPI + RawInput/XInput enabled. **Fallback** = runtime
    `LoadLibrary("SDL2.dll")` (Q3's original path: DLL beside the exe, no
    build-system change, engine still runs without it). **Last resort** =
    XInput (4 Xbox-class pads, zero deps) behind the same small device
    interface.
  - Linux/macOS builds with `-Dsdl2=enabled`: use the engine's SDL
    (`SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER)`), no second init.
- **Menus with a pad.** Action's team/weapon/item menus are server `PMenu`
  layouts navigated with `invnext` / `invprev` / `invuse` / `inven`
  (`src/action/g_cmds.c:1934-1941`, `a_menu.c`) — per player by
  construction. The pad layer only adds a **menu layer**: while that
  player's `cl.frame.ps.stats[STAT_LAYOUTS] & LAYOUTS_LAYOUT` is set,
  d-pad up/down → `invprev`/`invnext`, A → `invuse`, B → `inven` (close).
  The scoreboard also sets that bit; A there sends `invuse` (uses the
  selected item, harmless). The engine UI (§7.1) uses Q3 §13.2's mapping
  for P1.
  *As built (M2-T2+T3, 0.0.0.10):* the menu layer runs while
  `STAT_LAYOUTS & (LAYOUTS_LAYOUT | LAYOUTS_INVENTORY)` (what Escape checks
  in `keys.c`); **B sends `putaway`**, not `inven`: Escape's own command,
  closes the menu *and* the scoreboard, where `inven` over the scoreboard
  would open the team menu. Press is consumed (release does nothing).
  Verified pad-only: Action's teamplay join menu opens by itself (Action's
  auto menu), d-pad up + A joined BLUE, then weapon and item menus with
  d-pad + A. **Default binds** (one table, `padDefaultBinds`, applied when a
  pad is given to a slot with no pad binds and by `padbinddefaults <n>`;
  `Key_WriteBindings` writes a slot only if it differs from the table).
  The maintainer's rule: the Quake3e fork's Urban Terror layout, same
  button for the same idea, Action's commands:

  | Pad | AQ2 command | UrT (Q3 fork) |
  |---|---|---|
  | A | `+moveup` (jump) | `+moveup` jump |
  | B | `+movedown` (crouch) | `+movedown` crouch (toggle) |
  | X | `reload` | `+button5` reload |
  | Y | `bandage` | `+button6` bandage |
  | Back | `score` (toggle, cycles the teamplay boards) | `+scores` |
  | Start | `padmenu` (overlay, since 0.0.0.11; its first item = `inven`) | `padmenu` (overlay) |
  | LB | `lens 1` (zoom back to 1x) | `ut_zoomreset` |
  | RB | `weapnext` | `weapnext` |
  | LT | `lens` (sniper zoom cycle 1x 2x 4x 6x 1x; `lens in` until 0.0.0.31) | `ut_zoomin` |
  | RT | `+attack` | `+attack` |
  | L3 | `+speed` (walk; AQ2 has no sprint) | `+button8` sprint (toggle) |
  | R3 | `use Combat Knife` | `ut_weaptoggle knife` |
  | D-pad up | `drop item` (menu: `invprev`) | `ut_itemdrop` |
  | D-pad down | `drop weapon` (menu: `invnext`) | `ut_weapdrop` |
  | D-pad left | `irvision` | `ut_itemuse nvg` |
  | D-pad right | `weapon` (weapon mode) | `+button3` fire mode |
  | Misc (Share) | `opendoor` | `ut_itemuse` |
  | Paddles 1-4 | `radio enemys` / `cover` / `reportin` / `taking_f` | (unbound) |
  | Left / right stick | move / look | move / look |

  `inven` / `invnext` / `invprev` / `invuse` are not on free buttons: the
  menu layer gives them to the d-pad and A whenever a menu is up.
- **Join / leave / hot-plug, as built (M2-T3, 0.0.0.10)**, Q3's rules and
  cvar names (`in_gamepad.c`): a pad nobody has is free; **any button** on a
  free pad takes a player that is missing a pad (lost it, an extra added from
  the console, or P1 with `cl_splitP1Input pad`, the default); otherwise
  **holding `cl_splitJoinButton` (PAD_A) for `cl_splitJoinHold` 700 ms** (Q3
  §17.1's value, not "~1 s") runs the `addplayer` path
  (`CL_SplitAddPlayer`, lowest free slot below **`cl_splitMaxPlayers`**,
  default 4, also caps `addplayer`). Refusals (demo, not in a game,
  single-player server, cap, socket) print the `addplayer` text and show
  "Can't join: ..." in the hint corner for 5 s. **Leave:** Back+Start held
  `cl_splitLeaveHold` 2000 ms (Q3's value) drops an extra; on P1's pad it
  only frees the pad. **Hot-plug:** Q3 lets a player whose pad vanished idle
  forever; here an extra leaves after **`cl_splitPadGrace` 30 s** (new; 0 =
  never, Q3's behaviour) and its cell shows "Player N: controller
  disconnected (leaves in N s)"; the same device key back within the grace
  (or any button on a free pad) takes the player again. **Join hint**
  (`cl_splitJoinHint` 1/2/0 as Q3 §14.1, Q3 wording "Player N: hold A to
  join" / "press any button"), drawn by `screen.c` after the cells and
  before the menus, bottom right above the status bar, console font at 2x
  from 720p; the "online servers may limit extra players" line on a remote
  server. No pad-type labels (always "A", "Back+Start"). `BUTTON_ANY`: any
  button / trigger of a player's pad sets it in that player's usercmd
  (player 1: only in game). `padinject <N>` now drives **virtual pad N**
  (device key `virtual-<N>`, connected unassigned on first use, like a
  real pad being plugged in), not player N's pad.
- **Per-player userinfo:** as Q3 §5.2/§13.3. For contexts > 0 the
  `Cvar_BitInfo` output is overlaid with the slot's shadow values
  (`p<N>_name`, `skin`, `hand`, `fov`, `gender`, `spectator`, `msg`,
  `rate`), `steamid*` / `cl_discord_id` blanked, `password` shared; a change
  of a shadow cvar marks *that* context `userinfo_modified =
  MAX_PACKET_USERINFOS`. AQtion cvar sync answers from the slot's shadow
  for the synced names; the per-player list is data (`playercvar`, Q3
  §13.3 R11).
  *As built (M1-T5, 0.0.0.7):* one table in `split.c` (key, default);
  `p2_..p8_` shadows exist from startup, archived where the base cvar is,
  default = the base cvar's (name `Player N`). An extra always sends its
  **whole** userinfo (`clc_userinfo`) on a shadow change, never deltas.
  Blanked keys: `steamid`, `steamcloudappenabled`, `steamclouduserenabled`,
  `cl_discord_id`, `discordid`. A change of a base key without shadows
  (`password`, `uf`, ...) resends every extra's userinfo; a base userinfo
  cvar changed while an extra is active queues player 1's full update.
  `p<N> <key> [value]` reads/writes the shadow (also for cvar-sync names).
  `info_hand` is swapped to the active context's `hand` shadow in
  `CL_ActivateContext`, so the renderer's left-hand gun and the beam
  offsets follow each cell without a renderer change. **Action pins
  `ps.fov` at 90** (only sniper zoom changes it), so `fov` / `p<N>_fov`
  reach the server's userinfo but change no view, for player 1 as before.
  Cvar sync for extras is not callback-driven: a 20 ms poll per extra
  compares `p<N>_<cvar>` (if it exists, e.g. via `set`) or the shared cvar
  with the last value sent; player 1 keeps upstream's `changed` callback.
- **Profiles, guests, per-player settings:** Q3 §12.3, §13.1, §14.3
  unchanged (files under `homedir`).
  *As built (M2-T5, 0.0.0.12, `split/split_profile.c`):* one file per
  profile, `<homedir>/action/profiles/<Name>.cfg` (Q3 splits it into an
  all-mods and a per-mod file; Q2 has one mod). Plain slot-free lines:
  `name skin hand fov gender msg rate` (the userinfo shadows minus
  `spectator`), the 12 `joy_*` feel cvars, `pbind <PAD_KEY> "<command>"`;
  unknown lines are ignored, a file with no `pbind` lines gets the default
  set. The profile name is the file name (sanitised as Q3: letters, digits,
  `-`, `_`, max 15, not "guest", case kept); renaming yourself in game only
  changes the `name` line, the profile keeps its file name (Q3 renames the
  profile). "Save as new" sets the in-game name to the profile name (Q3).
  Named profiles are saved by polling: every 500 ms the slot's settings are
  rendered as profile text and compared with what the file holds; a change
  is written 1 s after it was first seen, and on leave / quit. **Guests:**
  players 2..8 start as guests at startup and become guests again when they
  leave (name `Player N`, shadow defaults, player 1's current `joy_*` as the
  feel, the default pad set) — so a guest's changes last for its stay, and
  `set p2_name Bob` before `addplayer` still works. The `p<N>_*` shadows and
  `pbind 2..8` are **no longer written to `q2config.cfg`** (an older
  config's `seta p2_*` lines lose their archive flag and are dropped on the
  next write); only `pbind 1` stays there. **Profile follows the pad:**
  `profiles/_pads.cfg` maps a pad's device key (`guid#n`, i.e. the GUID
  plus the ordinal among pads of that GUID, so two identical pads differ;
  Q3 used the bare GUID) to its last named profile, recorded whenever a pad
  is given to a named slot or a slot with a pad loads / creates a profile;
  a pad that joins by holding A loads it (refused if another player uses
  it: guest). Q3's join-time picker is not built: the pad's profile loads
  directly, any other is picked from the overlay. **Player 1** has no
  profile (its config) unless one is loaded / created for it; its own
  values are then kept in memory and in `profiles/_p1keep.cfg` and put back
  on `profile guest 1`, at quit (before the config is written) and at the
  next start after a crash. *Since 0.0.0.13 (Q3 §14.3, acceptance note
  1):* a pad that takes player 1 (`cl_splitP1Input pad`) and that
  `_pads.cfg` remembers brings its profile to P1 the same way; it is put
  away (P1's own values back) when that pad steps out (Back+Start,
  `cl_splitP1Input kbm`, another pad taking P1), not when it is merely
  unplugged, and at quit. A profile picked by hand stays. Switching P1
  from one profile to another applies P1's kept values first, so the new
  profile does not inherit the old one's values for keys it lacks. Note: a
  profile switch is a name change, and q2pro servers allow 5 per minute
  (`sv_namechange_limit`); a refused change is stuffed back by the server. Q3's editable Guest defaults page (§14.3) is not
  built (the host's own `joy_*` are the guests' feel). Overlay: pause page
  "Profile: <name>" -> Save / Save as new profile... (on-screen keyboard) /
  Use guest / the list (in-use ones shown with their player). Console:
  `profile list | load <p> <name> | save <p> [new name] | guest <p>`,
  `addplayer [profile]`; `p<N> <cvar>` now also reads/sets an existing
  `p<N>_<cvar>` (`p2 joy_invertPitch`).
- **Headless test hooks** (agents have no pads): `addplayer [n]
  [profile]`, `dropplayer n`, `p<N> <cmd>`, `padinject <virtual pad>
  <key|axis> <value>` — as Q3 §6.2 / M2.

---

## 6. Sound (`src/client/sound/*`)

What exists: one listener (`listener_origin/forward/right/up/entnum`,
`sound/main.c:35-39`) set by `CL_CalcViewValues` from `cl.refdef`
(`entities.c:1505-1508`), and `listener_entnum = cl.frame.clientNum + 1`
in `S_Update` (`sound/main.c:866-873`); DMA spatializes against it
(`dma.c:648-690`), OpenAL sets it as the AL listener (`al.c:475-477`);
"full volume when from the listener entity" (`sound.h:171`,
`main.c:552`); local sounds play on `listener_entnum` (`main.c:742-755`);
loop sounds and entity origins read `cl.frame` / `cl_entities` of the
**active** context while mixing (`dma.c:736-761`, `al.c:339-350`,
`CL_GetEntitySoundOrigin` `entities.c:1538+`); sexed sounds look up
`cl.clientinfo` (`main.c:417-450`). `s_enable` tries OpenAL first, DMA
second (`main.c:137-198`).

Decision = Q3 §7 (one mixer, nearest listener wins):

- `listeners[MAX_SPLITVIEW]` filled by `CL_CalcViewValues` per context;
  context 0's is also the OpenAL listener and the DMA reference axis.
- Every `S_StartSound` is tagged with the issuing context
  (`cla->playerNum`). **Dedupe**: the same (sfx, entnum, entchannel)
  started by another context within the same client frame is dropped —
  every server event (weapon fire, footsteps, pain, `svc_sound`) arrives
  once per connection. Local sounds (`talk.wav` on chat, menu sounds)
  dedupe by sfx within 50 ms.
- Spatialize each channel against the nearest valid listener; stereo pan
  only for N ≤ 2, mono at N ≥ 3 (`s_splitPan` override). The "full
  volume" test becomes "entnum equals *any* listener's entnum".
- Moving sound origins resolve through the issuing context's
  `cl_entities` (the channel keeps the context index), so an entity
  outside P1's PVS still moves correctly.
- Loop sounds: `S_BuildSoundList` iterates all contexts and merges by
  entity number.
- OpenAL has one listener: use P1's listener and dedupe only, unless the
  DMA backend is active (decision §11.5). M1 ships "listener = P1 +
  dedupe" on both backends; the nearest-listener rule lands in M2.
- `OGG_*` music, `S_StopAllSounds` from `CL_ClearState` / `CL_Precache_f`
  (`main.c:691,1767`): P1 only.

*As built (M1-T6, 0.0.0.7, `split/split_snd.c`):* listener = P1 on both
backends (`CL_CalcViewValues` sets it only for context 0; `S_Update`
pushes context 0 if called with another active). All splitscreen work is
behind `s_split_numents` (extras active on P1's level, refreshed in
`S_Update`), 0 at N=1. Dedupe window: 40 ms for entity/world sounds (the
server sends one frame's packets to every connection back to back; the
copies arrive ~17 ms apart over 127.0.0.1), 50 ms for local sounds by sfx.
An entity's non-auto channel is deduped by (entnum, entchannel) whatever
the sfx, because client-picked variants (footsteps, landing) differ per
context and `S_PickChannel` would override that channel anyway. Fixed-
origin sounds also compare the origin (8 units). Full volume = `entnum`
is any active local player's entity, so a far-away P2's own gun is full
volume while its ricochets/impacts spatialize at P1's listener. Moving
origins: the channel keeps the issuing context; its entities are used,
else P1's, else any context with the entity in its current frame. Loop
sounds (`S_BuildSoundList`) are still P1's frame only (merge = M2).
`s_splitdebug 1` prints a per-frame dedupe counter, `2` every start/drop.

*As built (M2-T6, 0.0.0.12):* `CL_CalcViewValues` of an extra calls
`S_SplitSetListener` (view origin + right vector, time-stamped);
`S_SplitUpdate` lists the extras' listeners that are current (set within
250 ms, active, on P1's level). **DMA:** `SpatializeOrigin` (`dma.c`)
spatializes against player 1's and each extra's listener and keeps the
loudest (= nearest, as Q3); with 3+ players the result is mono (L = R =
the average, Q3's rule); `s_splitPan` -1 (default) = that rule, 0 = always
mono, 1 = always stereo. Loop sounds: `S_SplitLoopSounds` lists player 1's
frame's loop entities, then those only an extra's frame has (merged by
entity number, origin from that extra's entities); `AddSplitLoopSounds`
mixes them as `AddLoopSounds` does. Note a non-extended server sends a
model-less sound entity (a looped `target_speaker`) only within 400 units
of the player, so "only P2 hears it" is common. `s_splitdebug 2` logs each
DMA channel start with the listener it is heard from; `1` logs the loop
list when it changes. **OpenAL** (`s_enable 2`) is unchanged: one AL
listener = player 1, dedupe and full volume for local players; not forced
to DMA (a backend switch is a `snd_restart` with the game running; not
worth it). AQtion's default here is DMA: `s_enable 1` starts DirectSound
(OpenAL only with `s_enable 2`).

---

## 7. UI, console, menus, lifecycle

### 7.1 Engine UI and per-player menus

- The engine UI (`src/client/ui/*`, scripted by `ui/q2pro.menu`) is a
  singleton `uis` (`ui/ui.h:276-309`) with one menu stack. It reads no
  `cl` / `cls` fields (only cvars and commands), and AQtion has **no
  client-side mod menu**. It stays shared, owned by P1/host.
- In game with N>1, Esc / P1's Start opens it **inside P1's cell**
  (`uis.width/height` from the cell, `R_SetDrawRect`), forced transparent
  so the other cells keep rendering (§3.3). At the main menu it is full
  screen as today.
- Players 2+ (and P1's pad, by option) get the small engine-drawn
  per-player overlay of Q3 §12.2, much cheaper here because Action's real
  choices are server menus: **Resume / Team & weapons** (sends `p<N> menu`
  → the server opens its `PMenu` layout in that player's cell) **/
  Controls / Profile / Leave game**. No UI instancing (Q3 §13.3's
  one-UI-VM-per-player) is needed.
- Host splitscreen settings page (fill, wide player, orientation, join
  button/hold, hint, P1 input, max players, guest defaults): Q3 §13.4,
  §14.3, as an engine menu page (a `q2pro.menu` entry or a C page).
- *As built (M2-T4, 0.0.0.11), `split/split_menu.c`:*
  - **Engine UI in P1's cell.** `ui.c` asks `CL_SplitUIRect` (N > 1 and
    P1 `ca_active`): the UI's screen becomes P1's cell (`uis.width/height`
    from the cell, scale = the per-cell HUD rule capped by the window's
    unless `ui_scale` is set, `R_SetDrawRect` around `UI_Draw`, mouse and
    warp offset by the cell), `Menu_Init` re-runs when the cell changes, and
    `UI_IsTransparent` answers true so the other cells keep drawing (an
    opaque menu such as AQtion's `first_start` paints only P1's cell). Main
    menu / not in game / N = 1: full screen as upstream. This needed no
    `menu.c` layout change.
  - **P1's pad drives the engine UI** (Q3 §13.2): while `KEY_MENU` is up
    (not the console) d-pad / left stick = arrows, A = Enter, B / Back /
    Start = Escape (out of a game Start opens P1's overlay instead, see the
    host page), sent through `Key_Event` (down, then up on release).
    A on a **text field** opens the on-screen keyboard for it instead
    (`UI_GetFocusedField` / `UI_SetFocusedField`), so AQtion's "Choose your
    name" works with a pad, in P1's cell or full screen. No right-stick
    cursor (Q3's virtual mouse) yet. Extras' pads never reach the engine UI.
  - **Per-player overlay** (Q3 §12.2): **Start = `padmenu`** (the default
    bind, replacing `inven`) opens it in that player's cell only; every pad
    key goes to it (none to binds), sticks / `BUTTON_ANY` idle, held binds
    released on open. Items: **Team & weapons** (`p<N> inven`, Action's own
    menu in the cell), Scoreboard (`p<N> score`), Radio (15 Action radio
    calls, `p<N> radio <msg>`), Settings (Name via the keyboard, invert
    look, look speed H / V, gun hand: `p<N>_name`, `p<N>_joy_invertPitch`,
    `p<N>_joy_yawSpeed` / `pitchSpeed` (created on first use, not archived),
    `p<N>_hand`; P1's are the shared cvars), "Profile: Guest" (text until
    T5), P1 only: Game menu (the engine menu in P1's cell) and Splitscreen
    settings (the host page, below); Leave game (confirm page, `CL_SplitRequestDrop`) / P1: End
    game (confirm, `disconnect`). No Resume row (B / Start close). Drawn
    with the console font, magnified so the cell is about 320 lines;
    Q3's layout and footer wording; AQtion's menu blue as the accent.
    Values are set `FROM_CONSOLE` (a `FROM_CODE` set skips the shadows'
    `changed` callback, so the server would not hear a name / hand change).
  - **On-screen keyboard** (Q3 §12.4): Q3's grid and buttons (A type,
    X delete, Y space, L3 shift, Start done, **B cancel**), 15 characters
    for names.
  - **Host page** (corrected after M2 acceptance, 0.0.0.13; the
    maintainer's rule, Q3 §13.4): a page of **player 1's overlay**, not an
    engine menu, and **no entry in AQtion's native menus** (0.0.0.11-12 had
    a compiled-in `q2pro.menu` page plus run-time " Splitscreen " entries;
    both removed, `script.c` / `menu.c` are upstream again). Reached in game
    from P1's overlay ("Splitscreen settings", drawn in P1's cell) and, out
    of a game (main menu), by **Start on P1's pad** (`CL_SplitMenuPadStart`;
    in game Start over the engine UI stays Escape) or `padmenu 1`: P1's
    overlay opens full screen over a dimmed main menu with the host page and
    a "Player 1 profile" row. Rows: player 1 uses, two players (top/bottom,
    side by side), max players, join by (press / hold 0.3-1.5 s), join
    hint, cell borders, a static "Join: hold A  Leave: hold Back+Start" note
    (Q3 §14.1), map (`cl_splitStartMap`, cycling the `maps/*.bsp` the
    filesystem has, read when the page opens), game (deathmatch / teamplay;
    shown as Deathmatch before the game DLL created `teamplay`, a latched
    value shows what the next map gets), **Start multiplayer game**
    (`splitstartgame [map]`: `set deathmatch 1; coop 0; maxclients max(8,
    current); map`, inserted ahead of the command buffer). Items apply
    **live** as the overlay's other rows, so `cl_splitVertical` relays out
    while the page is open. The pad-layer rows are hidden in a
    `-Dgamepad=disabled` build. Since M4-T1 (0.0.0.16) it also has
    "3, 5, 7 players" (`cl_splitFill`: Fill the screen / Equal cells;
    renamed "Cell layout" in 0.0.0.17, it also switches 8 players between
    4×2 and 3×3) and
    "Wide cell for" (`cl_splitWidePlayer`: Last joined / Player 1..8) next
    to "Two players", and "Max players" defaults to 8. Not on it yet: Q3's
    guest defaults.
    *As built (0.0.0.20, first run):* the Map row offers only maps on the
    search path and a saved `cl_splitStartMap` that is gone becomes `wizs`
    (else the first map) when the page opens; `splitstartgame` likewise
    never starts a map that is not installed. The first button on the pad
    that becomes player 1's also acts (Start at the main menu opens the page
    at once). **AQtion's native menus** (pak1 `q2pro.menu`, unchanged data):
    the "Play Offline (Bots)" pages run `map $nextserver force` from a
    `strings` row over `$com_maplist`; with `nextserver` unset that row was
    blank and the map became "force". `Strings_Push` (`menu.c`) now shows
    and, on pop, sets the first entry when the cvar is empty (only then: a
    custom value is still left alone). `allweapon` / `allitem` (the same
    pages' `set allweapon $allweapon`) and `kill[#]` / `round[on]`
    (`triggers.cfg`) are pre-created at "0" (`CL_SplitMenuInit`). **No name
    prompt:** `pushmenu first_start` (pushed by Steam's `postinit.cfg` while
    `name` is "AQtionMan") is skipped in `ui.c`; `Com_ExecPostinit`
    (`common.c`) turns "AQtionMan" into "Player 1" and hides a name with
    spaces from postinit's `if $name == AQtionMan` (restored by a `set name`
    queued right behind the file, so a `wait` in the buffer cannot reorder
    it).
  - **Overlay over a server layout** (acceptance note 4): opening a
    player's overlay sends `p<N> putaway` first if its layout (Action's
    menu, scoreboard) is up, and the panel is opaque, so a layout that
    opens afterwards does not show through.
  - **Back / Start** act on their release, and not at all if the other one
    was pressed meanwhile: Back+Start (the leave chord) no longer flashes
    the overlay or the scoreboard (acceptance note 2).
  - **Cell borders**: `cl_splitBorder` 1 (default) draws 2 px black lines
    (1 px under 720 lines) on the internal cell edges after the cells. The
    Q3 fork has none (its cells meet edge to edge; "Q3's colour" = its
    black empty cells).
  - *As built (0.0.0.23, the maintainer's menu round; supersedes the item
    lists above):*
    - **AQtion's own menus 2x** (`ui.c` `UI_MenuSizeScale`, `menu.c`
      `Menu_NeededHeight`): with `ui_scale 0` (auto) the engine UI's scale
      is upstream's auto scale (or the cell rule) times
      **`cl_splitMenuSize`** (archived, default 2, 1 = upstream, clamped
      1-4), lowered per top menu so that menu still fits: virtual height >=
      the menu's own laid-out height (items + banner + bounds + status line;
      a self-sizing menu counts as 240) and virtual width >= 400; never
      smaller than upstream. Re-evaluated on push / pop (`UI_CheckMenuSize`,
      a full `UI_Resize` only when the scale changes), so a long page (Video
      settings) may draw smaller than the main menu. The scale is
      fractional (conchars filtered). An explicit `ui_scale` wins unchanged.
      Our overlay keeps its own size (fit-to-320 lines, unrelated cvar).
    - **Per-player overlay, new order:** Team (hidden when hosting a game
      without teams or a tourney), Weapons (`p<N> inven`: Action's weapon /
      item menu, its join menu first for a player on no team; hidden when
      hosting a game without weapon choice: deathmatch without
      `dm_choose 1`, team DM without it, jump mode), Scoreboard, Radio,
      Settings, **Profile...** (or "Profile: <name>"), P1 only: **Server
      options** (disabled "host only" on another machine's server),
      Splitscreen settings, Game menu; Leave / End game. The game mode is
      read from the game cvars only when P1 hosts (`SM_Hosting`: listen
      server + P1 on loopback); a Q2 client gets no serverinfo from a remote
      server, so online both rows show.
    - **Team page:** Join team 1 / 2 (CTF: red / blue), team 3 with
      `use_3teams`, Spectate (`team none`), "Action's join menu" (`inven`,
      auto-join / MOTD / votes, only while on no team). Action makes a
      player wait 5 s between team changes (its message shows in the cell).
    - **Settings:** Name, Gun hand, Controls >, P1 only: Crosshair (all
      players; `crosshair` 0-20, the HUD's crosshair is one shared cvar),
      Crosshair size (`ch_scale`), AQtion menu size (`cl_splitMenuSize`); see Amendments below.
      **Controls** (Q3 Controls rows our pad layer has; `SM_Var` shadows):
      look speed H / V, invert, aim curve (+ custom exponent, dim unless
      Custom), turn boost + ramp, look speed zoomed, dead zone, full speed
      at tilt (outer dead zone), trigger press point, Buttons >, Reset
      controls (each `joy_*` to its default). No vibration or aim assist
      rows: our pad layer has neither (Q3 has both). **Buttons:** one row
      per pad button (A ... Paddle 4; Start fixed = this menu), left / right
      cycles the AQ2 action list (`smBindActions`: fire, jump, crouch,
      reload, bandage, zoom (cycle) / zoom in / out / reset, weapon mode, next / previous /
      last weapon, knife, pistol, grenade, drop item / weapon, IR, flashlight,
      use, walk, scoreboard, Action menu, this menu, radio menu
      (`padmenu radio`), 8 radio calls, center view) through
      `PAD_MenuSetBind` (= `pbind`; a held key is let go first); an empty
      table (no pad yet) gets the defaults first; Reset buttons = the
      default set. Profiles save the table as before. The Q3 fork has no
      preset layouts, so none here.
    - **Zoom cycle (0.0.0.31, the maintainer: at full zoom, zoom again
      should reset):** TNG's `Cmd_Lens_f` (`src/action/a_cmds.c`, the same in
      Steam's 2.82) already does it: `lens` with no argument (or one it does
      not know) is `_ZoomIn(ent, true)`, 1x -> 2x -> 4x -> 6x -> 1x (fov 90 /
      45 / 20 / 10); `lens in` / `lens out` stop at 6x / 1x; `lens 1|2|4|6`
      sets a level; anything but the sniper in hand: nothing. So no client
      code: the default LT is `lens` ("Zoom (cycle)" on the Buttons page,
      "Zoom in" stays there for the step that stops). Profiles carry a
      `binds "2"` line (bind-set revision, Prof_Build); one without it whose
      LT is exactly `lens in` gets `lens` once at load and is saved
      (Prof_Apply). Player 1's own pad binds in `q2config.cfg` (only written
      when changed from the defaults) are not upgraded.
    - **Hold-repeat on the overlay (0.0.0.31):** a d-pad or left-stick
      direction held on a player's overlay page or on-screen keyboard steps
      again after 400 ms, then every 120 ms, every 60 ms after 1.5 s held
      (`PAD_REPEAT_*`, in_gamepad.c `Pad_OverlayRepeat` from `PAD_Frame`; a
      tap moves once). It stops on the release, when the overlay closes or
      shows another page / the keyboard (`CL_SplitMenuView`), when the press
      is taken from the menu layer, and on any other press. Pressing the same
      direction on the other control while one is held adds no step, and
      that control carries the repeat on if the first is let go. Only
      the overlay: not binds, the engine (AQtion) menus (keyboard arrows
      there repeat through Windows) or Action's layouts.
    - **Profile wording:** "Profile..." for a guest, "Profile: <name>" with
      one, also on the main-menu page; the page says "Playing as: Guest"
      (P1: its guest settings stay in the config) and offers "Use guest".
    - **Splitscreen page** (main menu) = layout / join rows + Game type
      (`cl_splitGameType`, archived, default Teamplay) + Map + Server
      options > + Start multiplayer game; in game the Splitscreen settings
      page has only the layout / join rows (map and game type are in
      Server options). `splitstartgame` now applies the game type
      (teamplay / teamdm / ctf / use_3teams / use_tourney / dom) and bots
      (`am 2` + `am_botcount` + `ltk_loadbots 1`, or `am 0`), maxclients >=
      8 + bots, and uses `map <m> force`.
    - **Server options** (Q3 §18, P1 only; `SM_BuildServer`): every row is a
      cvar of Steam's TNG 2.82 DLL (cvarlist on that DLL:
      `work/menus-steam-cvarlist.txt`), created with the DLL's default and
      archived when the page first shows it (`SM_GameVar`), so the choices
      stay for the next session. Hosting: Restart map (`map <current>
      force`, "changes waiting" when latched cvars are pending), Change map
      (list of installed maps, loads at once with `map <m> force`). Not in
      a game: Map (list) and every row for the next game. Rows: Game type,
      Time limit, Frag limit (not in round games / CTF), Round limit and
      Round time limit (round games), Capture limit (CTF), Bots (0-20,
      next map: Steam's attract mode adds them at map start; live `sv
      addbots` / `removebot` did not give a stable count with `am 2`, see
      `work/menus-bots.log`), Bot skill (`ltk_skill`), Bot chat
      (`ltk_chat`), Joke sounds (`bot_randvoice`, only with a game DLL that
      has it: ours, not Steam's; replaced in 0.0.0.27 by Humor sounds, see
      its as-built note), Friendly fire / Falling damage (dmflags
      256 / 8, inverted), Weapon choice in deathmatch (`dm_choose`, latched),
      Spawn protection (see Amendments), All weapons / All
      items, Weapons / Items carried (`weapons` / `items`, latched), Weapon
      bans / Item bans (pages of `wp_flags` / `itm_flags` bits, live), Team
      grenades (`tgren`), Spectating dead players (`limchasecam`, latched),
      Darkmatch (latched), Item / Weapon respawn (not in round games),
      Announcements (`use_rewards`), Time / frag warnings (`use_warnings`),
      Reset to defaults (the DLL's defaults; dmflags: only the two bits).
      Rows are offered by the chosen game type; a row's footer line says
      "Now" or "Next map (Restart map applies it)". On another machine's
      server the page is not reachable (disabled row); if shown, all rows
      are dim. Espionage is not offered: Steam's DLL has no `esp`.
    - **Amendments (the maintainer, same round):** Settings has "AQtion
      menu size" (`cl_splitMenuSize`) and "Splitscreen menu size"
      (`cl_splitMenuScale`, archived, 0.75-2x, our overlay in every view:
      the fit-to-320-lines factor times it, never under 200 lines), per
      player **HUD opacity** (`p<N>_scr_alpha`) and **Crosshair colour**
      (`ch_colour` -> `p<N>_ch_red/green/blue`): `screen.c` now reads
      `scr_alpha` and the crosshair colour through `SCR_PlayerVar` (the
      cell's player's `p<N>_` cvar if it exists; health colouring stays
      shared); both are in profiles. No aim assist, no swap sticks.
      Server options: Bots 0-20, **Next map** (the installed map after the
      current one, alphabetical), **Saved settings** (save via the on-screen
      keyboard / load / delete: `<homedir>/action/splitsrv/<name>.cfg`,
      `set <cvar> "<value>"` lines of the page's cvars, dmflags, game type,
      bots; loading only sets those names, never executes the file;
      `cl_splitSrvSet` names the set in use), **Weapons** preset row
      (`wp_flags`: All 511 / Knife only 128 / Sniper only 32 / "Custom" for
      any other mix from Weapon bans; replaced in 0.0.0.30 by one preset per
      weapon that also gives the weapon at spawn, see its as-built note), **Infinite ammo** (dmflags 8192: in
      Action that means pickups give 1000 rounds and grenades are never
      used up; magazines still reload), **Respawn delay** (team DM
      `teamdm_respawn`, CTF `ctf_respawn`; deathmatch: "Respawn by itself" =
      dmflags 1024), **Spawn protection** (`uvtime`, tenths of a second;
      also sets `dm_shield` for deathmatch). Dropped: "bandaging required"
      (Steam's DLL has no such cvar or flag).
    - **Main menu (still no native entries: the maintainer withdrew that
      request the same round):** **Quick Local Match** is the first row of
      our Splitscreen page at the main menu (`splitquickmatch`: the archived
      server options on a random installed map other than `beer`, via
      `splitstartgame`). **F1** at the main menu opens the Splitscreen page
      (Start's keyboard key; `splitmenu` does the same from the console).
      The keyboard and mouse now drive P1's
      overlay (`keys.c` -> `CL_SplitMenuKeyEvent`: arrows / wheel, Enter /
      left click, Escape / right click / Backspace back, Tab closes; on the
      on-screen keyboard typed characters go in). A hint "Press Start on a
      pad (or F1) for Splitscreen" (drawn by us, not a menu item) sits at the bottom of the main menu while a pad
      is connected.
    - **Start at launch (bug, 0.0.0.23):** Start opened the page only from
      the pad that was player 1's. With several pads (the maintainer has
      three) the first button or stick on *any* pad claims P1 at the main
      menu; Start on another pad then fell into "unjoined press", which only
      joins with A, and did nothing. Now Start on any pad out of a game makes
      that pad P1's (the other steps out) and opens the page
      (`Pad_KeyDown`, `CL_SplitMenuCanOpenHost`).
    - **Rows** got a footer help line (`SM_Help`), a disabled state, bit
      toggles (`SMR_FLAG`) and pad-bind rows (`SMR_BIND`); `SM_MAX_ROWS` is
      128 (the map list scrolls), `SM_MAX_DEPTH` 5.
    - **Joke sounds** (farts, laughs, "newbie"): `src/action` `Add_Frag`
      plays one of 85 `user/*.wav` (CHAN_VOICE) when a bot frags, with
      probability `bot_randvoice` % (default 5), gated by `use_voice`. Only
      our newer game DLL has that code; Steam's TNG 2.82 DLL contains none
      of those sound names and no `bot_randvoice`, so the release (which
      hosts with Steam's DLL) never plays them; they were heard with our DLL
      (0.0.0.19 shipped it).
- Console: one, P1's (typed commands run as context 0 unless `p<N>`).
  With 2+ cells its notify lines draw in P1's cell (0.0.0.17, §3.4).
  Server prints arrive once per connection: P1's go to console + chat HUD
  as today (`CL_ParsePrint`, `parse.c:1145-1208`); extras' go only to their
  own chat HUD / centerprint, and a line another context already printed
  this frame is not repeated — no N× console spam, no N× `talk.wav`.

  - *As built (0.0.0.25, the maintainer's pad pass of 0.0.0.24):*
    - **Profile row:** always "Profile: Guest" / "Profile: <name>" (pause
      page and the main-menu Splitscreen page), never "Profile...".
    - **Per-player look (Settings, every player):** Crosshair (`crosshair`),
      Crosshair size (`ch_size`, ours, 1x-4x, default 3x, times upstream's
      `ch_scale` picture size, so the scope, which follows `ch_scale`, is
      unchanged), Crosshair colour, **Text size** (`cl_splitTextSize`,
      default 2x), HUD opacity, **AQtion menu size** (`cl_splitMenuSize`),
      **Splitscreen menu size** (`cl_splitMenuScale`). Player 1 uses the
      shared cvars (archived); players 2-8 their `p<N>_` shadows (created
      from player 1's value, kept in profiles: `prof_feel`, guests reset to
      player 1's). screen.c: `SCR_CrosshairPic` / `SCR_CrosshairSize` per
      cell (pictures registered once per player and number, cache cleared
      with the media); `SCR_TextScale` draws the chat HUD and centre prints
      of a cell (and player 1's notify lines, `SCR_SplitNotifyCell`) at the
      HUD scale divided by the text size, as long as the cell keeps 416
      virtual pixels (52 columns, Action's MOTD); one view (no cells) is
      upstream.
    - **AQtion menu in cells (bug):** ui.c called `CL_SplitUIScale` without a
      prototype (`inc/client/client.h` is the public header, not
      `src/client/client.h`), so MSVC took the float as an int: the base
      scale in a cell was garbage and the menu came out at a random, usually
      1x, size once the screen split. The split UI functions are now declared
      in `inc/client/client.h`. The size is now relative to the window:
      `cl_splitMenuSize` times the window's own scale, capped only by what
      fits the cell (a menu of items needs its own height, not the 240-line
      floor, which stays for self-sizing menus; width floor 400), never
      below 1 screen pixel per virtual pixel. Measured at 1080p: AQtion's
      game menu at size 2 is 4x full screen, 3.07x in a 1920x540 cell, 2.4x
      in a 960x540 cell.
    - **Engine UI owner:** the player whose context opens the engine UI owns
      it (`CL_SplitUIOpened` from `UI_PushMenu` on an empty stack): it is
      drawn in that player's cell at that player's AQtion menu size and that
      player's pad drives it (`in_gamepad.c`, `Pad_ExtraInUI`); player 1 in
      practice (keyboard, Escape, Start, Game menu row). Players 2-8 reach it
      only through `p<N> pushmenu game` (no row: AQtion's game menu has
      Disconnect / Play Now, which end everyone's game).
    - **Infinite ammo:** Steam's TNG 2.82 has no switch that keeps a
      magazine from running dry. dmflags 8192 (0.0.0.24's row) only makes
      weapon pickups give 1000 rounds and removes the map's ammo boxes;
      `allweapon` / `allitem` / `item_kit_mode` only change what you spawn
      with. The game's own `give ammo` (full magazine for every gun, clips to
      the carry limit) needs `cheats 1`. The row now sets
      `cl_splitInfiniteAmmo` (archived, in saved sets) and `cheats` to match
      (latched: next map / Restart map), clears a leftover 8192 bit, and
      while both are on every live local player (`PM_NORMAL`) sends
      `give ammo` every 500 ms (`CL_SplitMenuFrame`). Bots and remote
      players get nothing extra. Verified: 60 presses of fire with two
      players, 12 / 2 left each; off: 0 / 0 and "Out of ammo".
    - **Test windows:** a `vid_noactivate` window may be larger than the
      desktop (`WM_GETMINMAXINFO`), so 1080p / 4K windows render on a
      smaller or disconnected display.
  - *As built (0.0.0.27, humor sounds):*
    - **Correction of 0.0.0.23** (the "Joke sounds" note above): Steam's TNG
      2.82 DLL does play AQtion's joke clips. The string scan looked only for
      `user/*.wav` names and `bot_randvoice`; the clips are also in
      `sound/insane/`: AQtion's pak0.pkz replaced Quake 2's
      `insane1-11.wav` (2022) with them (insane2 = `user/killme.wav`, 3 =
      `lol`, 4 = `muu1`, 5 = `newbie`, 6 = `shootorwhat`, 7 = `fart4`, 1 and
      8-11 own clips). Both DLLs (`ClientBeginServerFrame`, src/action
      `p_client.c`) play `insane/insane<1-11>.wav` on CHAN_VOICE,
      ATTN_NORM, of every living player whose `resp.idletime` (no input)
      reaches a multiple of `ppl_idletime` seconds (default 15): an idle
      split-screen player sets one off every 15 s. `user/*.wav` (78 clips, 77 in
      `baseaq\sndlist.ini`) come from the `voice <file>` command (both
      DLLs, `use_voice`) and, in our src/action only, from `Add_Frag` with
      `bot_randvoice`. Evidence: `work/humor27-*`.
    - **Row "Humor sounds"** (`cl_splitHumorSounds`, archived, in
      `smSrvDefaults` so saved sets keep it; shown with every DLL, live).
      Client side (split_snd.c `CL_SplitSoundAllowed`, hooked in parse.c
      `svc_sound` and the `play` command): with 0 no local player starts a
      `user/` or `insane/` sound, with any game library or server. Game side
      (split_menu.c `SM_ApplyHumor`, on the row's change, game start,
      Restart / Next / Change map, reset and set load; not on another
      machine's server): `ppl_idletime` and `bot_randvoice` 0 (created with
      0 when the game has not registered them yet; the game keeps a value
      it finds), and back to 15 / 5 where 0 when the row is on. Both are read
      live by the game. `sv_showsounds 1` (2 = every sound) logs the game's
      humor sounds with the entity that made them (server/game.c).
  - *As built (0.0.0.28, infinite ammo live in every path):*
    - **Why 0.0.0.25-27 failed for the maintainer:** `cheats` is
      `CVAR_LATCH` (`Cvar_SetByVar`, `src/common/cvar.c`: any set other
      than `FROM_CODE` while `sv_running` only fills `latched_string`, and
      `SV_InitGame` -> `Cvar_GetLatchedVars` applies it at the next full
      map load). The row wrote it `FROM_CONSOLE`, so in a running game the
      live value stayed 0 ("cheats will be changed for next game"), the
      sender (gated on the live value) sent nothing and a hand `give ammo`
      got "You must run the server with '+set cheats 1'" (repro
      `work/inf28-repro.log`, 0.0.0.27 build). A game started any way other
      than `splitstartgame` / Quick Local Match (AQtion's own menus, console
      `map`) never ran `SM_ApplyInfAmmo` at all: `cheats 0`, no refill
      (`work/inf28-repro2.log`). Quick Local Match with the row saved On
      worked (the set happens before the server runs). The game DLL holds
      the engine's own `cvar_t` (`PF_cvar` -> `Cvar_Get`; the legacy API 4
      struct is the same), and Steam's TNG 2.82 `ClientCommand` checks
      `sv_cheats->value` for `give`, so the live value is all that matters.
    - **Live:** `SM_SetCheatsNow` writes `cheats` with `FROM_CODE` (no latch,
      any pending latched value dropped; `Cvar_FixCheats` after a 0), used by
      the row, start, Restart / Next map, reset and set load. While the row
      is on and we host, `CL_SplitMenuFrame` puts `cheats 1` back whenever
      the live value is 0 or a value is latched, whichever way the game was
      started, so it holds from the first frame of the game.
    - **Refill:** per local player, each frame: alive (`PM_NORMAL`) with a
      weapon model (`gunindex`; teamplay spectators before joining a team are
      `PM_NORMAL` without one, and `give` answers "This command can't be used
      by spectators"); `STAT_AMMO` (rounds in the gun / knives / grenades)
      or `STAT_CLIP` (magazines) lower than last frame with the same gun ->
      a refill, at most one per 100 ms per player (missed decrements are
      counted); plus `give ammo` every second for the guns not in hand.
      What is in hand comes from the `STAT_AMMO_ICON` image name: `w_knife`
      -> `give Combat Knife`, `a_m61frag` -> `give M26 Fragmentation
      Grenade` (both are weapons in Action, one per give through the pickup
      code, capped by the carry limit; `give ammo` only fills the guns'
      `*_rds` and the `IT_AMMO` clips), anything else -> `give ammo`.
    - **Sent with `CL_ClientCommand` in the player's context,** not through
      the command buffer: a pending `wait` (a cfg, an alias with waits)
      stalls `Cbuf_Execute`, and in the first test every refill sat there
      until the alias finished.
    - **Help:** hosting "Now: guns never run dry; cheats on", otherwise
      "Guns never run dry (turns cheats on)" (the footer shows about 40
      characters). `developer 1` logs each refill: `infinite ammo: p<N> give
      ... (ammo A, clip C, cheats 1[, used])`.
    - **Evidence** (Steam DLL, offscreen, `work/inf28-*.log`, screenshots
      `work/inf28*-*.jpg`): every default weapon held on full auto 200 frames
      twice and reloaded: MK23 never below 11/12, dual 22/24, MP5 28/30, M4
      22/24, M3 6/7, sniper 5/6, handcannon 0-2 (both barrels per shot, back
      to 2), thrown knives 9 -> 10, grenades 1 -> 2 per throw.
  - *As built (0.0.0.29, infinite reserve, reloads stay):*
    - **What the maintainer asked for after 0.0.0.28:** players still
      reload; only the spare magazines / shells are infinite. 0.0.0.28's
      `give ammo` after every shot set every gun's `*_rds` to its maximum, so
      the gun never emptied.
    - **Lever (Steam's TNG 2.82, same code as src/action `Cmd_Give_f`):**
      `give <pickup name>` of an `IT_AMMO` item does `inventory[index] +=
      it->quantity` (uncapped, no count argument; the count variant is
      commented out) and leaves `*_rds` alone. Pickup names from the DLL's
      item table: `Pistol Magazine` (1; MK23 and dual pistols), `MP5
      Magazine` (1), `M4 Magazine` (1), `12 Gauge Shells` (7; M3 and
      handcannon), `AP Sniper Ammo` (10). The HUD's `STAT_CLIP` is
      `inventory[ammo_index]` (the reserve), `STAT_CLIP_ICON` its icon
      (`a_clip`, `a_mag`, `a_m4`, `a_shells`, `a_bullets`), `STAT_AMMO` the
      rounds in the gun. Verified at runtime (REA's Ghidra bridge failed its
      handshake on this box, so not in the decompiler): the reserve rises by
      the quantity, `STAT_AMMO` does not change (`work/inf29-c.log`: row on
      with a dry pistol, `loaded 0 reserve 0` -> `loaded 0 reserve 1`).
      Weapon pickups (`give MP5/10 Submachinegun`) refill `*_rds`, `give
      ammo` refills every gun, dmflags 8192 only affects weapon pickups:
      none of them leaves the loaded rounds alone.
    - **Carry limit:** the client cannot see `max_pistolmags` etc. (they
      depend on game mode and bandolier). `give ammo` caps the clips at the
      limit (`Add_Ammo`) and at spawn every gun is loaded anyway, so it is
      sent once per life (a player seen dead / not in play / before the game
      ran, then alive with a gun); the reserve it leaves is "full" for that
      kind (per player, the highest reserve seen before our own first
      top-up of that kind; frozen afterwards so the overshoot below never
      raises it). Switched on mid-life: "full" is the reserve at that moment
      (at least 1, so a dry gun gets one magazine / 7 shells / 10 rounds at
      once) until the next spawn.
    - **Sender (`CL_SplitMenuFrame`):** with a gun in hand whose clip icon is
      known and `STAT_CLIP` below full: `give <item>` ceil(missing /
      quantity) times at once (the dual pistols' reload takes two
      magazines), then nothing more for that player until the reserve has
      risen or 500 ms passed. Shells and sniper rounds come 7 / 10 at a time,
      so their reserve can sit up to 6 / 9 above the limit (14 -> 13 -> 20).
      The sidearm's magazines are the MK23 kind, only used while it is in
      hand, so the same rule covers it. Knives / grenades: unchanged from
      0.0.0.28 (one `give` per one thrown). No periodic `give ammo` any more.
      Cheats handling and the live apply: unchanged from 0.0.0.28.
    - **Help:** hosting "Now: endless spares, reloads; cheats on",
      otherwise "Endless spare mags (turns cheats on)" (39 / 36 characters:
      the footer shows about 40, the longer sentence was cut off).
      `developer 1` logs every change of a player's counts (`infinite ammo:
      p<N> loaded A reserve R (<item>) t=<ms>`) and each top-up.
    - **Evidence** (Steam DLL, offscreen, two players, `work/inf29-*`,
      tables from `work/inf29-analyze.py`): every default weapon held on
      fire until empty and reloaded: the loaded count reaches 0 and stays
      there while fire is held, reload brings it back to the full magazine
      (M3 one shell per reload, sniper one round, handcannon two), the
      reserve is back to full within one round trip (at most 103 ms below
      full minus one: dual pistols, handcannon).
  - *As built (0.0.0.30, Weapons presets: one per weapon, they give it):*
    - **The maintainer's report on 0.0.0.29:** "Sniper only" (Steam's DLL,
      root placement) gave every player a pistol only, with no ammo.
    - **Bits (Steam's TNG 2.82, checked with its own `gamesettings`
      printout per value, `work/wpn30-a.log` / `wpn30-b.log`; the same as
      src/action `items[].flag`):** MK23 1, MP5 2, M4 4, M3 8, handcannon 16,
      sniper 32, dual MK23 ("Akimbo Pistols") 64, knife 128, grenades 256.
      Ammo items take their weapons' flags: pistol magazines MK23|dual,
      shells M3|handcannon, the rest their gun's bit (`itm_flags` is the
      six items, not ammo; the presets never touched it).
    - **Root cause:** `PutClientInServer` always puts the MK23 in the
      inventory and selects it (the knife only if MK23 is banned and the
      knife allowed); it sets the pistol's rounds only when MK23 is allowed.
      Then deathmatch without weapon choice (`EquipClientDM`) adds the
      weapon named by the `dmweapon` cvar (default "MK23 Pistol" -> nothing
      added), with no `wp_flags` check; teamplay / `dm_choose 1`
      (`EquipClient`) gives the player's chosen weapon, defaulting to MP5,
      else MK23, else knife, and drops a banned choice. With `wp_flags 32`
      the MK23 bit, and so the pistol magazines' bit, was off: an empty
      pistol and no magazines, and in deathmatch no sniper because
      `dmweapon` still said MK23 (`work/wpn30-a01..03`: 0 / 0). Weapon bans
      only remove map pickups and menu entries. No "only one weapon ->
      give it" path exists in the DLL.
    - **Presets (`smWeapSetValues`):** All 511, MK23 only 1, Dual pistols
      only 65, MP5 only 3, M4 only 5, M3 only 9, Handcannon only 17, Sniper
      only 33, Knife only 128; any other value "Custom". A gun preset keeps
      the MK23 bit (the sidearm every player carries; its rounds and
      magazines work), bans the knife and grenades so the teamplay weapon
      menu lists only that gun. Dual pistols are a weapon of their own in
      the DLL (menu entry, `dmweapon` / `choose` name "Dual MK23 Pistols")
      but need the MK23 bit (pickups, magazines), hence 65.
    - **Levers (`SM_WeapPresetFrame`, every client frame, not on another
      machine's server):** `SM_StartWeapon(wp_flags)` = the only allowed gun
      of MP5 / M4 / M3 / handcannon / sniper / dual; none: "MK23 Pistol" if
      allowed, else "Combat Knife"; several: the game's own default if the
      MK23 is allowed, else the first allowed gun; All: none. (1)
      deathmatch: `dmweapon` = that name (read at every spawn, also by
      bots; loaded + the usual spares: sniper 6 / 10, M3 7 / 7, handcannon
      2 / 12, MP5 30 / 1, M4 24 / 1, dual 24 / 2, knives 10), back to "MK23
      Pistol" when the row returns to All and we had changed it. (2)
      teamplay and `dm_choose 1`: `choose <name>` on each local player's
      own connection once per map / preset, after the preset has been
      stable 1 s (cycling the row sends nothing in between); the MK23 is
      not a choice (the game's default gives it); the game ignores `choose`
      where there is no weapon choice. Back to All: players we chose for
      get `choose MP5/10 Submachinegun` (the game's default) so a knife
      round does not stick. Bots in teamplay pick an allowed weapon by
      themselves (ACE `wp_flags` checks; not observed here). Live: the
      next spawn / round, no map restart. Weapon bans and the row share
      `wp_flags`, so a ban edit shows "Custom" and still gets a start
      weapon by the same rule (a set saved with 0.0.0.29's 32 spawns the
      sniper). Help: hosting "Now: at the next spawn", otherwise "Everyone
      spawns with it". `developer 1` logs every change of a player's gun /
      loaded / reserve (`weapon preset: p<N> gun <model> loaded A reserve R`).
    - **Not possible with Steam's DLL:** a pistol-less gun preset without
      an empty MK23 in the inventory (the spawn code always adds it; with
      the bit off it is just empty, as in Knife only), and spare pistol
      magazines in deathmatch MK23 only (the DLL gives none: 12 / 0 is its
      normal deathmatch spawn; teamplay 12 / 1).
    - **Evidence** (Steam DLL, offscreen, two players, `work/wpn30-*`,
      tables `work/wpn30-{b,c,d,e,g}-table.txt` from
      `work/wpn30-analyze.py`): every preset in deathmatch (b) and teamplay
      (c), the row and the bans page driven by virtual pad 1 (d), Knife
      only -> All in teamplay (e), the root-placed exe with Quick Local
      Match (g).

### 7.2 Join hint

Q3 §14.1 (activity-based, `cl_splitJoinHint 1/2/0`), drawn after the
cells, before the UI.

### 7.3 Connect, precache, map change

- P1 connects as upstream. Extras join once P1 is `ca_active` (Q3 §8).
- **Extras skip downloads and registration.** `CL_Precache_f`
  (`main.c:1758-1789`) for i>0 does not run `CL_RequestNextDownload` /
  the download state (`download.c:730-905`); instead it copies the
  registered handles from context 0 (`model_draw`, `model_clip`,
  `image_precache`, `sound_precache`, `clientinfo`, `weaponModels` —
  identical configstrings), loads `cl.bsp` with `BSP_Load` (shared,
  ref-counted), and sends `begin <spawncount>` like `CL_Begin`
  (`main.c:1715-1747`) minus `CL_PrepRefresh` / `CL_RegisterSounds` /
  `LOC_LoadLocations` / `Cvar_FixCheats` / `OGG_Play`. Later per-context
  `CL_UpdateConfigstring` registrations (a new skin mid-game) are fine:
  `R_RegisterModel` / `S_RegisterSound` outside a Begin/End pair just add.
- `CL_ParseServerData` sets `fs_game` only when no local server runs
  (`parse.c:605-612`); for i>0 never touch `fs_game`.
- **Map change** arrives as stufftext `changing` → `reconnect` per
  connection (`SV_BroadcastCommand`, `server/init.c:123,223`;
  `exec_server_string` `main.c:2563-2604`; `CL_Changing_f` `main.c:1011`,
  `CL_Reconnect_f` `main.c:1060`). With a per-context stufftext buffer
  each player handles its own. Rule as Q3 §8: **only context 0 loads the
  level** (`CL_PrepRefresh`, sounds); an extra that reaches `precache`
  before P1 finished loading waits in `ca_connected` and takes the copy
  path once P1 is `ca_precached`.
- Server restart (`SV_Shutdown(…, ERR_RECONNECT)` → `svc_reconnect` to
  every client, `server/main.c:2918-2950`; client `CL_ParseReconnect`
  `parse.c:1068-1087`): per context; extras redo their challenge,
  serialized.

### 7.4 Disconnect and errors

- `CL_Disconnect` (`main.c:735-790`) and `CL_ClearState`
  (`main.c:689-726`) for **i>0**: send `disconnect` on its own netchan,
  clear its `cl` / entities / effects / screen state only, and skip the
  shared side effects (`SCR_EndLoadingPlaque`, `SCR_ClearChatHUD_f`,
  `cl_disconnectcmd`, `CL_CleanupDemos/Downloads`, `CL_GTV_Suspend`,
  `UI_OpenMenu`, `S_StopAllSounds`, `OGG_Stop`, `SCR_StopCinematic`,
  `LOC_FreeLocations`, `fs_game` / `gl_*` cvar flags, `CL_CheckForPause`,
  `CL_UpdateFrameTimes`); free its socket; recompute the layout. **P1
  disconnect** = disconnect all extras first, then the stock path.
- **Errors.** `Com_Error(ERR_DROP | ERR_DISCONNECT)` shuts down the server
  *and* the client (`common.c:560-583`), then `longjmp`s to the frame.
  Errors raised while an extra's packet, stufftext or timeout is being
  processed (`parse.c` has ~25 `Com_Error(ERR_DROP…)` sites;
  `CL_CheckTimeout` `main.c:3101-3125`; `CL_SendReliable`
  `input.c:1113-1128`) must drop only that player. Design: the per-
  context work for i>0 runs under a `setjmp` guard; a ~6-line hook at the
  top of `Com_Error`'s DROP/DISCONNECT branch calls
  `CL_SplitTrapError(code, msg)`, which — when a guard is armed — clears
  `com_errorEntered`, `MSG_Init()`s and `longjmp`s back to the guard,
  where `CL_SplitDrop(i, msg)` runs. One hook instead of Q3's per-site
  conversions; P1 errors keep the stock behaviour.
- **Demos:** record/playback P1 only; `cl_autorecord` P1; joins are
  refused while a demo plays. GTV client and Discord: P1 only.
- **Cinematics:** single-player only (`ca_cinematic`); no joins.
- **Pause:** `sv_paused` is global; single-player pause does not engage
  with N>1 clients — expected.
- *As built (M1-T2+T3, 0.0.0.5):* the guard is `CL_SplitRunGuarded`
  (`setjmp`) around each extra's events, frame and `p<N>` command; the
  `Com_Error` hook (7 lines in `common.c`, before the DROP/DISCONNECT
  branches) clears `com_errorEntered`, calls `CL_SplitTrapError` (which
  `longjmp`s to the armed guard) and sets it again. Unguarded errors first
  pop every pushed context so the stock path always disconnects player 1.
  Drops are deferred to a safe point (`dropPending`) when the player is on
  the context stack. Player 1's `CL_Disconnect` drops every extra unless
  the type is `ERR_RECONNECT` (svc_reconnect / `reconnect`: the extras
  follow on their own connection); in addition, each frame an extra that
  is no longer on player 1's server (player 1 disconnected, playing a demo,
  connected elsewhere, or the local server's port changed) is dropped.
  Extras' sounds (`svc_sound`, entity events, temp entities) still play
  through the shared sound system (duplicates; T6).
- **Timing:** one clock. `cl_async 1` (default, `main.c:2774`) runs
  physics at `cl_maxfps` 62 (`main.c:2772`) and refresh at `r_maxfps`;
  every context builds and sends its cmds in the same physics tick.
  `cl_maxpackets` and `rate` apply per connection through the same cvars.

---

## 8. Mod and server compatibility risks (AQ2-specific)

Works by construction: everything Action does through the server game API
(statusbar, layouts, `PMenu` team/weapon/item menus, GHUD elements,
centerprints, cvar sync, stufftext probes), because every extra is an
ordinary client with its own connection.

| Risk | Mitigation |
|---|---|
| Steam's game DLL (TNG 2.82, built against the 2023 engine, API 4) vs this engine (v1.3.8 removed `game_import_t.CheckForExtension` and `game_export_t.FetchGameExtension`) | *As built 0.0.0.20:* `SV_InitGameProgs` (`src/server/game.c`): a library without `GetGameAPIEx` gets the import with `CheckForExtension` appended (45th pointer); if it reports API 4, its exports are read through `game_export_aq4_t` and `ge` points to a copy whose 15 functions wrap the DLL's and refresh `edicts` / `edict_size` / `num_edicts` / `max_edicts` after each call; `customizeentityforclient` / `CvarSync_Updated` come from its `FetchGameExtension`. DLLs with `GetGameAPIEx` (ours) get exactly what they got before. Verified: listen server, 4 local players, teams, `gamemap`, bots; q2proded with it too |
| `sv_iplimit` 3 on remote servers (`server/main.c:2858`) | refusal shown in the cell; our listen server exempts 127/8 (§4.3) |
| Per-IP challenge slot (`SVC_GetChallenge`) | serialized joins |
| 8-bit qport demux | qports distinct mod 256 |
| AQtion cvar sync (`cl_xerp`, `cl_antilag`, `cl_indicators`, `cl_spectatorhud`, `cl_spectatorkillfeed`) answered from shared cvars; `force_cl_xerp` server-side (`src/action/g_ext.c:255-272`) | per-player shadows via the `playercvar` list (§5.2) |
| `steamid` / `discordid` duplicated in userinfo (stats, Discord) | blank for extras |
| Ghost stats / LRCON claim / silence-ban keyed on ip + name | distinct names (Guest = "Player N") |
| r1ch anticheat-required servers | extras refused; document |
| Action opens its join menu (`PMenu_Open`) for each client on connect; navigation with `invnext` / `invprev` / `invuse` | per player by construction; pad menu layer (§5.2). *As built:* in Action `menu` is the vote menu; the team/join menu is `inven` (TAB), so the overlay's "Team & weapons" (§7.1) should send `p<N> inven` |
| Matchmode captain / ready / forfeit commands | per player through `p<N>` and pad binds |
| Bots (`sv addbot`, `addbots`) | server-side; used for headless soak tests |
| `%cpsi` / `%!fc` and other server stufftext probes | per-context stufftext buffer answers per connection |
| Server menus / scoreboards wider than a small cell | per-cell HUD scale + scissor (§3.2); measured in Spike C |
| Autoreply `!version` (`USE_AUTOREPLY`) | P1 only |
| Duplicate console prints / chat sounds | dedupe (§6, §7.1) |
| Stufftext from an extra's server changing player 1's settings | *as built (M3):* `CL_SplitStufftext` (split.c), one rule called by `exec_server_string` for i>0 after the per-connection commands: forwarded / unknown commands (`%cpsi`, Action's), `cmd`, `wait`, `play`, `echo`, `set`/bare writes of this player's userinfo / cvar-sync / `p<N>_` shadows, new cvars this server created (sv_force_reconnect's junk names; engine-style prefixes excluded), `connect` (= this player's reconnect, same address only), `disconnect` (= this player's drop) run; everything else (binds, aliases, `exec <file>`, `quit`, existing cvars) is ignored with `split: P<N> ignored stufftext "..."` (once per second per player). `split_stufftest <N> <text>` (`set developer 1`; the release build has no `developer` command) feeds the path |

---

## 9. Diff footprint

Rule (as in the Q3 charter): new code in new files, thin hooks upstream.

**New files** (`src/client/split/`), ≈ 5–7 k lines by M2 (reference: Q3's
`cl_splitscreen.c` 2956 + `in_gamepad.c` 4115 lines, plus its menu and
profile files):

| File | Content |
|---|---|
| `split.c` | contexts, `CL_SetContext/Push/Pop`, `p<N>`, `addplayer` / `dropplayer`, join/drop, serialized connect, refusal text, error guard, frame-loop helper |
| `split_view.c` | layout table, cells, per-cell HUD scale, overlays, borders, join hint |
| `pad.c` (+ `pad_sdl.c` backend) | device layer, binds, analog, aim feel, hold-to-join, menu layer |
| `split_menu.c` | per-player overlay menu, host settings page, on-screen keyboard |
| `split_profile.c` | profiles, guest defaults |
| `split_snd.c` (or inside `split.c`) | listener array, dedupe helpers |

**Thin hooks in upstream files** (≈ 600–800 changed lines):

| File | Change | ~Lines |
|---|---|---|
| `src/client/client.h` | context struct, macros, anonymous `client_conn_t` slice | 80 |
| `src/client/main.c` | definitions, `CL_Frame` / `CL_ProcessEvents` loops, connect path (`cla->sock`, qport, userinfo overlay), OOB print refusal, `HTTP_SetServer` / AC guards, `CL_Disconnect` / `CL_ClearState` / `CL_Precache_f` / `CL_Begin` i>0 branches | 150 |
| `src/client/input.c` | kbutton macros, KBM-only mouse, `Cbuf_Frame` split, pad move hook, userinfo overlay | 60 |
| `src/client/parse.c` | `CL_Z`, stufftext buffer, print routing, `fs_game` guard, cvar-sync shadow | 40 |
| `src/client/screen.c` | per-player state macros, cell geometry, crosshair `r_config` → hud, `SCR_UpdateScreen` loop, transparent-menu rule | 80 |
| `src/client/view.c` | per-context scene arrays, refdef origin | 15 |
| `src/client/effects.c`, `tent.c` | file-local macros | 30 |
| `src/client/entities.c` | listener per context | 5 |
| `src/client/keys.c` | pad bind write hook | 5 |
| `src/client/sound/main.c`, `dma.c`, `al.c`, `sound.h` | listener array, context tag, dedupe | 80 |
| `src/client/ui/ui.c` | cell-sized UI in game | 15 |
| `inc/refresh/refresh.h`, `src/refresh/draw.c`, `state.c`, `main.c` | `R_SetDrawRect`, offsets, FBO sizing | 50 |
| `inc/common/net/net.h`, `src/common/net/net.c`, `chan.c` | `NS_CLIENT2..8`, sockets, `!= NS_SERVER` qport tests | 50 |
| `src/common/common.c` | split error hook | 8 |
| `src/server/main.c` | listen server: 127/8 = local | 15 |
| `meson.build`, `meson_options.txt`, `subprojects/sdl2.wrap` | split sources, `gamepad` option | 25 |

PR-ability: the context macros + anonymous `cls` slice (behaviour-neutral
at N=1), `R_SetDrawRect`, and the SDL gamepad layer at N=1 are each
upstreamable on their own; the rest is one feature behind
`USE_SPLITSCREEN` (meson option, on in our fork).

---

## 10. Milestones and spikes

Every task: build per `docs/BUILD-AND-PROVENANCE.md` (Spike A); windowed
run (`+set vid_fullscreen 0 +set vid_geometry 1280x720`) with `+set
basedir <AQtion install> +set homedir <repo>\work\aqhome +set logfile 1`
(both path cvars are `CVAR_NOSET`, so command line only); evidence = the
engine's `screenshot` output + a `console.log` excerpt copied to `work/`
and cited in STATUS.md. Extra players without pads through `addplayer` /
`p<N>` / `padinject`, motion through `sv addbots`.

### Spikes

- **Spike A — build parity** (owned by `docs/BUILD-AND-PROVENANCE.md`):
  upstream builds on this machine and runs a map windowed. Prerequisite
  for everything below.
- **Spike B — static inventory proven by compile** (this document is the
  inventory). Apply §1.4 at N=1: `client_context_t`, the `cl` /
  `cl_entities` macros, the anonymous `client_conn_t` slice in `cls`,
  `CL_Z`, file-local macros for every per-player static of §1.3 (effects,
  tent, view scene arrays, screen per-player state, input kbuttons and
  autosens, `cl_cmdbuf`, `precache_spawncount`). Done when: it builds
  clean; `deathmatch 1; map <AQ2 map>` with `sv addbots 4` plays the same
  as upstream; a `timedemo` of a stock demo is within noise of the
  baseline; `sizeof(client_context_t)` is logged (replaces §1.5);
  file-scope globals in `src/client` are re-grepped on the branch and any
  this inventory missed are listed in the report. *Becomes M1-T1 if
  clean.*
  *As run:* done directly as M1-T1 (0.0.0.4): §1.3 "As built", sizes in
  §1.5; timedemo of the stock `AQtion-demo-guide` within noise of 0.0.0.3.
- **Spike C — two views.** On top of B: a second context with its own
  `NS_CLIENT2` socket connects to the listen server over 127.0.0.1
  (challenge, distinct qport); `R_SetDrawRect`; top/bottom cells, both
  HUDs. Done when: a screenshot shows two different POVs with two status
  bars; server `status` shows 2 humans; checked and reported: flare
  flicker, `gl_bloom 1` on/off, `scr_viewsize 80`, Action's teamplay
  scoreboard in a 640×360 and a 320×360 cell, a `map` change with both
  attached, no ERR_DROP.
  *As run (0.0.0.3):* reduced to the renderer half: `R_SetDrawRect` plus
  the same `cl` drawn into 1–4 cells (`cl_splitspike`, throwaway); the
  second context, socket and two-POV checks move into M1. Results in §3.4.
- **Spike P — SDL2 pads in the MSVC build.** WrapDB `sdl2` linked
  statically, joystick/gamecontroller/hidapi only, no SDL video; logs GUID,
  name and buttons of every device at 1 Hz. The maintainer plugs in 2+
  pads once and sends the log (the only pad-dependent spike). Fallback:
  `SDL2.dll` dynamic load; last resort: XInput.

### M1 — two keyboard-driven players on a local server (the refactor)

- T1 context plumbing (Spike B productionised) + `split.c` skeleton.
- T2 net: `NS_CLIENT2..8`, sockets, qport per context, `!= NS_SERVER`
  fixes, listen-server 127/8 rule, serialized connect, refusal handling.
- T3 frame loop, per-context stufftext, `p<N>` / `addplayer` /
  `dropplayer`, the extras' precache copy path, per-context
  `CL_Disconnect` / `CL_ClearState`, the `Com_Error` guard.
  *As run (0.0.0.5):* T2 and T3 in one round. Commands `addplayer`,
  `dropplayer <N>`, `p1`..`p8`, `splitplayers` (not `players`: that is
  Action's own server command, which the client forwards), `splitdebug
  error <N>` (raises an ERR_DROP in player N's frame to test the guard),
  cvar `cl_splitdebug 1` (echo extras' server prints as `[P<N>] ...`).
- T4 viewports: `R_SetDrawRect`, cell HUD, layouts N ≤ 4 (Q3 §3.2 table),
  overlays, transparent in-game UI in P1's cell.
- T5 per-player userinfo + cvar-sync shadows (`p2_name` visible in
  `status` and on the scoreboard; changing it renames only P2).
- T6 sound: listener = P1 + dedupe.
- Done when: `deathmatch 1; map <AQ2 map>; addplayer` → 2 humans;
  `p2 +forward` moves P2 (seen from P1's view); `p2 menu` opens Action's
  join menu in P2's cell only, and `p2 invnext` / `p2 invuse` joins a
  team; a `map` change, a server restart, `dropplayer 2` and a server
  kick of P2 all survive without ERR_DROP; screenshots at N = 2, 3, 4.

### M2 — pads, join, profiles, menus

- T1 pad layer per Spike P; T2 binds / analog / aim feel (Q3 §14.4) + an
  AQ2 default pad set (move, look, fire, reload, weapon/item, zoom,
  `inven`, radio, `menu`) + the menu layer (§5.2); T3 hold-to-join,
  leave, auto-heal, join hint (Q3 §6.2, §12.1, §14.1, §17.1); T4
  per-player overlay menu, host settings page, on-screen keyboard (§7.1,
  Q3 §12.2, §12.4, §13.4); T5 profiles + guest defaults (Q3 §12.3, §13.1,
  §14.3); T6 nearest-listener sound.
- Done (agent): `padinject` drives join, movement, the menu layer, the
  overlay and the profile picker; screenshots of each. **Maintainer
  checklist** (into STATUS.md): fresh config, P1..P4 join by holding A,
  Action's join menu with a pad, leave as P2 with four players, rejoin,
  the profile follows the person, unplug/replug.

### M3 — online

- T1 remote path for extras (serialized, blank steamid, reworded
  refusals); T2 robustness (one extra kicked, timed out; server map
  rotation; server restart). Done (agent): a **second AQtion dedicated
  server process on this machine bound to the LAN IP**, default
  `sv_iplimit 3`: P1 + 2 extras in, the 4th shows "this server allows only
  3 players from one connection" in its cell while the others play;
  `sv_iplimit 8` → 4 join; `sv_force_reconnect` set → all reconnect.
  T3 (maintainer): real public servers of his choosing — touches online
  components, so it waits for him.

### M4 — 4–8 players + release candidate

- T1 layouts 5–8, per-cell HUD scale at 320×360 / 426×240, FBO sizing,
  perf pass (target ≥ 120 fps with 8 views at 1440p on this GPU; measure
  per-view `R_RenderFrame` time). T2 pads 5–8 (maintainer). T3
  `VERSION` / `BUILD-INFO.txt`, CHANGELOG, maintainer pass on the release
  candidate; the release number and timing are the maintainer's call.
  *As run (T1, 0.0.0.16):* done; details in §3.4 "As built (M4-T1)".
  Layouts 5–8 + `cl_splitFill` / `cl_splitWidePlayer` (host page rows),
  `cl_splitMaxPlayers` default 8, fit-to-640 fractional HUD scale per cell
  (fits, but 320-wide cells are unreadable), post-process FBO sized per
  frame + the bloom scissor fix, `cl_splitperf`. 8 players end to end on
  the listen server (virtual-pad joins, team menus, leave / rejoin, dedupe)
  and on the dedicated rig with `sv_iplimit 8` (all in, `gamemap` followed).
  8 views at 1440p: 412–460 fps (target 120).
  *As run (M4 fixes + T3, 0.0.0.17):* acceptance notes N1–N5, N7 and the
  review's notice-cell / FBO-alternation items fixed (§3.4 "As built (M4
  fixes)"); release candidate: `scripts\package.ps1` → 
  `build\aqtion-splitscreen-<VERSION>.zip` (`q2pro-splitscreen.exe`, our game DLL with a
  version resource now, `launch-splitscreen.bat`, `install.ps1` with
  `-Uninstall`, README, CHANGELOG, LICENSE, BUILD-INFO); `install.ps1`
  installs into `<AQtion>\splitscreen\` with a SHA-256 manifest and
  replaces nothing; tested install → launch → uninstall against the Steam
  folder (listing identical afterwards). T2 (pads 5–8) and the RC pass are
  the maintainer's.
  *As run (0.0.0.19):* the launcher batch file is gone; the exe locates
  itself (`src/windows/system.c` `split_self_locate`, called from
  `Sys_Init`): when its folder has no `baseaq\` but the parent has
  `baseaq\` and `action\`, `basedir` defaults to the parent, `homedir` to
  `<exe dir>\home` (created), `sys_forcegamelib` to `<exe dir>\gamex86_64.dll`
  and `game` to `action`, each only when not given on the command line.
  One `split: running from ...` line after the log opens. Run from the
  repo's `build\` or an AQtion root, nothing changes.
  *As run (0.0.0.20, first run + two-file zip):* the zip is
  `q2pro-splitscreen.exe` + `README.txt` only. Self-locate also defaults
  `libdir` to the parent, and `sys_forcegamelib` only points at a
  `gamex86_64.dll` that actually sits beside the exe (dev builds), so a
  release hosts with **Steam's `action\gamex86_64.dll`** through the
  legacy API-4 adapter (§8). With a `homedir`, `q2config.cfg` is read only
  from there (`FS_AddConfigFiles`, `FS_DIR_HOME`): a stale one in Steam's
  `action\` had made the first run an 800x600 window. No config there =
  first run (`FS_FirstRunDefaults`): `vid_fullscreen 1` with "desktop" as
  the first `vid_modelist` entry (a borderless window at the desktop
  size, no mode change); q2config / autoexec / `+set` still win. First-run
  pass on the real Steam folder: `work/firstrun-run.log`,
  `work/firstrun-*.jpg`, `work/firstrun-steam-folder-diff.txt`.
  *As run (0.0.0.21, run from the AQtion root):* the release is "put the
  exe in your AQtion folder". `split_self_locate` now knows two placements,
  by the folders beside the exe (never its file name): **root** (its folder
  has `baseaq\` + `action\`): `basedir` / `libdir` = that folder, `homedir`
  = `<AQtion>\splitscreen` (created), no `sys_forcegamelib` (a
  `gamex86_64.dll` in the root, Steam ships one, is never used; the library
  is `libdir\action\gamex86_64.dll`), and the **working directory** moves to
  `<AQtion>\splitscreen` (only when `basedir` and `libdir` are absolute):
  Steam's TNG 2.82 DLL writes `items.txt` into the working directory at
  every map load (and bots write `action\bots\...`), which in the root is
  Steam's folder. **Subfolder** (as 0.0.0.19/20): `homedir` = `<exe
  dir>\home`, a DLL beside the exe is used. Both default `game` to
  `action`; `+set` wins. The `split:` line names the placement. Verified on
  the real Steam folder with a test-named exe (`work/rootrun-run.log`,
  `work/rootrun-01-*.jpg`, `work/rootrun-steam-folder-diff.txt`).
  *As run (0.0.0.22, working directory = AQtion root):* the 0.0.0.21 move
  of the working directory to the homedir is reverted and replaced by one
  rule for **both** placements: the working directory is the **AQtion
  root** (`basedir`), exactly what Steam's launch gives the game DLL, set
  at the end of `split_self_locate` only when `basedir`, `libdir`, a
  non-empty `homedir` and a non-empty `sys_forcegamelib` are all absolute
  (a relative `+set` path keeps the old exe-folder cwd). The engine itself
  has no cwd-relative path after `Sys_Init` (`basedir` / `libdir` /
  `homedir` absolute; crash dumps and `anticheat` use the exe folder; the
  only other relative path, `./SteamCloud/...` in `files.c`, is used only
  with a Steam `steamid` and then means the same as under Steam). The
  reason: the Action DLL (Steam's TNG 2.82 and `src/action`) opens its own
  files with plain `fopen` relative to the cwd: `action/action.ini`,
  `action/tng/<map>.aqg` / `.flg` (locations), `action/terrain/<map>.ltk`
  (ACE bot nodes), `action/bots/*` (bot names), vote / map lists, MOTD, and
  it writes `items.txt` and bot files there. Consequence: those writes land
  where Steam's own AQtion writes them (`<AQtion>\items.txt`, possibly
  `action\bots\...`); the engine still writes only into the homedir. The
  `split:` line ends `working directory <AQtion root>` (or `<exe dir>
  (unchanged)`). Note: Steam's install keeps `action.ini`, `sndlist.ini`
  and `tourney.ini` in `baseaq\`, so "Unable to read action/action.ini" is
  printed under Steam's own AQtion too. Found on the way: Steam's TNG 2.82
  with ACE bots (`am 2`) never finishes spawning `beer` (Steam's own
  `q2proded.exe` hangs the same way, so it is AQtion's bug; before 0.0.0.22
  the bots simply had no nodes there). Evidence `work/cwd-run.log`,
  `work/cwd-steam-folder-diff.txt`.
- Optional later: Independent mode (Q3 §17.2) — cheap in Q2PRO terms (N
  processes with distinct `homedir`), only if asked.

---

## 11. Open decisions for the maintainer (default taken if unanswered)

1. **Pad backend on Windows:** static SDL2 from WrapDB vs `SDL2.dll`
   beside the exe vs XInput-only. *Default: static SDL2 if Spike P
   passes, else `SDL2.dll`.*
2. **Single-player servers:** splitscreen needs `deathmatch 1` (the
   listen server's UDP socket exists only with `maxclients > 1`).
   *Default: a join on a `maxclients 1` server prints how to start a
   multiplayer game; the host page's start-game sets `deathmatch 1` and
   `maxclients` ≥ 8.*
3. **2-player layout:** top/bottom vs left/right. *Default: top/bottom,
   `cl_splitVertical` to switch (as Q3).*
4. **HUD scale in small cells:** integer scale + clipping vs fractional
   (soft) scale. *Default: integer + clipping; revisit after Spike C.*
   *Taken at M4-T1: fractional, so that every cell is at least 640
   virtual pixels wide (§3.4 as built); readable down to 426×240, not at
   320 wide.*
5. **Sound on OpenAL:** P1 listener + dedupe only, or force the DMA
   backend while N>1 to get nearest-listener mixing. *Default: P1
   listener + dedupe on OpenAL, the full rule on DMA.*
6. **P1 input:** `cl_splitP1Input pad` (Q3 §14.2). *Default: pad.*
7. **Online identity:** extras blank `steamid` / `discordid`, use distinct
   names, and nothing gets around `sv_iplimit` or anticheat. *Default:
   exactly that.*
8. **Per-player cvars beyond userinfo:** *Default: userinfo + the AQtion
   cvar-sync names per player; crosshair style and other HUD cvars
   shared.*
9. **Upstream PRs** (context macros, `R_SetDrawRect`, gamepad layer).
   *Default: decide after M2.*
