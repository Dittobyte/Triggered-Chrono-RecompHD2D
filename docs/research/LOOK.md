<!--
SPDX-FileCopyrightText: 2026 jerbanpls
SPDX-License-Identifier: GPL-3.0-or-later
-->
# R2: how the commercial HD-2D games build the look, mapped to `frontend/hd2d/`

Research note, ideas only. Nothing here is copied code or art. It extends and corrects
`docs/VISUAL-DESIGN.md` and `docs/HD2D.md` (both read in full) and does not repeat them.

Marks: **[verified: source read]** means I read the cited page or repo file in this
session. **[unverified]** means it comes from a search summary, memory, or inference and
no page I read states it. Numbers marked [unverified] are starting values to tune against
the acceptance checks, not facts about the commercial games.

## 0. What the public record actually contains (read this first)

The honest finding: **no public source gives numeric post-process settings for any
commercial HD-2D game.** Developers have published slide decks and interviews that name
techniques and goals, never FOV, blur radius or bloom threshold. Every number in
VISUAL-DESIGN.md (tilt 40, FOV 30, bloom 0.7) is therefore our own tuning and cannot be
"checked against Octopath". Treat those as design choices, and judge by eye against
screenshots, not by citation.

What the sources do say:

| Source | What it gives | Mark |
| --- | --- | --- |
| Acquire, Unreal Fest East 2018, Octopath Traveler. https://www.docswell.com/s/EpicGamesJapan/KXPWY5-UE4_UFE2018_ACQUIRE_OCTOPATHTRAVELER | All post effects come from UE4 PostProcessVolume, tuned per map; before/after of Color Grading and Bloom (no values); goals "dynamic, rich lighting", dark areas lit so texture stays visible; DOF set "deep" so near and far dots both stay sharp, changing dynamically in battle; battle camera slightly angled with constant subtle sway; interiors built at half scale; a door trigger swaps lights, post effects, visible objects, camera distance and character size for seamless indoor/outdoor; chromatic aberration only on BREAK | [verified: source read] (via fetch summary; slide images themselves not seen) |
| Square Enix/Historia, Unreal Fest West 2022, Live A Live. https://docswell.com/s/EpicGamesJapan/Z32GJ5-UE_UFE22W_SQEX_Historia | Deferred rendering for look-dev; **DOF weaker than earlier HD-2D titles because the game relies on depth; vignette off or weak**; per-map brightness lift so caves stay readable and sprite colors visible; directional-light shadows outdoors, **round blob shadows indoors**; "telephoto lens prevents distortion at screen edges"; camera angled top-down; aperture change on scene transitions; background pieces fade when a character overlaps them (UE layers); bosses get outline from two enlarged dot images behind the sprite | [verified: source read] (via fetch summary) |
| Wikipedia HD-2D. https://en.wikipedia.org/wiki/HD-2D | Tilt-shift, DOF, bloom, volumetric lighting, fog, particles, parallax; a point light so characters and objects cast shadows on the environment; Acquire relied heavily on UE4 | [verified: source read] |
| Nintendo Life / Nintendo Everything on Triangle Strategy. https://www.nintendolife.com/news/2022/05/triangle-strategy-producers-talk-hd-2d-and-why-other-devs-havent-used-it | Artdink had the "accurate" HD-2D look; others made photoreal and reduced it; character proportions must be deformed enough that pixels stay big, since realistic proportions at a given resolution make pixels shrink and sprites read as illustrations | [verified: source read] (search summary of article text; I did not open the article) |
| PlayStation Blog, DQ3 HD-2D interview. https://blog.playstation.com/2024/11/08/dragon-quest-iii-hd-2d-remake-interview-how-the-classic-rpg-was-revamped-for-its-ps5-debut/ | Goal: 2D sprites look natural in 3D "like a miniature garden"; DQ3 does not use pixels in backgrounds (Octopath does); flickering torches, dust catching light, falling leaves | [unverified] (search summary only) |
| Community spec, Godoiosis #176. https://github.com/Phaazoid/Godoiosis/issues/176 | One person's HD-2D recipe: FOV 25-35, pitch 35-45, Y-billboard nearest-filtered sprites, sun plus warm point lights, glow with high threshold, strong top/bottom blur, art kept flat-lit so grading and lights can relight it | [verified: source read], but a hobbyist spec, not a commercial source |
| Hobby deferred pipeline, dev.to. https://dev.to/gaurav_de/creating-an-hd-2d-rendering-pipeline-on-dx12-205k | Sprite normal maps from a tool (Laigter); alpha-test sprite shadows (discard alpha < 0.9) so shadows follow the silhouette; peter-panning from shadow bias; DOF as a 64-tap golden-angle disc gather, depth-weighted; screen-space contact shadows; ACES tonemap | [verified: source read] |

Not found despite searching: any CEDEC talk, any Digital Foundry breakdown with numbers,
any statement about water, fog density, bokeh shape or normal maps in the commercial
games. Anything about those below is inference from the look, marked [unverified].

## 1. Corrections to VISUAL-DESIGN.md

1. **"Remakes' look" claims are not sourced.** The doc's 40 degree tilt, 30 FOV,
   "sprites take 40% of the scene light" are our design, fine, but present them as such.
   The one hard datum we have is Live A Live: the team *reduced* DOF and vignette because
   they flattened the picture. That supports keeping our values modest, not raising them.
2. **Vignette 0.35 is probably high.** Historia (Live A Live) turned vignette off or weak
   as "mood we did not want" [verified: source read]. Keep 0.35 on `hd2d` if tests like it,
   but drop to about 0.2 for `hd2d3d` and expose it; the check L13 (25-35% corner
   darkening) should become "10-30%".
3. **DOF in Octopath is "deep" in battle** (near and far dots both sharp) [verified:
   source read]. The tilt-shift look people associate with HD-2D is mostly the *field
   camera's* strong top/bottom blur [unverified]. So per-context DOF: field strong,
   battle/menus weak. The repo already turns DOF off on UI-heavy frames; add "battle
   scene" as a low-DOF context if the module can flag it.
4. **Indoor shadows can be blob shadows, by design.** Live A Live uses round blob shadows
   indoors and directional shadows outdoors [verified: source read]. VISUAL-DESIGN
   improvement 9 already wants a blob; make it the *primary* indoor shadow and drop the
   shadow-map requirement in dark interiors (cheaper, and removes acne checks).
5. **Bloom check L5 is not implemented as written.** VISUAL-DESIGN says bloom needs
   brightness *and* saturation or local contrast; `shaders/bright.frag` thresholds on
   max channel only, with a 0.2 soft knee, and masks only UI [verified: source read of
   repo]. White clothing and sand will bloom. See item 4 below.
6. **Pixel density is a design input, not only a render rule.** Triangle Strategy's
   producers say sprite proportions were chosen so pixels stay large [verified: source
   read]. For us sprites are fixed by the ROM, so the equivalent is the integer-zoom rule
   (already rule 3); nothing to add except do it (item 6 below).

## 2. Per-technique findings and repo mapping

State of the repo is from reading `frontend/hd2d/shaders/*.frag` and `hd2d.c` presets.

### 2.1 Sprite lighting and fake normals

- **Commercial:** sprites are flat quads, upright, in the 3D lighting; the Octopath deck
  shows only `PixelNormalWS` used for projecting texture on top faces, i.e. no sprite
  normal-map statement [verified: source read]. Hobby pipelines use tool-made sprite
  normal maps [verified: source read, dev.to]. The community spec says art should stay
  flat-lit so lights can relight it [verified: source read, Godoiosis]. Whether Square
  Enix bulges sprite normals is [unverified].
- **Repo today:** 2D path (`light.frag`) derives relief normals from brightness (Lambert,
  one warm key, rim). 3D path (`scene3d.frag`): sprites (`obj`) take `rel = mix(shade,
  rel, 0.4)`, a fixed per-face normal from `NORMALS[]`, no relief, no rim, no bulge
  [verified: source read]. So 3D sprites are lit as a flat face, which is exactly what
  the commercial games do; the difference is mainly that ours get no point-light
  directionality.
- **Change (small):** in `scene3d.frag`, for `obj` fragments build a fake normal
  `n = normalize(vec3(sx, 0.35, sz))` from the sprite quad's local x in -1..1 (a cylinder
  bulge, not read from the art) and use it for the point-light and key terms only. Do not
  touch the color shading (rule: art carries its shading). Starting values: bulge
  strength 0.5 x, y component 0.35 [unverified]. Check: a sprite beside a torch is
  brighter on the torch side by 10-20%, no shape change, no dark outline; in daylight the
  sprite differs from today's by under 3% mean luminance.
- Also add a rim: `rim = pow(1 - dot(n, viewDir), 3) * keyTerm * 0.15` on sprite edges
  facing the key [unverified values]; edge-only, keep it under 15% brightening (L10 ring
  limit is 6% over either side, so keep rim inside the sprite's own pixels).

### 2.2 Shadows

- **Commercial:** outdoor directional shadow, indoor round blob [verified: source read];
  battle uses a per-level dynamic point light for shadow variation and turns the sun off
  during skill lighting [verified: source read].
- **Repo today:** `scene3d.frag` shadow map via `shadowAt()`, `uDepth.w` shadow amount;
  2D path casts actor silhouette shadows with a pixel offset [verified: source read].
  Sprite shadow-map cutout uses `SHADOW_PASS` alpha test [verified: source read].
- **Change:** in dark scenes (the 2D darkness measure the code already has, wired to
  `uAmount2.y` style signals) skip the shadow-map tap for actors and draw a blob: an
  ellipse under the foot, radius about 7 console px by 3.5, alpha 0.35, cool tint,
  feathered edge 2 px, snapped to the console pixel grid. Check L8 stays true (touches
  feet, 30-40% dark) and L16 (no swim). Files: `scene3d_world.c` (add a blob quad per
  sprite), `scene3d.frag` (a mode flag), `hd2d.c` (threshold).

### 2.3 Depth of field and bokeh

- **Commercial:** Octopath uses UE4's DOF with a Bokeh slide, but the slide image is from
  UDK as an illustration [verified: source read, per fetch summary]; values none.
  Disc/spiral gather is the standard (hobby example: 64-tap golden-angle spiral with
  depth-weighted samples [verified: source read, dev.to]).
- **Repo today:** `coc.frag` + `dofblur.frag`: half-resolution **separable** blur with a
  Gaussian weight `exp(-2.5 t^2)`, up to 15 taps, weights `0.08 + coc` per sample against
  halos, near field blurred by row only (`fgCoc`) [verified: source read]. A separable
  Gaussian cannot make bokeh discs, which is the improvement #12 target, and at radius
  6 px (half res 3 texels) the difference is small.
- **Change:** a gather pass instead of the two separable ones, only where `coc > 0.5`
  (so most pixels cost nothing): 16 taps on a golden-angle spiral, radius `r = sqrt((i +
  .5)/16) * uRadius * coc`, per-pixel rotation from interleaved gradient noise to turn
  banding into grain you already have at 0.018. Weight each tap by `0.08 + tap.coc` and
  by a small brightness term `1 + 4*max(0, luma - 0.8)` so lights become discs
  [unverified values]. Keep `dof_radius` 6 / 3.5 (deck). Check: L4 (no halo), a lamp at
  the top edge renders as a soft disc not a cross; cost within +0.3 ms on the Ally
  (budget is 1.5 / 3.0 ms). Files: `shaders/dofblur.frag` (replace), `hd2d.c`
  (pass setup, quality tier for `deck`).

### 2.4 Tilt-shift

- **Commercial:** "tilt-shift" is listed by Wikipedia as part of the stack [verified:
  source read]; whether it is a true shear-plane blur or just DOF with a vertical focus
  plane is [unverified]. Octopath battles use deep DOF instead [verified: source read].
- **Repo today:** `cocAt()` is a vertical band plus smoothstep ramp, and `tilt` 0.022
  enlarges the foreground plane [verified: source read]. This is already tilt-shift.
- **Change:** none to the shader; change the **context**: scale `dof_tilt` by a battle/
  menu factor (0.3) and ease over 8 frames. Check: battle frame blur amount (debug view
  3) below 0.3 away from the UI; field frame unchanged.

### 2.5 Bloom

- **Commercial:** UE4 bloom, before/after only [verified: source read]; thresholds
  unknown. Community spec: "high HDR threshold so only bright lights bloom"
  [verified: source read].
- **Repo today:** `bright.frag` max-channel threshold 0.7 (HD2D_FULL), soft knee 0.2,
  no saturation/contrast test, UI masked; two Gaussian levels (1/4, 1/8), strength 0.45
  [verified: source read]. Note the 2D path also has `emit.frag` that already finds
  lights using brightness, saturation and local contrast [verified: source read of
  HD2D.md description; shader not opened].
- **Change:** feed bloom with `emit.frag`'s light mask as a gate: `w *= mix(0.35, 1.0,
  lightMask)` so a bright but non-light pixel (white cloth, snow) glows at 35% of the
  full amount; threshold 0.7 -> 0.75; add a 3-tap warm tint `(1.0, 0.92, 0.8)` on the
  bloom term only. Check L5: daylight mean luminance with bloom on/off differs under
  4%; white text and clothing show no halo. Files: `shaders/bright.frag`,
  `shaders/final.frag` (tint), `hd2d.c` (bind the light map to the bloom pass in the 3D
  path, where `uLightMap` is a different space: `scene3d.frag` samples it at console
  screen coordinates).

### 2.6 Colour grade

- **Commercial:** per-map PostProcessVolume tuning; per-map brightness lift so dark
  areas keep sprite color [verified: source read, both talks]. Earlier HD-2D leaned on
  "shallow color" [verified: source read, Historia summary].
- **Repo today:** one grade for all maps: exposure 1.06, shoulder at 0.7, saturation
  1.12, contrast 1.04, split tone 0.35, vignette 0.35, grain 0.018 [verified: source
  read, `final.frag` and `hd2d.c`]. The shoulder is a global soft roll-off; no per-scene
  adjustment except the darkness measure that drives lights.
- **Change:** a **dark-scene lift**: when the scene darkness `d` (0..1) exceeds 0.6, add
  `lift = 0.04 * d` to the shadows before contrast (`c = c + lift * (1 - c)^2`) so caves
  keep readable sprite colors, and skip split-tone shadow tint (cool) beyond 50% so dark
  interiors do not go blue. Keep black rule F2: a console black stays under 2/255, so
  the lift must be 0 at exact zero (the `(1-c)^2` term does not satisfy that; use
  `lift * c * (1-c)` instead). Check F2, L11 (mean luminance within +-15% of off).
  Files: `shaders/final.frag` `grade()`, `hd2d.c` (pass the darkness).

### 2.7 Water

- **Commercial:** the Octopath article says the team weighed pixel vs realistic water
  [verified: source read, Wikipedia via search]; Octopath sea uses dot-style visuals
  [verified: source read, Acquire deck]. No technical water description exists
  publicly [unverified].
- **Repo today:** world-map water (`ni == 7`): swell +-3.5%, 3x3 console-px glints,
  seed-hash twinkle, no AO [verified: source read]. Field water: none.
- **Change:** a field water cell class exists only if the module marks it; if so reuse
  the same function from `scene3d.frag` plus a fake reflection: sample the already
  rendered picture mirrored about the shore line, 15% mix, with a 3 px horizontal wave
  displacement that steps (not smooth) at 8 Hz so it stays on the pixel grid
  [unverified values]. Needs a mask output; a safe cheap first step is only the swell and
  glints on cells the module tags. Check L14 (under 1% lit) and no motion faster than 4 Hz.

### 2.8 Fog and haze

- **Commercial:** volumetric fog and light shafts are in the HD-2D stack per Wikipedia
  and Godoiosis [verified: source read]; Live A Live models fog on stage effects
  (theatrical, "stage-like") [verified: source read]. Values unknown.
- **Repo today:** distance haze in `light.frag` for far planes and
  `mix(lit, uFog.rgb, uFog.a*smoothstep(haze start, end))` in `scene3d.frag`; no height
  term, no scattering [verified: source read].
- **Change:** add height fog by multiplying the amount with `exp(-max(vWorld.y - 0,0) *
  0.02)` so low ground hazes more than roofs, and a fixed screen-space light-shaft pass
  from the emit map only in interiors (up to 8 taps toward each bright window, strength
  0.15, off by default on deck) [unverified values]. Check L12 (<= 25% at far edge, 0 at
  the player's row).

### 2.9 Camera angle and FOV

- **Commercial:** Live A Live: camera top-down angled, a **telephoto lens** to avoid edge
  distortion [verified: source read]. Octopath battle: slightly angled, subtle sway
  [verified: source read]. This matches VISUAL-DESIGN's narrow FOV. Numbers: none; the
  25-35 / 35-45 range is the Godoiosis author's [verified: source read], a hobbyist.
- **Repo today:** tilt 40, FOV 30, zoom 0.75, user and gyro offsets clamped
  [verified: source read].
- **Change:** none. **Do not add the battle sway**; VISUAL-DESIGN "what not to do"
  forbids camera motion we invent, and nothing shows it helps the picture. If tests
  want the feel, a 0.5 degree sinusoid at 0.1 Hz is the most I would try [unverified].

### 2.10 Pixel density rules

- **Commercial:** one pixel density across the scene: Octopath interiors are built at
  half scale so sprites read larger [verified: source read]; Triangle Strategy shapes
  proportions to keep pixels big [verified: source read]; community: nearest filter,
  tile 32x32 or 48x48, one density [verified: source read].
- **Repo today:** world-map sprites snapped; field billboards **not** snapped; no integer
  zoom (stated in VISUAL-DESIGN) [verified: source read].
- **Change:** this is the top item below. Concretely: B = round(scale at the foot row);
  place the sprite quad corner at `floor(x*B + .5)/B`; pick zoom = `B / (outputHeight/224)`
  where B is the integer nearest the current value. Add a half-pixel clamp. Check S3, L16.

### 2.11 Transitions

- **Commercial:** a door trigger swaps lights, post effects, visible objects, camera
  distance and character size in one go [verified: source read]; aperture change on
  scene transitions [verified: source read]; loading starts from blackout [verified:
  source read].
- **Repo today:** 8-frame crossfade between valid pictures; scene gate; fades follow
  console brightness [verified: source read, VISUAL-DESIGN].
- **Change:** an **aperture ease**: during the crossfade ramp `dof_radius` from 0 to
  target over 12 frames (starting sharp, "focus pull") instead of snapping DOF on after
  the swap. Check T2 (no flash) and that no frame has DOF applied to a UI frame (U3).
  Cheap: one uniform ramp in `hd2d.c`.

### 2.12 Background fade where a character is hidden

- Live A Live fades background pieces when a character overlaps them [verified: source
  read]. The console already solves occlusion by priority; our foreground class (roofs,
  arches) hides the player when he walks under a canopy [unverified that this is a
  problem in this game]. A generic fix: where an actor pixel is *behind* a foreground
  pixel by the G-buffer (`oMat.a` z flag), draw the foreground at 70% alpha within 24
  console px of the actor. Check S4, and the player is visible under roofs. Files:
  `shaders/light.frag` / `final.frag`.

## 3. Things the repo already has that the commercial games do not document

Not a recommendation to drop them, only a note so nobody "fixes" them to match a
source that does not exist: the behind buffer and foreground/background split in the DOF
(prevents halos on arches), console-resolution lighting pass, the scene gate and the
crossfade rules. These are our solutions to the fixed-ROM problem.

## 4. Ranked list (max 10)

1. **Integer zoom and snapped field billboards.** Files: `scene3d_world.c`
   (`s3d_build_world_sprites`, around line 539), `scene3d.c` (camera zoom),
   `hd2d.c` (`user_zoom`). Acceptance: S3 and L16 pass; sprite pixels all the same size
   at 1920x1080 and 1038x556. This is the "one density" rule and the highest-impact
   amateur tell.
2. **Context-dependent DOF and vignette** (battle low DOF, vignette 0.2 in 3D). Files:
   `hd2d.c` presets, `shaders/final.frag`, `shaders/coc.frag`. Acceptance: L1-L3 pass,
   L13 updated to 10-30%, battle frame debug view 3 below 0.3.
3. **Blob shadow for indoor/dark scenes**, shadow map outdoors. Files:
   `scene3d_world.c`, `scene3d.frag`, `hd2d.c`. Acceptance: L8 passes in an interior,
   no acne, no swim (L16), GPU time not higher than before.
4. **Bloom gated by the light mask**, threshold 0.75, warm tint. Files:
   `shaders/bright.frag`, `shaders/final.frag`, `hd2d.c`. Acceptance: L5 (daylight
   change under 4%; no halo on white text, clothing or sand).
5. **Dark-scene lift and no cool tint in deep shadow.** Files: `shaders/final.frag`
   `grade()`, `hd2d.c`. Acceptance: F2 (blacks), L11 (mean luminance within +-15%),
   hero palette hue within 8 degrees.
6. **Sprite fake-normal bulge and rim in 3D** (point-light direction only). Files:
   `shaders/scene3d.frag`. Acceptance: sprite beside a torch brighter on the torch side
   by 10-20%, daylight sprite within 3% of today, no outline (L9, L10).
7. **Disc-shaped DOF gather above radius 4 px.** Files: `shaders/dofblur.frag`,
   `hd2d.c`. Acceptance: L4 no halo; a lamp becomes a soft disc; perf within +0.3 ms
   (`CT_HD2D_PROFILE`).
8. **Aperture ease on transitions.** Files: `hd2d.c`. Acceptance: T2, U3.
9. **Height fog and interior light shafts (off by default on deck).** Files:
   `shaders/scene3d.frag`, `shaders/light.frag`, `hd2d.c`. Acceptance: L12; interior
   mean luminance within L11 bounds; shafts under 8 taps.
10. **Field water swell and glints** on module-tagged cells; reflection later.
    Files: `shaders/scene3d.frag`, `scene3d_world.c`, ct_scene tag. Acceptance: L14.

Skipped on purpose: camera sway (forbidden by "what not to do"), foreground fade under
roofs (needs a problem report before building, item 2.12), chromatic aberration (the
Acquire deck uses it only on a battle event [verified: source read]; our rule is not to
add it).

## 5. Sources

- https://www.docswell.com/s/EpicGamesJapan/KXPWY5-UE4_UFE2018_ACQUIRE_OCTOPATHTRAVELER
- https://docswell.com/s/EpicGamesJapan/Z32GJ5-UE_UFE22W_SQEX_Historia
- https://en.wikipedia.org/wiki/HD-2D
- https://www.nintendolife.com/news/2022/05/triangle-strategy-producers-talk-hd-2d-and-why-other-devs-havent-used-it
- https://blog.playstation.com/2024/11/08/dragon-quest-iii-hd-2d-remake-interview-how-the-classic-rpg-was-revamped-for-its-ps5-debut/
- https://github.com/Phaazoid/Godoiosis/issues/176
- https://dev.to/gaurav_de/creating-an-hd-2d-rendering-pipeline-on-dx12-205k
- Unfetched (403): https://www.unrealengine.com/en-US/developer-interviews/octopath-traveler-ii-builds-a-bigger-bolder-world-in-its-stunning-hd-2d-style

Licence note: all items above are ideas and goals from talks and articles; no code or
assets were taken. The hobby pipeline's math (CoC, golden-angle spiral) is standard
textbook technique, rewritten here from scratch.
