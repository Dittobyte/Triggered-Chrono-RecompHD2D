<!--
SPDX-FileCopyrightText: 2026 jerbanpls
SPDX-License-Identifier: GPL-3.0-or-later
-->
# R3: asset pack research (Mesen2 SNES packs, RT64 storage, upscalers, material maps, loader changes)

Date of research: 2026-11-16. Read-only. Extends `docs/ASSET-PACKS.md`, `docs/PRIOR-ART.md`
(A, A3, A4, "What we should change") and `docs/HD2D.md` ("Asset packs"); none of that is repeated.

Tags: **[verified: source read]** means I opened the page, header or file named. **[unverified]**
means search snippet, memory, or inference only. Hard rules from the owner apply throughout: no game
data or extracted assets in the repo, packs are user-supplied, no copyrighted art or names, third-party
code only with a verified licence compatible with GPL-3.0-or-later, recorded in `THIRD_PARTY.md`.

## 0. Findings that change the plan

1. PRIOR-ART "change #1" (read Mesen `hires.txt` packs) is **dropped**. Mainline Mesen2 has no SNES
   HD pack code, and `hires.txt` is NES-shaped (section 1). Keep `pack.toml`; do not write a
   `hires.txt` reader.
2. The loader has **no path-traversal guard**: `load_one` does `snprintf("%s/%s", dir, file)` with the
   manifest's `file` as written, and a grep of `frontend/packs/*.c` for `..` finds no check
   [verified: source read, `frontend/packs/packs.c` lines 694-760]. A shared own-art pack with
   `file = "../../../x.png"` or `/etc/...` makes the loader open arbitrary paths. Today it only reads
   PNG/glTF/audio, so the damage is probing and decoder exposure, but it breaks the "data-only,
   safe to share" promise and must be fixed **before** any container format (zip-slip is the same bug).
3. The atlas is uncompressed RGBA8 with 4,096 px pages (64 MB each) and is fully resident. RT64's
   answer (DDS + mips + streaming) does not drop in as-is on this repo's GL 3.3 / GLES 3.0 target
   (section 2.4). A compressed on-disk cache that is transcoded at load is the safer port.
4. `docs/ASSET-PACKS.md` has duplicated "Legal notes" text near the top (two "The project's packages
   never contain a pack" bullets and a repeated intro line). Cosmetic, but fix when that file is next
   edited [verified: source read, lines 1-60].

## 1. Does mainline Mesen2 support SNES HD packs, and in what format?

**Answer: no.** [verified: source read]

- `SourMesen/Mesen2` is **archived** (read-only since 2026-06-04; README: "an archive of Mesen as it
  was around version 2.1.1 in mid-2025", points to MesenCE for the latest). GPL-3.0, "Copyright (C)
  2014-2025 Sour". Sources: https://github.com/SourMesen/Mesen2 and the GitHub REST API
  (`/repos/SourMesen/Mesen2`, `archived: true`, `pushed_at 2026-06-04`).
- I listed the whole git tree (`git/trees/master?recursive=1`). The only HD pack code is under
  **`Core/NES/HdPacks/`** (`HdPackLoader.cpp`, `HdNesPack.cpp`, `HdNesPpu.cpp`, `HdPackBuilder.cpp`,
  `HdPackConditions.h`, `HdData.h`, `HdVideoFilter.cpp`, `HdAudioDevice.cpp`, `OggMixer.cpp`, ...)
  plus the UI builder window. There is **no `Core/SNES/HdPacks`**.
- The same is true of the maintained fork `nesdev-org/MesenCE` (GPL-3.0, pushed 2026-11-15): only
  `Core/NES/HdPacks` [verified: source read, tree listing].
- Mesen's own doc page https://www.mesen.ca/docs/hdpacks.html describes only NES / HDNes. The word
  SNES appears once, as a comparison for audio replacement ("similar to the MSU-1") [verified: source read].

**What the NES `hires.txt` dialect is** (same page, v105): text lines `<ver>105`, `<scale>N`,
`<img>file.png`, `<tile>img,tileData,paletteData,x,y,brightness,defaultTile`, `<condition>` lines
(`hmirror`, `vmirror`, `bgpriority`, `tileNearby`, `spriteNearby`, `tileAtPosition`,
`spriteAtPosition`, `memoryCheck`, `ppuMemoryCheck`, `...Constant`, `frameRange`). `tileData` is a
CHR ROM offset or a 32-hex-digit (16-byte) CHR RAM string; `paletteData` is **8 hex digits = 4 bytes**
(NES palette). Those fields are NES facts (2bpp 8x8, 4-entry palettes), so they cannot carry a SNES
2/4/8 bpp tile with a 16- or 256-colour 15-bit palette without inventing new fields. [verified: source read]

**The one SNES attempt found:** `Beacher83/Mesen2-SNES-HD_Projekt`, "Mesen 2 fork with SNES HD
texture pack support", GPL-3.0, 0 stars, last push 2026-07-06 (milestone commits such as "M5.19").
https://github.com/Beacher83/Mesen2-SNES-HD_Projekt [verified: source read via API].
- It adds `Core/SNES/HdPacks/{SnesHdData.h, SnesHdPackLoader.cpp, SnesHdVideoFilter.cpp}`.
- Its format is **not `hires.txt`**. Loader comments show a folder layout: `manifest.json`,
  `hashes.bin` (content hashes, little-endian), `checksums.bin` (legacy), `fingerprints.bin`,
  `bg/` (`bg1`..`bg4`, per-`gfxset_NN` subfolders) and `sprites/`, with PNG names like `4000_P03.png`
  (= VRAM word address 0x4000, palette 3). The key is `{FNV-1a 64 of the tile's VRAM bytes, palette
  0-7, layer 0-4}` with a legacy VRAM-address mode. `SNES_HD_PACK_PROJECT.md` (partly German) says it
  is driven by an external JS viewer/exporter for one game (DKC2). Maturity: single author, game-specific
  tooling, no releases seen. [verified: source read of `SnesHdPackLoader.cpp` head, `SnesHdData.h`,
  `SNES_HD_PACK_PROJECT.md` head]

**Consequences for us**
- Our `pack.toml` + decoded-index FNV-1a key is the only general SNES dialect I can show exists
  (it is a different hash: ours hashes decoded indices + palette colours, the fork hashes raw VRAM bytes,
  so keys are **not** interchangeable). No community packs to be compatible with. Do not promise
  compatibility with the fork. [verified for the fork's hash; our hash from `packs.h` comments]
- Optional, low priority: a one-way importer `tools/pack_import_mesen_snes.py` is not worth it until
  someone asks; the fork's packs are tied to its own key and a ROM-specific exporter. [unverified demand]
- Fix `docs/PRIOR-ART.md` A "SNES" bullet and change #1 to the verified statement above.

## 2. RT64-style pack storage: what to port

Sources: https://github.com/rt64/rt64 (MIT, "Copyright (c) 2024 RT64 Contributors", LICENSE read),
`TEXTURE-PACKS.md`, `src/common/rt64_replacement_database.h`, `rt64_filesystem_zip.cpp`,
`src/tools/texture_packer/`. [verified: source read for all]

### 2.1 What RT64 does

| Piece | RT64 behaviour | Evidence |
| --- | --- | --- |
| Database | `rt64.json`: `configuration` (autoPath rice/rt64, configurationVersion 3, hashVersion 5, defaultOperation, defaultShift), `operationFilters` and `shiftFilters` (wildcards `*` `?`, in order, entry wins over filter), `textures[]` with `hashes{rice, rt64}`, `path`, `operation`, `shift` | TEXTURE-PACKS.md |
| Images | DDS first-class (BC7, mipmaps, "almost four times" less VRAM than PNG); PNG only for authoring ("do not ship PNG to end users"); loader tries DDS then PNG, extension ignored | TEXTURE-PACKS.md |
| Hash | XXH3 over TMEM bytes; Rice hash accepted for old packs | TEXTURE-PACKS.md |
| Load modes | `stream` (async, default, may pop in), `preload` (resident), `stall` (block) | TEXTURE-PACKS.md |
| Low mip cache | `texture_packer --create-low-mip-cache`: one file of small mips loaded at start so streaming has no pop-in; advised 100-200 MB max | TEXTURE-PACKS.md |
| Container | `.rtz` = a zip, `texture_packer --create-pack`, zstd default (or `--deflate`/`--store`); when both DDS and PNG exist only DDS goes in; zstd "faster than uncompressed reads" even on NVMe (their claim) | TEXTURE-PACKS.md; `rt64_filesystem_zip.cpp` includes `miniz/miniz.h` and `zstd.h` |
| Dev mode | F1 debugger: dump, replace in-game, save directory; not possible on `.rtz` | TEXTURE-PACKS.md |
| Memory | no minimum; textures streamed in and evicted by available VRAM | TEXTURE-PACKS.md |

### 2.2 Licences of the pieces RT64 uses (for anything we might port)

- RT64 code: MIT. [verified: source read, LICENSE]
- The `texture_hasher` tool is GPL (to be Rice-compatible), per TEXTURE-PACKS.md. We have no Rice
  need; do not look at it.
- Submodules named in `.gitmodules`: `ddspp` (MIT, "Copyright (c) 2018-2025 Emilio Lopez"; API
  licence field MIT), `miniz` (MIT per API), `xxHash` (library part BSD-2-Clause, "Copyright (c)
  2012-2021 Yann Collet"; the repo's CLI tools are GPL-2.0+, so vendor only `xxhash.h/.c`), `zstd`
  (the `LICENSE` file reads "BSD License"; the project is also dual-licensed GPLv2, [unverified] which
  `COPYING` file I did not open), `nlohmann json` (MIT, [unverified]). All permissive, so GPL-3.0+
  compatible once their file headers are checked at vendoring time.

### 2.3 What to port, ranked by value for us

1. **The database idea, in our own TOML**, not RT64 code. We already have it (`pack.toml`, key to
   file). Add only the *filters*: `[[filter]] match = "tiles/4*"` with `operation`/`filter` so a
   pack can say "this directory stays resident" without one line per tile. Cheap, data-only.
2. **A single-file container.** Not zstd+miniz zip on day one: a 1,000-line dependency set for what
   is a read-only archive. Proposal: **`.ctpack`**, a plain **ZIP with STORE or DEFLATE** read through
   miniz (MIT), `pack.toml` inside, same relative paths. Reason: authors can build it with any zip tool,
   hand-edit it, and the loader code path (`fopen(dir/file)` becomes `pack_read(handle, rel_path)`)
   is the same for dir and zip. zstd is an optional later entry compression (miniz does not read zstd;
   RT64's own zip layer is custom). [unverified: that miniz alone is enough; it reads deflate and store]
3. **PNG stays the authoring format; a compiled cache is what runs.** RT64's rule ("no PNG to end
   users") exists because decoding PNG at runtime costs time and RAM. For us the cost is a one-off at
   load/F5 (49 MB 4x Leene pack loads quickly per the measured table in ASSET-PACKS.md) and the real
   pressure is the 64 MB-per-page resident atlas. So: **do not make DDS/BC7 the shipping format.**
   Instead add `tools/pack_compile.py` that writes a **per-machine cache** (`ct/cache/NAME.atlas`)
   holding the shelf-packed pages already mipmapped, plus the rect table, invalidated by the pack's
   mtime/size hash. Loading becomes `mmap` + `glTexImage3D`, no PNG decode, no repack. This is the
   part of RT64's design that actually buys speed, without new codecs.
4. **GPU compression is optional and per device.** Our GL targets are 3.3 core and GLES 3.0
   (`frontend/hd2d/hd2d.c` lines 881-883 [verified: source read]). BC7 (BPTC) is core in desktop GL
   4.2, an extension on 3.3, and on GLES needs `EXT_texture_compression_bptc`, which many Android GPUs
   lack; ASTC (`KHR_texture_compression_astc_ldr`) is the usual mobile choice, ETC2 is core in GLES 3.0.
   [unverified: exact extension availability per device; check `glGetStringi` at runtime]. Plan:
   cache stores RGBA8 always; if the GPU reports BPTC (desktop) or ASTC (Android), an encoder step in
   `tools/pack_compile.py` may also write that variant via an *external* CLI the user installed
   (`--encoder 'CMD {in} {out}'`, like `--external` in `pack_upscale.py`). No encoder code enters the repo.
   Note: lossy block compression smears 1-pixel pixel-art edges and palette-exact colours, so
   default off for tiles; sensible for 4x+ painted art. [unverified: visual quality; test on a made-up
   pixel-art fixture]
5. **Stream / preload / stall and the low mip cache.** Our need is smaller: tile art is tiny, the
   atlas is few pages. Streaming matters only if full-game 4x packs outgrow VRAM (ASSET-PACKS.md
   "Limits" already says so). Defer. When needed, copy the *policy*: pages split by `operation`,
   preload pages built into the cache, others uploaded on first use of a key; `packs_map_frame`
   already produces per-frame used entries, which is exactly a streaming request list.
6. **Hash.** Keep the decoded-content FNV-1a. xxHash/XXH3 would be faster but changes every key
   (every existing user dump), for no problem we have (7,000 pieces hashed once a frame via a cache).
   Do not port. If a stronger hash is ever wanted, add `key_v2` as a second hex field, never replace.

### 2.4 Things not to port

RT64's in-game replace UI (we have F5 and the Pack folder), `texture_hasher`/Rice (GPL, N64 only),
`shift` half-texel option (N64 bilinear quirk; ours is nearest-sampled console texels), N64 TMEM
hashing. [verified: source read for the descriptions]

## 3. Placeholder upscalers: exact licences of exact files

Current state: `tools/pack_upscale.py` has a built-in edge-directed scaler "in the manner of xBRZ,
written from the published idea" and no third-party code (header comment line 14 [verified: source
read]). THIRD_PARTY.md mentions neither MMPX nor xBRZ [verified: source read, grep]. So any item
below is new vendored code and needs an entry.

| Candidate | Exact file read | Licence text seen in the file | Verdict |
| --- | --- | --- | --- |
| **MMPX, C99** | https://github.com/ITotalJustice/mmpx `mmpx.c` (raw, master) | Header: "Copyright 2020 Morgan McGuire & Mara Gagiu. Available under the MIT license." (repo has no LICENSE file; GitHub API reports none, raw `LICENSE` is 404) | MIT by file header. **Usable.** Port is a modified C99 copy of the authors' C++ [unverified: modifications]; for provenance prefer the authors' own supplement (see below) |
| **MMPX, GLSL** | https://github.com/mgba-emu/mgba `res/shaders/mmpx.shader/mmpx.fs` + `manifest.ini` | "Copyright 2020 Morgan McGuire & Mara Gagiu, provided under the Open Source MIT license" | MIT by file header. Usable per file (mGBA itself is MPL-2.0 [unverified]; take only this file, keep its header). Useful for a GPU path |
| MMPX, authors' reference | https://casual-effects.com/research/McGuire2021PixelArt/index.html (paper + supplement zip) | A search summary says supplement is MIT, (c) 2020 McGuire and Gagiu | **[unverified]**: page content did not load for me. Download and read its LICENSE before citing |
| **ScaleFX** | https://github.com/libretro/glsl-shaders `scalefx/shaders/scalefx-pass0..4.glsl` | "Copyright (c) 2016 Sp00kyFox", MIT permission text | MIT by file header. 5-pass shader; needs a CPU/GL runner; overkill for tiles |
| **Super-xBR** | `libretro/glsl-shaders` `xbr/shaders/super-xbr/super-xbr-pass0.glsl` | "Copyright (c) 2015 Hyllian", MIT permission text | MIT by file header, 2 passes |
| xBR-lv2 (Hyllian) | `libretro/glsl-shaders` `xbr/shaders/xbr-lv2.glsl` | "Copyright (C) 2011-2016 Hyllian", MIT permission text | MIT by file header |
| **xBRZ (Zenju)** | `snes9xgit/snes9x` `filter/xbrz.h` and `BradleyPudge/XBRZ` `xbrz.h` | "part of the xBRZ project ... GNU General Public License: https://www.gnu.org/licenses/gpl-3.0 ... Copyright (C) Zenju ... Additionally ... special exception" to link with MAME, FreeFileSync, Snes9x (the other copy adds ePSXe) | **GPL-3.0, "or later" not stated.** Treat as **GPL-3.0-only** [verified: source read]. Mixing it into a GPL-3.0-or-later program is legal, but that file/binary is then 3.0-only; REUSE.toml needs a per-file `GPL-3.0-only`. The linking exception does not name us and we do not need it. See below |
| libretro `xbrz-freescale.glsl` | `libretro/glsl-shaders/xbrz/shaders/xbrz-freescale.glsl` | Top: MIT (Hyllian, 2011/2016). Further down: embeds the Zenju GPL-3.0 banner "uses code and/or concepts from xBRZ" | **Mixed.** Because it carries the xBRZ GPL banner, treat the whole file as GPL-3.0 (no better than xBRZ). Do not rely on the MIT top |

Notes
- Hyllian xBR shaders are MIT in the libretro copies I read, but the PRIOR-ART table's "LGPL-2.1+ in some
  projects" is also true elsewhere; the rule stays *read the exact file's header at vendoring time*.
- MMPX is a **2x** filter. For 4x apply twice (MMPX paper: the algorithm is a 2x magnifier) [unverified:
  quality of double application on sprites with alpha; test on our own made-up fixture].
- MMPX handles alpha (the C code's `luma` takes alpha) [verified: source read, `mmpx.c` top], good for
  our transparent tiles. It is edge-preserving, not smoothing, so it will look sharper and blockier
  than the current built-in scaler. Offer as `--scaler mmpx|builtin` rather than replacing.

**Recommendation.** Keep the built-in scaler as default (zero third-party code). Add **MMPX** as
second scaler by porting from the MIT-headed file(s) into a Python function in `tools/pack_upscale.py`
(about 150 lines; the kernel is a handful of comparisons, so a port, not a vendored C file), with the
MIT notice kept above it and a `THIRD_PARTY.md` entry naming the exact source file, commit and the
header text. **Do not vendor xBRZ.** The built-in is already "in the manner of xBRZ" from the idea, which
is the clean way; copying Zenju's code would make that file GPL-3.0-only for little gain. Also keep
`--external` as the route for AI and xBRZ users (their own installs, their own licence).

THIRD_PARTY.md entry template (for the build item):

```
### tools/pack_upscale.py: MMPX
- Morgan McGuire and Mara Gagiu, MMPX, "Copyright 2020 Morgan McGuire & Mara Gagiu.
  Available under the MIT license." Ported from <exact URL at commit SHA>, file mmpx.c, to Python.
  MIT notice kept in the function's comment. Licence text: LICENSES/MIT.txt (already in repo).
```

## 4. Per-tile normal / height / emissive maps as a data-only extension

### 4.1 Where this fits in the renderer

- The 2D HD-2D path today derives "relief" normals **from the picture's brightness** (`light.frag`
  line 108: "normals from the brightness field", Lambert from the key light) [verified: source read].
  Real normal maps from a pack would replace that guess per tile.
- The 3D path samples packed art through `tileTexel(tile, uv)` in `scene3d.frag` with an atlas
  array texture `uPackAtlas` and a rect table `uPackRects` [verified: source read, lines 62-69]. A second
  array texture with the **same page/x/y layout** is addressed by the same rect, so no new lookup.
- Emissive: no emissive input exists in the shaders (grep `emis` finds nothing in `*.frag`)
  [verified: source read]; bloom exists (`bright.frag`), so emissive feeds the bright pass.

### 4.2 Format proposal (`pack.toml` version 2, additive)

Rule: **only images and numbers**. No expressions, no shader text, no file names outside the pack,
every numeric field clamped by the loader. `[pack] version = 1` packs keep working unchanged.

Sibling files by name (so a tile entry stays four lines and tools can glob):

```
tiles/KEY.png        albedo (RGBA), as today
tiles/KEY.n.png      normal map, RGB, tangent space, +Y up (OpenGL convention), +Z out of the tile
tiles/KEY.h.png      height, 8-bit gray, 128 = flat, optional (parallax/AO/derived normals)
tiles/KEY.e.png      emissive, RGB, scaled by `emissive` below, alpha ignored
```

Explicit keys override the naming rule, and the same keys work in `[[pose]]`:

```toml
[[tile]]
key = "67570e230f375981"
file = "tiles/67570e230f375981.png"
w = 8
h = 8
normal = "tiles/67570e230f375981.n.png"     # optional; default: sibling .n.png, else flat
height = "tiles/67570e230f375981.h.png"     # optional
emissive = "tiles/67570e230f375981.e.png"   # optional
normal_strength = 1.0                        # 0-4, default 1; scales xy before renormalising
height_scale = 2.0                           # console px of relief for height 255 vs 0; 0-8, default 0
emissive_strength = 1.0                      # 0-8, default 1; added before bloom, unaffected by lighting

[material]                                    # optional pack-wide defaults, same keys
normal_strength = 1.0
```

Loader rules (all enforced, none optional):
- Each map must have the **same aspect** as the albedo and the same size, or it is resized with a warning
  (same rule as poses); max 2,048 px a side as for albedo.
- Decode via the existing `pngio_read_rgba` (8-bit only, no 16-bit/interlaced: already its limit
  [verified: source read, ASSET-PACKS.md "Editing"]). Normals: decode `n = rgb*2-1`, renormalise,
  discard pixels with `|n| < 0.1` as flat; clamp every float key and treat NaN as the default.
- Missing map = flat normal (0,0,1), zero emissive, zero height: output is **identical to today**, so
  the byte-identical-screenshot guarantee for no-maps packs holds.
- A pack that is only maps (no albedo) is valid: maps attach by key to another pack's albedo. This lets
  an artist add lighting to a placeholder 4x pack without touching its pictures (licence note: the
  normal map made from a dumped tile is still derived from game art, same legal bucket as ASSET-PACKS.md).

Storage: one extra RGBA8 array texture "material" with the **same rects**, packed as
`R = normal.x, G = normal.y (z rebuilt), B = height, A = emissive luminance mask`; emissive colour =
albedo.rgb * A * `emissive_strength`. This costs 2x atlas memory instead of 4x and covers the common
sprite case (windows, lamps, magic glows are the albedo's own colour). Full RGB emissive (`.e.png` with a
colour differing from albedo) is kept as a rare overflow: a third array texture allocated only when some
entry has `emissive` with differing hue. [unverified: that the mask-only form suffices; test with a made-up
lamp tile]

Shader use: `upscale.frag` (2D) samples the material texel with the same sub-texel position and replaces
the brightness-derived normal in `light.frag`'s relief block; `scene3d.frag` perturbs the surface normal
(tangent from the quad's fixed basis, since tiles are axis-aligned quads, so no tangent attribute is
needed [unverified: for rotated billboards]). Height drives only a small optional parallax and AO bias;
leave it out of the first build (item 8 below) if time is short.

Tools: `tools/pack_derive_maps.py PACK` makes **placeholder** maps from a pack's own pictures (Sobel of
luminance for `.n.png`, blurred luminance for `.h.png`), so the pipeline has test data without any
artist, written for this project (no third-party code). Same personal-use caveat in each generated
`pack.toml`.

### 4.3 Why this keeps packs data-only

A pack stays: TOML (flat tables, `key = value`), PNG, glTF, audio. New fields are strings that are
relative paths and bounded numbers. The only parser is the existing line-based
`packs_toml_parse` (fixed buffers: `key[64]`, `val[1024]`). No scripting, no `when` expressions beyond
a closed list (the planned `when` table must be a closed enum of comparisons, never text evaluated).
`tools/check_dist.py` must learn the new names (`*.n.png`, `*.h.png`, `*.e.png`, `.ctpack`) so none can
ship in a package [verified: check_dist.py matches pack.toml, content-key PNGs, sheets, audio; the
new suffixes would otherwise be caught only if their parent folder matches `tiles/`, `sprites/` or
`objects/`].

## 5. How the existing loader should change (files)

Current structure [verified: source read]: `frontend/packs/packs.c` (1,130 lines) holds keys, dump, the
`pack.toml` parse (`packs_toml_parse`, `load_one`), `build_atlas` (shelf packing into 256-4096 px
pages) and per-frame mapping; `pngio.c` is the PNG reader; `hd2d.c:hd2d_pack_atlas` uploads RGBA8 +
`glGenerateMipmap` (max 4 levels); `tools/pack_upscale.py`, `pack_import_sheet.py`, `pack_test_art.py`.

| # | Change | Files |
| --- | --- | --- |
| a | Reject `file` values that are absolute, contain `..`, a drive letter, a backslash, or resolve outside the pack dir (use `realpath` prefix check); also for `[[model]]` file, `[[music]]` file and `.gltf` buffer URIs (cgltf can load external buffers; check `packs_model.c`) [unverified: model buffer path handling] | `frontend/packs/packs.c` (`load_one`), `packs_model.c`, `music.c`, new `packs_path_ok()` in `packs.h` |
| b | Abstract file access: `pack_open(dir_or_zip)`, `pack_read(h, rel, &len)`; backends: directory, ZIP via miniz | new `frontend/packs/packfs.c/.h`; `packs.c`, `pngio.c` (read from memory), `packs_model.c` (cgltf `cgltf_parse` from memory), `music.c` (decoders from memory); `CMakeLists.txt` `ct_packs` sources |
| c | Material maps: parse `normal`, `height`, `emissive`, `*_strength`, `[material]`; load with same rect; build a second atlas | `packs.c` (`pending`, `build_atlas`, `packs_rect` unchanged), `packs.h` (`packs_material_atlas()`) |
| d | Upload material array texture, bind a new unit, bump `pack_textures_init` | `frontend/hd2d/hd2d.c`, `hd2d.h`, `main.c` (call site near line 498) |
| e | Shaders sample the material | `shaders/upscale.frag`, `light.frag`, `scene3d.frag`, `bright.frag` |
| f | Compiled cache: write/read pre-mipmapped pages + rect table, keyed by a hash of the pack's file list/mtimes; F5 rebuilds | `tools/pack_compile.py` (new), `packs.c` (cache fast path), `main.c` option `--pack-cache` (default on) |
| g | MMPX scaler option | `tools/pack_upscale.py` (`--scaler`), `test/test_pack_upscale.py`, `THIRD_PARTY.md` |
| h | Dist guard for new names, docs | `tools/check_dist.py`, `docs/ASSET-PACKS.md`, `docs/PRIOR-ART.md`, `THIRD_PARTY.md` |
| i | Tests | `test/test_packs.c` (traversal, maps, zip), `test/test_pack_upscale.py` |

No game data in tests: fixtures are generated at run time by `tools/pack_test_art.py`-style code (made-up
pictures), as `test_pack_upscale.py` already does.

## 6. Ranked build list (max 10)

1. **Path-traversal guard** in the loader for tile/pose/model/music/glTF-buffer paths.
   Files: `frontend/packs/packs.c`, `packs_model.c`, `music.c`, `packs.h`. Test: in `test_packs.c` write
   a made-up pack whose `file` is `../outside.png`, `/abs/x.png`, and a symlink pointing out; expect each
   rejected with a message and zero entries loaded, and a normal entry still loads. Small, security,
   blocks everything below.
2. **Fix the record**: correct PRIOR-ART A (SNES Mesen statement, drop "read hires.txt" change #1),
   de-duplicate the legal block in ASSET-PACKS.md, and note the fork and its different key.
   Files: `docs/PRIOR-ART.md`, `docs/ASSET-PACKS.md`. Test: `reuse lint` and doc link check.
3. **Pack file access layer + `.ctpack` (ZIP) container** (miniz, MIT, vendored with its header,
   recorded in `THIRD_PARTY.md`). Files: new `frontend/packs/packfs.c/.h`, `packs.c`, `pngio.c`,
   `packs_model.c`, `music.c`, `CMakeLists.txt`, `third_party/miniz/`, `THIRD_PARTY.md`,
   `REUSE.toml`. Test: build a pack dir and a `.ctpack` of it from made-up pictures with Python
   `zipfile`; both load the same entries (same atlas hash); zip-slip names (`../x`) rejected; a corrupt
   archive fails without a crash (fuzz a few truncations).
4. **Material maps (normal/emissive) data format + loader + atlas** as in section 4.2, without height.
   Files: `packs.c`, `packs.h`, `hd2d.c/h`, `main.c`, shaders `upscale.frag`, `light.frag`,
   `scene3d.frag`, `bright.frag`, `test/test_packs.c`. Test: a made-up tile with a left-lit gradient
   normal map in 2D and 3D, expect the lit side brighter than the flat tile in a headless screenshot;
   a no-maps pack stays byte-identical to the current golden screenshots (existing guarantee).
5. **MMPX as a second `pack_upscale.py` scaler** (ported from the MIT-headed `mmpx.c`, notice kept,
   `THIRD_PARTY.md` entry). Files: `tools/pack_upscale.py`, `test/test_pack_upscale.py`, `THIRD_PARTY.md`.
   Test: on the existing made-up 12x12 picture check size, flat areas and alpha kept, diagonal smoothed
   (it will not smooth; assert corners-only changes) and run twice for 4x. Before merging, open the
   authors' supplement LICENSE (still [unverified]) and cite it.
6. **Compiled atlas cache** (`pack_compile.py`, mmap fast path). Files: `tools/pack_compile.py`, `packs.c`,
   `main.c`. Test: load a generated 2,000-tile pack with and without the cache; atlas bytes and entries
   equal; cache invalidated when one PNG's mtime changes; time reported (target: no PNG decode on warm start).
7. **`pack_derive_maps.py` (placeholder normals/heights)** so item 4 has data for artists and tests.
   Files: `tools/pack_derive_maps.py`, `test/test_pack_derive.py` (new), `tools/check_dist.py` (new
   suffixes). Test: flat tile gives (128,128,255); a step edge gives opposite-signed x normals.
8. **Height channel** (parallax/AO bias) and `[[filter]]` load-operation rules (RT64-style wildcard
   filters, data only). Files: `packs.c`, `scene3d.frag`, `light.frag`. Test: unit test of wildcard
   matching (`*`, `?`, ordering, entry beats filter, as RT64 documents).
9. **Optional GPU compression via external encoder CLI** (BPTC desktop, ASTC Android) selected at
   runtime from the GL extension string; RGBA8 fallback always. Files: `tools/pack_compile.py`,
   `hd2d.c` (`hd2d_pack_atlas` format switch). Test: with a fake extension list the loader picks each
   path; images decode in a software-only run [unverified: Android driver coverage; test on the ROG
   Ally and one Android device].
10. **Streaming/preload pages for packs over one atlas** (RT64 policy, our data). Files: `packs.c`,
    `hd2d.c`. Test: synthetic 6-page pack, force a small page budget, assert used entries resident each
    frame and no missing-texture frames across the Leene replay's tile set (synthetic).

Not on the list, by decision: vendoring xBRZ (GPL-3.0-only file, no gain), a `hires.txt` reader (no SNES
dialect exists), XXH3 keys, DDS/BC7 as the *shipping* format, importing the Beacher83 fork's packs.

## 7. Open items I could not settle

- MMPX authors' supplement licence page did not load (needs a manual read). [unverified]
- Whether `cgltf` is called with external-buffer loading in `packs_model.c` (affects item 1). [unverified]
- zstd `COPYING` choice and miniz file header text (check when vendoring). [unverified]
- BPTC/ASTC availability on the target Android devices. [unverified]
- Whether the owner wants any compatibility with the Beacher83 fork; I found no demand either way.

## 8. Sources

- https://github.com/SourMesen/Mesen2 (GPL-3.0, archived 2026-06-04); API tree listing, `Core/NES/HdPacks` only
- https://github.com/nesdev-org/MesenCE (GPL-3.0); tree listing, NES only
- https://www.mesen.ca/docs/hdpacks.html (NES `hires.txt` v105)
- https://github.com/Beacher83/Mesen2-SNES-HD_Projekt (GPL-3.0 fork, `Core/SNES/HdPacks`, own format)
- https://github.com/rt64/rt64 (MIT), `TEXTURE-PACKS.md`, `src/common/rt64_replacement_database.h`, `rt64_filesystem_zip.cpp`
- https://github.com/ITotalJustice/mmpx (`mmpx.c`, MIT header); https://github.com/mgba-emu/mgba (`res/shaders/mmpx.shader/mmpx.fs`, MIT header)
- https://casual-effects.com/research/McGuire2021PixelArt/index.html (MMPX paper; page did not load for me)
- https://github.com/libretro/glsl-shaders (`scalefx`, `xbr`, `xbrz`)
- https://github.com/snes9xgit/snes9x `filter/xbrz.h`, https://github.com/BradleyPudge/XBRZ `xbrz.h` (xBRZ GPL-3.0 banner)
- https://github.com/redorav/ddspp (MIT), https://github.com/Cyan4973/xxHash (library BSD-2-Clause), https://github.com/richgel999/miniz (MIT), https://github.com/facebook/zstd
