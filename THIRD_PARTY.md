# Third-party code and data

This project (Triggered Chrono) is licensed under GPL-3.0-or-later (see `LICENSE`).
It is built on **ThisIsAkill/ct-recomp**, imported with its history at commit
`a6b49257e527d1f219ce6466075b5e035dd8e5f2`. ct-recomp is MIT-licensed
(`Copyright (c) 2026 Akhil Moola`); its license text is kept as `LICENSES/MIT.txt`.
Files from it stay under MIT, and our changes and additions are GPL-3.0-or-later.
`REUSE.toml` maps every file to its license; run `reuse lint` to check.

The notes below come from upstream ct-recomp. Where they say `LICENSE`, the MIT
text now lives in `LICENSES/MIT.txt`.
This file tracks third-party sources used by the project and their terms.

## Currently in use

### Symbol names and entry-state hints

- **dscotton/ct_disassembly** — public domain. Function labels, boundaries,
  and comments are usable directly. Where they conflict with ChronoRET,
  ChronoRET wins.
- **ChronoRET** (this project's own matching disassembly, `../ChronoRET`) —
  source of truth for names and verified entry states.

No files from either source are vendored verbatim; `tools/sync_symbols.py`
reads them to populate `funcs.toml` / `unresolved.toml`.

### Hints only, not copied

- **DocDamage/chronotrigger_disassembly** (MIT) — used only as a hint when
  labeling routines. Anything actually copied from it must be verified
  against our own decoder and keep its MIT notice here.

### Debugging tools, never copied from

- **bsnes**, **Mesen2** (GPL), **snes9x** (non-free) — used only as external
  debuggers/tracers during development. No code from any of them is in this
  repository.

### `frontend/hid_deck.c` (ported code)

- **Simple DirectMedia Layer**, release `2.32.10`: the Steam Deck input report
  layout and its button bits, buttons and axes, from
  `src/joystick/hidapi/SDL_hidapi_steamdeck.c` (Copyright (C) 2023 Max Maisel) and
  `src/joystick/hidapi/steam/controller_structs.h` (Copyright (C) 2020 Valve
  Corporation), zlib license (`LICENSES/Zlib.txt`). Rewritten as a byte-offset parser
  in `hid_deck_parse`, without SDL's hidapi layer, lizard-mode reports or
  sensors. The simple rumble command (`hid_deck_rumble_report`: `ID_TRIGGER_RUMBLE_CMD`
  in a `FeatureReportMsg` / `MsgSimpleRumbleCmd`, filled as
  `HIDAPI_DriverSteamDeck_RumbleJoystick` fills it) is ported from the same two files
  for simulated rumble. The zlib notice is kept at the top of the file, which is
  `GPL-3.0-or-later AND Zlib`. It reads the emulated Steam Deck controller that
  InputPlumber offers on Bazzite handhelds, and sends it rumble only while rumble is
  on (docs/HANDHELD.md, "Bazzite / InputPlumber"). The rest of the file (the rumble
  thread included) is ours.

### `third_party/glyphs/`

- **Kenney, Input Prompts** version 1.3 (CC0-1.0; its `License.txt` is kept
  beside the pictures, the CC0 text is `LICENSES/CC0-1.0.txt`) — 145 of the
  pack's 64 px PNG button and key pictures (Steam Deck, Steam Controller,
  Xbox, PlayStation, Nintendo Switch, keyboard), unmodified, for the button
  glyphs in on-screen messages. Source: <https://kenney.nl/assets/input-prompts>;
  downloaded from the mirror <https://github.com/Maaack/Kenney-Input-Prompts>
  at commit `dd37497091c176e91ce81a14e203729ea12d8e4b`. `tools/embed_glyphs.py`
  (our code) turns them into coverage masks at build time. Details in
  `third_party/glyphs/README.md`.

### `third_party/ares/`

- **ares-emulator/ares**, `ares/component/processor/spc700/`, commit
  `4cb8d92b441557cb6bcaf133c4cbc7f6819b1122` (ISC, "ares team, Near et al"; the repository's
  `LICENSE` is copied whole as `third_party/ares/LICENSE`) — the
  cycle-accurate SPC700 CPU core, vendored unmodified. `runtime/spc700_host.cpp`
  compiles it (without its disassembler and serializer) against a small
  `nall` stand-in (`runtime/nall_shim.hpp`, our code) and drives it against
  the zelda3 APU's memory map, DSP and timers. Details in
  `third_party/ares/README.md`.

### `third_party/cgltf/`

- **jkuhlmann/cgltf**, `cgltf.h` at tag `v1.14` (MIT, Johannes Kuhlmann) -- a
  single-header glTF 2.0 parser, vendored unmodified. `frontend/packs/packs_model.c`
  uses it to read and check the models asset packs name in `[[model]]` entries
  (docs/ASSET-PACKS.md). Details and the file's hash in `third_party/cgltf/README.md`.

### `third_party/dr_libs/`

- **mackron/dr_libs** (David Reid), `dr_flac.h` v0.13.4 (commit
  `806ee9ada8ca9827b22f76827f739c290f37ba48`) and `dr_wav.h` v0.14.6 (commit
  `35b5c392cc15456736c440a68dbe073178bbffe0`): public domain (Unlicense) or MIT No
  Attribution, the reader's choice; vendored unmodified. `frontend/packs/music.c`
  decodes audio packs' FLAC and WAV files with them (docs/ASSET-PACKS.md). Hashes in
  `third_party/dr_libs/README.md`.

### `third_party/fidelityfx/`

- **GPUOpen-Effects/FidelityFX-FSR**, tag `v1.0.2`, commit
  `a21ffb8f6c13233ba336352bdff293894c706575` (`ffx_fsr1.h` FSR 1 v1.20210629, `ffx_a.h`),
  and **GPUOpen-Effects/FidelityFX-CAS**, tag `v1.0.2`, commit
  `9fabcc9a2c45f958aff55ddfda337e74ef894b7f` (`ffx_cas.h` CAS 1.20190610). MIT, Advanced
  Micro Devices, Inc.; vendored unmodified with their license files, and not compiled.
  `ffx_fsr1_cas.glsl` is our port of EASU, RCAS and CAS to GLSL 3.30 / ES 3.00 (the
  headers' GLSL needs GLSL 4.00 / ES 3.10), MIT like its source. The HD-2D renderer's 3D
  diorama uses it for its render scale and sharpening (docs/HD2D.md). URLs, hashes and
  the port's differences in `third_party/fidelityfx/README.md`.

### `third_party/imgui/`

- **ocornut/imgui** (Dear ImGui), release `v1.92.9b`
  (<https://github.com/ocornut/imgui/releases/tag/v1.92.9b>; MIT, Omar Cornut; the
  release's `LICENSE.txt` is kept beside it) -- the immediate-mode GUI library, with
  its SDL2, OpenGL 3 / ES 3 and SDL_Renderer 2 backends, vendored unmodified. The
  frontend's settings menu (`frontend/menu.cpp`) draws with it; nothing else uses it.
  Its stb headers (`imstb_*.h`, Sean Barrett) are public domain / MIT as their
  headers say. Also `fonts/Roboto-Medium.ttf` from the release's `misc/fonts/`
  (Roboto, Christian Robertson, Apache-2.0, `LICENSES/Apache-2.0.txt`), the menu's
  font. Details and hashes in `third_party/imgui/README.md`.

### `third_party/stb/`

- **nothings/stb** (Sean Barrett), commit `2838bd5823c095dd1d5b5d8fac0b4fc8aa72a36d`:
  `stb_vorbis.c` v1.22 and `stb_truetype.h` v1.26, public domain (Unlicense) or MIT, the
  reader's choice; vendored unmodified. `frontend/packs/music.c` decodes audio packs' Ogg
  Vorbis files with the first, `frontend/hdtext.c` draws HD text with the player's own
  font with the second. Hashes in `third_party/stb/README.md`. (Dear ImGui's `imstb_*.h`
  copies are separate.)

### `third_party/snes/`

- **snesrev/zelda3**, `snes/` subtree, commit `fbbb3f967a51fafe642e6140d0753979e73b4090`
  (MIT, snesrev + elzo_d) — PPU, DMA/HDMA, APU, SPC700, and DSP emulation.
  Full license text and file list in `third_party/snes/README.md`.
  `runtime/snes_adapter.c` bridges it into `bus.c`; `dma.c`/`dma.h` are
  otherwise unmodified. Local patches to `ppu.c`/`ppu.h` (all marked
  `ct-recomp:` in place):
  - `OBSEL` ($2101): upstream asserted one fixed value and never actually
    decoded it (`objSize`/`objTileAdr1/2` were hardcoded in `ppu_reset`).
    Implemented the real decode.
  - `BGMODE` ($2105) bit 3 (BG3 priority): upstream hardcoded "always on
    for mode 1" instead of reading the bit. Added `Ppu.bg3Priority` and
    read it for real.
  - `BGMODE` ($2105) bits 4-7 (BG character size): upstream never decoded
    them (A Link to the Past uses 8x8 BG tiles only). Added
    `BgLayer.bigTiles`; the per-pixel BG fetch handles 16x16 characters
    per fullsnes.
  - Asserts on `BGMODE`, `M7SEL`, `VMAIN`, `WBGLOG`/`WOBJLOG`, `CGWSEL`,
    `SETINI`, `OAMADDH` restricted every value to what A Link to the Past
    happens to use. Relaxed so Chrono Trigger's actual register writes
    don't abort; several of the underlying features (window AND/XOR/XNOR
    logic, VRAM address remapping, direct color mode, interlace/hi-res/
    overscan) are still genuinely unimplemented, not just untested --
    see issue #9's gap report for the full list.
  - Offset-per-tile (BG modes 2, 4, 6): upstream never implemented it (A
    Link to the Past doesn't use it). Added `ppu_offsetPerTile` and
    `ppu_bg3TilemapWord`, following fullsnes.
  - `CGDATA` ($2122): bit 7 of the high byte is no longer stored (CGRAM is
    15-bit).
  - Brightness: each 5-bit channel is scaled by brightness / 15 before it
    is expanded to 8 bits (Mesen 2); upstream scaled the expanded value.
  - Provenance G-buffer (port, GPL-3.0-or-later, for asset packs):
    `PpuSetProvBuffer` records per pixel which character, palette and
    sprite texel the main screen's winner came from (`PpuProv`);
    sprite evaluation is compiled in two versions so the path with it off
    is unchanged.
  - Lines are drawn as they go: `ppu_runLine` sets a line up and
    `ppu_drawTo` draws it up to a pixel, so a register write partway
    through a line (the adapter draws up to the write's dot first) changes
    only the rest of it. Upstream drew each line whole at its start.

  Local patches to `apu.c`/`apu.h`: `apu_tick` (the DSP, timer and cycle
  count half of `apu_cycle`, split out so the SPC700 core in
  `third_party/ares/` can drive it per cycle); `apu_inport_read`, the hook
  the SPC700's $F4-$F7 reads go through; and (marked `ct-recomp:`) the
  timers' stage-1 clock, which falls every 128 cycles (16 for timer 2) from
  reset, first at the end of cycle 128 (bsnes/ares), where upstream ticked
  at cycle 0 and every period from there.

## Auditing

`git ls-files` plus a size scan of this repo are run periodically to confirm
no ROM data or ROM-derived binary assets are tracked. See the setup report
for the most recent audit.
