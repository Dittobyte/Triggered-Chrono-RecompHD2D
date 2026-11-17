<!--
SPDX-FileCopyrightText: 2026 jerbanpls
SPDX-License-Identifier: GPL-3.0-or-later
-->
# R4: native controller support (Nintendo, Xbox, PlayStation, Steam Controller 2026, Deck)

Read-only. Repo = Triggered-Chrono-RecompHD2D on main (8f5874a). SDL claims were checked by fetching source at the
tags named; `frontend` paths are in the game repo. "unverified" = not seen in code or docs by me. Greps for feature
presence counted calls in `src/joystick/hidapi/SDL_hidapi_<driver>.c` per tag, not behaviour; no hardware was used.

Tags: 2.30 = release-2.30.0, 2.32 = release-2.32.10, SDL3 = release-3.4.0 and release-3.4.20 (HANDHELD.md says the
dev box runs sdl2-compat 2.32.74 over SDL 3.4.20; I did not verify the package, only the tag exists and has the files).

## 0. Where the game stands (all families)

- Hints (`frontend/input.c:157-167`): HIDAPI_STEAMDECK=1 always, HIDAPI_STEAM=1 unless `steam_hidapi=off`,
  GAMECONTROLLER_USE_BUTTON_LABELS=0 (Nintendo read by position). All other drivers (PS4/PS5, Switch, Xbox, GameCube)
  are left to SDL's own defaults (HIDAPI default on, `SDL_HIDAPI_DEFAULT`, seen in the drivers' IsEnabled).
- Init: `SDL_INIT_GAMECONTROLLER` only (`main.c:3306`); polled with the SDL_GameController API, one `pad`, `pad2`, `pad3`
  (`main.c:363-364`), buttons via `SDL_GameControllerGetButton` (`main.c:318-325`), mapped to SNES bits in `input.c:22`.
- Used SDL features: rumble + trigger rumble (`rumble_out.c:71-92`, `:195-198`), gyro on pad 1 only
  (`main.c:1914-1930`, `gyro_cam_step` `input.c:176`, drives the HD-2D camera up to 8 deg), controller type/VID/PID
  for glyphs (`glyphs.c:164-169`), paddle1-4/misc1/touchpad-click as bindable actions (`bindings.c:9`, `glyphs.c:31-37`).
- NOT used anywhere: touchpad finger API (`SDL_GameControllerGetTouchpadFinger`), LED (`SDL_GameControllerSetLED`),
  `SDL_GameControllerSendEffect` (adaptive triggers/lightbar), player index (`SetPlayerIndex`), battery,
  `SDL_SENSOR_ACCEL`, trackpad as pointer (grep of frontend/*.c: only the `BUTTON_TOUCHPAD` click is referenced;
  `touch.c` is the Android touch overlay, not controllers).
- Glyph families (`glyphs.h:25-33`): keyboard, Xbox, PlayStation, Switch, Steam Deck, Steam Controller (old 2015 pad).
  Per-pad glyph choice is global "device last used" (`glyphs_use`, `main.c:1093`), not per player.
- Own hidraw reader: `hid_deck.c` (797 lines) reads only Deck-format 64-byte reports (28de, vendor-defined descriptor), feeds
  an `SDL_JoystickAttachVirtualEx` pad (`hid_deck.c:565-576`), rumble via hidraw feature report; no gyro, no pads/touch.
- Tests: `test_actions.c:79-125` (virtual pads with VID/PID, `SDL_JoystickSetVirtualButton`), `test_handheld.c`,
  `test_hid_deck.c` (fake sysfs + FIFO, made-up report bytes, `hid_deck_parse` per bit), `test_rumble.c`, `test_input.c`.

## 1. Nintendo (Switch Pro, Joy-Con pair, GameCube adapter)

1. SDL today. Switch driver `SDL_hidapi_switch.c` exists in 2.30, 2.32 and 3.4. Counted calls in all three tags:
   gyro/accel sensors (AddSensor 6), SetLED, RumbleJoystickTriggers present (the HD-rumble driver; not a trigger motor on
   Switch), paddles code present (Switch driver handles Joy-Con SL/SR as paddles; "paddle" mentions 8 in 2.x, 20 in 3.4).
   Hints present in both 2.x tags' `SDL_hints.h`: HIDAPI_SWITCH, SWITCH_HOME_LED, SWITCH_PLAYER_LED, JOY_CONS,
   COMBINE_JOY_CONS, VERTICAL_JOY_CONS, JOYCON_HOME_LED, HIDAPI_GAMECUBE, NINTENDO_CLASSIC. Joy-Con pair combining:
   `SDL_hidapi_switch.c` 2.32.10 has "Use the right sensor in the combined Joy-Con pair" (line 1485). 3.4.0 adds
   HIDAPI_SWITCH2 and `SDL_hidapi_switch2.c` (the 404 on 2.30/2.32 means no Switch 2 pad driver there). Capture button =
   MISC1 on Pro. No touchpad, no adaptive triggers.
   GameCube adapter: `SDL_hidapi_gamecube.c` present in all tags (hint HIDAPI_GAMECUBE, 3.4 adds GAMECUBE_RUMBLE_BRAKE).
   It writes an init report with `SDL_hid_write` (2.32.10 line 169), so the hidraw node must be writable. Rumble only.
   Whether 2.30 needs libusb for it: unverified (2.32.10 file has no libusb text).
2. Game today. Works as a generic pad (HIDAPI default on); position mapping via the label hint; glyph set Switch
   (`glyphs.c:131-137`, type PRO/JOYCON_*, or vendor 0x057e `glyphs.c:200`). Rumble via the generic call. Gyro menu
   option exists and would work if SDL enabled the sensor (`SetSensorEnabled`).
3. Gap. Joy-Con L+R pair as one pad needs the right hint (not set; SDL default for COMBINE: unverified, check the
   hints file) and otherwise appears as two half pads (glyphs.c has LEFT/RIGHT types, so a single Joy-Con shows Switch
   glyphs but with half the buttons); player LEDs for co-op (player index to LED: SDL does this itself with
   SWITCH_PLAYER_LED, nothing in game sets the index); gamecube adapter: glyph set missing (falls to vendor 0x057e =
   Switch glyphs, wrong: GC has A/B/X/Y/Z/L/R with different shapes, no art in Kenney set: unverified); capture/home
   button action; gyro on Pro for aim/menu beyond camera tilt.
4. Options. (a) Rely on SDL (cost S: set hints, add a "GameCube" glyph family with text labels, player index).
   (b) SDL3 API path: nothing new for Nintendo beyond Switch 2 pad driver (3.4.0 only). (c) Own hidraw reader: not
   worth it, Switch protocol is large (subcommands, HD rumble); copying from SDL's zlib driver is licence-ok but a big
   maintenance cost for no gain. Recommend (a).
5. Perms. Pro over USB and Bluetooth both go via hidraw; needs a udev rule granting the seat user read/write
   (uaccess); distros with systemd ship `70-joystick`/`73-seat-late` rules for some, Valve's
   `60-steam-input.rules` (steam-devices package, github.com/ValveSoftware/steam-devices) lists Nintendo VIDs
   (listed in that file; contents unverified by me this session). Without hidraw SDL falls back to evdev: buttons work,
   no gyro, no LED, no Joy-Con pair. GameCube adapter: needs hidraw rw (USB only, mode switch by init write).
6. Tests. Virtual pad with VID 057e / PID 2009 / type: hint `SDL_GAMECONTROLLERTYPE`-style override unavailable in 2.x
   (3.x has `SDL_HINT_GAMECONTROLLERTYPE`); in 2.x use the VID/PID the test passes to `SDL_JoystickAttachVirtualEx`
   (as `test_actions.c:84-90`) and check `glyphs_family`. Made-up report fixtures only if we write our own parser.
   Hardware only: Joy-Con pair merge, HD-rumble feel, GC adapter init, player LED, drift/gyro calibration.

## 2. Xbox (One/Series, Elite paddles, trigger rumble)

1. SDL today. 2.30/2.32: `SDL_hidapi_xboxone.c` (USB GIP; Elite paddle code, "paddle" 89 mentions, share button),
   `SDL_hidapi_xbox360.c` (+ wireless, player LED), RumbleJoystickTriggers present in both: impulse-trigger motors work
   (`SDL_GameControllerRumbleTriggers`, API since 2.0.14). Bluetooth Xbox pads go through the Linux kernel `xpadneo` /
   `hid-microsoft` or plain evdev: in 3.4 the GIP driver is `SDL_hidapi_gip.c` (new, 3.4.0) and hints HIDAPI_GIP,
   HIDAPI_GIP_RESET_FOR_METADATA exist. No gyro, no touchpad, no LED beyond Xbox home LED hint.
2. Game today. `rumble_out.c:90-92` sends main motors plus `lo/3` to both triggers when
   `SDL_GameControllerHasRumbleTriggers` (good). Glyph set Xbox is the fallback for anything unknown (`glyphs.c:204`).
   Paddles are bindable; Xbox glyph has no paddle pictures, written "P1..P4" (`glyphs.c:75`); order P1,P3,P2,P4 is
   arbitrary and should be checked on a real Elite: unverified.
3. Gap. Trigger rumble as its own effect (left trigger on a hit to the left, right trigger on a confirm): the game only
   couples triggers to the heavy motor. Elite paddle mapping only seen via SDL's mapping (works when the pad is the
   Series 2 Elite over USB, Bluetooth gives no paddles on some firmwares: unverified). Share button as MISC1 is bindable.
4. Options. Rely on SDL: complete for a player. SDL3-API path adds nothing material. Own reader: no.
5. Perms. USB GIP via hidraw/libusb (libusb is optional in SDL: unverified here). `xpad` kernel driver gives evdev for
   360/One without any udev rule; Bluetooth via `xpadneo` or `hid-microsoft`. Flatpak `--device=all` already there.
6. Tests. Virtual pad VID 045e (existing patterns); rumble-trigger call needs `HasRumbleTriggers` true, which a SDL
   virtual joystick does not offer in 2.x unless the desc sets `Rumble`+`RumbleTriggers` callbacks (the VirtualJoystickDesc
   has `RumbleTriggers`, 2.0.14+; the game's own `hid_deck.c` sets only `Rumble`). Hardware only: trigger motor feel,
   Elite paddle order, Bluetooth quirks.

## 3. PlayStation (DualShock 4, DualSense)

1. SDL today. 2.30 and 2.32: `SDL_hidapi_ps4.c`, `SDL_hidapi_ps5.c`. Counted in both tags: gyro/accel (AddSensor 2 and 4),
   one touchpad (`PrivateJoystickAddTouchpad` 1 each; API `SDL_GameControllerGetNumTouchpads`,
   `GetTouchpadFinger`, 2.0.14), `SDL_GameControllerSetLED` (lightbar) both, `SendEffect` both (PS5 adaptive-trigger
   and haptic effect structs, `SDL_PS5_` in the 2.x header `SDL_gamecontroller.h`: unverified header contents; driver
   code counts 4 `SendJoystickEffect` mentions), rumble + `RumbleTriggers`, hints PS5_PLAYER_LED, PS4_RUMBLE, PS5_RUMBLE
   (2.x). 3.4.0: same plus `SendJoystickTouchpad` (4 in ps5) and 6 effect mentions, HIDAPI_PS4_REPORT_INTERVAL,
   `PS3_SIXAXIS_DRIVER`, `SDL_HINT_GAMECONTROLLER_SENSOR_FUSION`. DualSense haptics (audio-style) are NOT exposed by SDL
   as such: only the two rumble motors; real haptics need SendEffect or a raw output report (unverified, not in the
   2.x public API). Adaptive triggers: via `SDL_GameControllerSendEffect` with Sony's raw 11-byte per-trigger effect.
   Touchpad click = `BUTTON_TOUCHPAD`; mic mute = MISC1.
2. Game today. Rumble, gyro (when pad 1), glyph PlayStation with Cross/Circle/Sq/Triangle, Create/Options, touchpad
   glyph (`glyphs.c:84-95`); L2/R2 glyphs; PS button "PS" text. No touchpad finger use, no LED, no player index.
3. Gap, for a player. Lightbar colour as co-op player colour or party-member colour (SetLED, one call, S);
   touchpad swipe as menu pointer or scroll (fingers API, M); gyro menu / aim (already half there); adaptive triggers
   and DualSense haptics are flavour only (M to L, PS5-only, raw bytes). Gyro only enabled for pad 1.
4. Options. (a) Rely on SDL: LED, touchpad, sensor all available today at 2.30+: ship a small `pad_extras.c` using
   `SDL_GameControllerHasLED/SetLED`, `GetTouchpadFinger`, `SetPlayerIndex`; no new licence. (b) SDL3 API: no gain
   unless sdl2-compat lacks a call: unverified. (c) Own hidraw reader (e.g. for haptics): large; Sony's report format is
   in SDL's zlib ps5 driver (would need zlib notice in the file as `hid_deck.c` does) and risks racing SDL for the device
   (two readers on one hidraw node: DS4/DS5 reports go to every open fd, but output reports conflict). Recommend (a).
5. Perms. USB and Bluetooth both via hidraw; kernel `hid-playstation` gives evdev for buttons/touchpad/motion as
   separate nodes (SDL's evdev path then sees the three nodes; SDL's hidapi path is the one with LED/effects and
   needs hidraw access). Needs udev rule (steam-devices or a `uaccess` rule). Bluetooth: pairing and hidraw mode
   with `hid-playstation`: unverified specifics.
6. Tests. Virtual joystick supports `ntouchpads`, `nsensors`, `SetVirtualJoystickTouchpad`, `SetVirtualJoystickSensor...`,
   `SetLED` callback on the desc in 2.0.14+ (descriptor fields `ntouchpads`, `nsensors`, `SetLED` in
   `SDL_VirtualJoystickDesc`: unverified for 2.30 header, check `SDL_joystick.h`). Hardware only: lightbar visibility,
   gyro bias/drift, effect bytes, haptics feel, Bluetooth latency.

## 4. Steam Controller 2026 (Triton; 28de:1302 USB, 1303 BLE, 1304/1305 dongles)

1. SDL today. IDs in `controller_list.h` at release-3.4.0 (lines 605-608): 1302 Triton, 1303 Triton BLE, 1304 Proteus
   dongle, 1305 Nereid dongle. Driver `src/joystick/hidapi/SDL_hidapi_steam_triton.c` exists at release-3.4.0 (533
   lines) and release-3.4.20 (711). It is NOT in release-2.32.10: `hj.c`/directory listing has no triton and
   `controller_list` of 2.32.10 has no 1302 (grep returned nothing); `SDL_hidapi_steam.c` of 2.x is the 2015 Steam
   Controller (hint STEAM default false, `SDL_hidapi_steam.c:962`).
   3.4.0 Triton driver: IsEnabled = HIDAPI_STEAM hint, else HIDAPI default (line 293). Buttons (lines 40-44, 137-171):
   A/B/X/Y, shoulders, back/start/guide, sticks, QAM = MISC1-like button, four grips as `TRITON_RIGHT_PADDLE1`,
   `LEFT_PADDLE1`, `RIGHT_PADDLE2`, `LEFT_PADDLE2`; triggers analogue; D-pad as hat; gyro AND accel
   (`SDL_SendJoystickSensor`, 214-219); simple haptic rumble (`HAPTIC_RUMBLE`, line 438); `RumbleJoystickTriggers`,
   `SetJoystickLED`, `SendJoystickEffect` return `SDL_Unsupported()` (lines 454-470); battery status; lizard mode
   disabled by the driver (line 363, "watchdog re-enables"). 3.4.0: 0 trackpad mentions: trackpads NOT exposed.
   3.4.20: adds `SDL_PrivateJoystickAddTouchpad` x2 (lines 588-589: two touchpads, one finger each),
   `SendJoystickTouchpad` x4, and extra buttons `RIGHT_TOUCHPAD`, `LEFT_TOUCHPAD` (click), `*_JOYSTICK_TOUCH`,
   `*_GRIP_TOUCH` (lines 53-58). So: trackpads, grips and gyro are available from SDL 3.4.20 (and not 3.4.0: the
   touchpads appear between the two tags; exact first tag unverified). SDL 2.x: none of it. sdl2-compat maps SDL2's
   `SDL_GameControllerGetTouchpadFinger` to SDL3's (sdl2-compat behaviour: unverified; its button numbering of the
   extra Triton buttons beyond SDL2's PADDLE1-4/MISC1/TOUCHPAD range is likely lost: unverified).
2. Game today. `glyphs.c:112-116` treats 1302-1305 as Steam Deck glyphs (L4/L5/R4/R5, `steamdeck_*` art), reasonable.
   `hid_deck.c` does not take it: `hid_deck_product_ok` accepts any 28de in mode on (`hid_deck.c:183`) but the
   report descriptor / type 9 check and 64-byte Deck layout (`hid_deck_parse`) would reject it (Triton report format is
   different: `TRITON_LBUTTON_*` bits, own struct; not the Deck's). So only SDL's driver (HIDAPI_STEAM=1 hint) or
   Steam Input can supply it. HANDHELD.md:200-206 already says this correctly.
3. Gap. Trackpads as menu pointer / scroll (needs SDL 3.4.20 path or own reader); two touch-surfaces as extra
   buttons; gyro aim; grips as the four back actions (SDL gives them when the driver is there); haptics beyond
   simple rumble (Valve's HD haptics: protocol not in SDL beyond `HAPTIC_RUMBLE` type 0: unverified elsewhere);
   LED (Triton has lights: driver returns unsupported: unverified whether the hardware has any addressable LED).
4. Options.
   (a) Rely on SDL 3.4.20+ via sdl2-compat or direct SDL3: nothing to write beyond reading touchpad fingers and the
       extra buttons. Risk: AppImage must ship that SDL.
   (b) Port to SDL3 API (S for the project's input layer only if the game links SDL3 directly; the game is SDL2
       throughout: L if all frontend moved, so not for this feature).
   (c) Own hidraw reader like `hid_deck.c` for Triton (Valve's report struct from `SDL_hidapi_steam_triton.c`, zlib,
       Copyright Valve; structs likely in `steam/controller_structs.h`: check). Works with SDL 2.30/2.32 AppImage
       and Flatpak runtime; gives trackpads + gyro + grips without waiting for SDL. Cost M (parser + sensors/touch
       virtual joystick; `SDL_JoystickAttachVirtualEx` desc has `nsensors`/`ntouchpads` since 2.0.14: unverified in
       2.30 header). Risk: two readers on the hidraw node, "lizard mode" (the pad emulates keyboard/mouse until the
       app sends the disable report: SDL sends it, `DisableSteamTritonLizardMode` line 107; our reader must too or
       the desktop sees phantom mouse/keys while playing), dongle pairing and BLE not tested. A write is needed for
       lizard off, which hid_deck.c deliberately avoids ("Nothing else is ever written", HANDHELD.md:275).
   Licence for (c): the code is zlib (Valve copyright in the structs, SDL Authors): copy only offsets and constants as
   `hid_deck.c` did, keep the zlib notice block in the file header, set `SPDX GPL-3.0-or-later AND Zlib` in the
   file and a `[[annotations]]` path entry in `REUSE.toml` (existing model: REUSE.toml:177-184), add a paragraph to
   `THIRD_PARTY.md` naming the SDL tag and files (existing model: THIRD_PARTY.md:39-51), `LICENSES/Zlib.txt` is
   already present. Mark the file "altered" (zlib clause 2). No game data involved.
5. Perms. Valve's `60-steam-input.rules` (steam-devices) grants uaccess to 28de:* hidraw including these PIDs only if
   the installed copy is new enough: whether it lists 1302-1305: unverified. Dongles present both a hidraw per
   interface; BLE via hidraw over hog (`uhid`), needs `bluez` input plugin. Flatpak: `--device=all`
   (`packaging/flatpak/...yml:27`) exposes hidraw. AppImage: runs on host, udev rules on host.
6. Tests. A synthetic Triton report fixture authored by us (bit positions from the driver's constants) fed through
   a new `triton_parse` in a test like `test_hid_deck.c` (fake sysfs + FIFO); virtual pad VID 28de PID 1302 for glyph
   checks (the existing `test_actions.c` pattern). Hardware only: trackpad coordinate range/feel, lizard mode,
   dongle and BLE, grips, gyro scale, haptics, battery.

## 5. Steam Deck

1. SDL today. 2.30 and 2.32: `SDL_hidapi_steamdeck.c` (Max Maisel, 2023): VID/PID 28de:1205, gyro and accel,
   four back buttons as PADDLE1-4, QAM as MISC1, simple rumble. Hint HIDAPI_STEAMDECK. No trackpads in 2.x: the
   driver has no `AddTouchpad` (counted 0). 3.4.0 adds 2 `AddTouchpad` and `SendJoystickTouchpad` to this driver.
2. Game today. `input.c:159` sets the hint; glyph set Steam Deck with L4/R4/L5/R5 and QAM `...` (`glyphs.c:97-106`);
   back-button actions (HANDHELD.md:144-183); gyro; InputPlumber reader (`hid_deck.c`) for the Ally/Legion class.
   Under Steam Input the pad is SDL type of the virtual pad (28de:11ff).
3. Gap. Trackpad as pointer in menus (needs 3.4.x path, or own parser in `hid_deck.c`: the Deck report already
   carries pad x/y at bytes 16-22 and pressures at 56-58, see `hid_deck.c` comment lines 69-77: unused today),
   gyro beyond the camera, haptic (trackpad) feedback. Under Steam Input all of these are Steam's, not ours.
4. Options. (a) Rely on SDL 2.x: Deck works already (buttons, gyro, rumble). (b) Extend `hid_deck.c` to expose the two
   pad axes and pressures as virtual axes/touchpad: S to M, and it is the one new feature that works with the 2.32
   AppImage. (c) SDL 3.4.x touchpads. Recommend (b) because the report layout is already ported.
5. Perms. On SteamOS the rules are installed. Desktop Mode launches already work. Flatpak/AppImage as above.
6. Tests. `test_hid_deck.c` pattern (made-up 64-byte report with pad bytes set). Hardware only: pad feel, Steam's
   lizard desktop conflicts (HANDHELD.md:196-197).

## 6. Summary table (X = yes in code at that tag, - = no, ? = unverified)
| Capability | Nintendo | Xbox | PlayStation | Steam Ctrl 2026 | Deck |
| --- | --- | --- | --- | --- | --- |
| SDL 2.30 / 2.32 driver | X / X | X / X | X / X | - / - | X (1205) / X |
| SDL3 3.4.0 / 3.4.20 | X / X (+Switch2) | X (GIP) / X | X / X | X (no pads) / X (pads) | X / X (+pads) |
| Gyro (API) | X Pro, Joy-Con | - | X | 3.x only | X |
| Touchpad (API) | - | - | X (1) | 3.4.20 only | 3.4 only |
| LED / lightbar | X (player/home) | X (home) | X lightbar | - (unsupported) | - |
| Rumble / trigger rumble | X / X (HD, ?) | X / X | X / X | X simple / - | X / - |
| Adaptive triggers | - | - | X raw SendEffect | - | - |
| Paddles / grips | Joy-Con SL/SR | Elite X | - (DualSense Edge ?) | 4 grips (3.x) | 4 (X) |
| Game uses it today | rumble, glyph | rumble, trig rumble, glyph | rumble, gyro, glyph | glyph only (Deck art) | rumble, gyro, back btns, glyph |
| Own hidraw reader needed? | no | no | no | only for 2.x runtimes | for pads: extend hid_deck.c |

## 7. Packaging and the AppImage SDL question

- Flatpak: manifest builds SDL2 2.32.10 (`docs/PACKAGING.md:140-143`), `--device=all` (yml:27). 2.32.10 has no Triton.
- AppImage: `packaging/appimage/build.sh` has `SDL2=system` (default: copies the build host's libSDL2, and libSDL3 when it
  is sdl2-compat, lines 118-131) or `SDL2=build` (2.32.10 tarball, SHA-pinned, lines 51-52, 90-101). The tag workflow
  (`.github/workflows/port.yml:99-113`) runs on `ubuntu-latest` with `libsdl2-dev` and default SDL2=system, so the CI
  AppImage ships the runner's libSDL2 (WORKFLOW.md says CI's SDL is 2.30): Ubuntu's 2.30.0 has no Triton, no Switch 2.
  A locally built AppImage on Fedora ships sdl2-compat + libSDL3 3.4.20 (per HANDHELD.md:200), which has Triton
  buttons/gyro/touchpads. So: does the shipped AppImage know the 2026 Steam Controller? CI AppImage: no (2.30 or 2.32.10);
  a Fedora-host build: yes (3.4.20), unverified on hardware. HANDHELD.md only documents the `SDL2=build` case.
- Decision for the owner: pin one SDL for releases. Options: keep 2.32.10 and add our own Triton reader (offline,
  deterministic, M), or bundle SDL3 3.4.20 + sdl2-compat 2.32.74 via the build script (S to M, gets Triton + Switch 2
  + new GIP, but sdl2-compat is a second layer with its own bugs; relicense/notice: zlib for both, add to THIRD_PARTY.md
  because the AppImage redistributes SDL binaries: check whether the existing entry for the bundled SDL exists, I found
  no SDL binary entry in THIRD_PARTY.md, only the hid_deck.c port: needs a line either way).
- udev: ship nothing ourselves; document `steam-devices` or Steam's `60-steam-input.rules` and, for non-Steam users,
  a small optional rule file in docs (`TAG+="uaccess"` for 28de/054c/057e/045e hidraw). The file contents were not
  re-read from Valve's repo here: write from the documented fields, do not copy text.

## 8. What can be tested where

- No hardware: glyph family by VID/PID (`test_actions.c` pattern), button routing and actions, binder, gyro_cam_step,
  report parsers (made-up buffers, FIFO and fake sysfs as `test_hid_deck.c`), LED/touch/sensor code paths via virtual
  joystick descriptors (needs the 2.30 header check on `ntouchpads`, `nsensors`, `SetLED`, `RumbleTriggers`;
  CI runs 2.30 and the box 2.32, see WORKFLOW.md gate 4: any call missing in 2.30 breaks CI only).
- Hardware only: feel (rumble, haptics, adaptive triggers), gyro calibration, trackpad range, pairing, dongles, BLE,
  lizard mode, hotplug, battery, real VID/PID quirks (Joy-Con pair, Elite paddles order), permissions on the real OS.

## 9. Recommended milestones

| # | Milestone | Size | Who |
| --- | --- | --- | --- |
| P1 | Set hints (PS5 player LED, switch combine/vertical Joy-Con, player index per pad), `SDL_GameControllerSetPlayerIndex` and `SetLED` for co-op colours | S | Claude 2 |
| P2 | GameCube glyph family (text labels) + Nintendo family tests; `glyphs_family` rows for new VID/PIDs | S | Claude 2 |
| P3 | Per-pad glyphs per player (not global "last used") in co-op messages | M | Sonnet agent |
| P4 | Touchpad/trackpad pointer in the menu via SDL touchpad API (PS4/PS5 now, Triton/Deck with SDL3 3.4.20) behind a setting, with virtual-joystick test | M | Sonnet agent |
| P5 | Gyro: enable for any pad, accel, menu use, calibration; settings per pad | M | Sonnet agent |
| P6 | Decide the release SDL (pin 2.32.10 vs bundle SDL3 3.4.20 + sdl2-compat) and AppImage/CI alignment; THIRD_PARTY entry for bundled SDL | S (decision) + S/M (build) | Owner then Claude 2 |
| P7 | Triton hidraw reader `hid_triton.c` feeding a virtual pad (grips, gyro, trackpads, lizard-off write) with zlib notices; only if P6 keeps SDL 2.x | L | Sonnet agent (Kimi for fixture tests) |
| P8 | Deck trackpad axes in `hid_deck.c` | S to M | Sonnet agent or Kimi |
| P9 | Trigger rumble effects (left/right separated), Xbox/PS tuning | S to M | Kimi |
| P10 | PS5 adaptive triggers / haptics | L, optional, low value | skip until asked |
| P11 | udev rules doc, `docs/HANDHELD.md` per-family section | S | Claude 2 |

Order: P1, P2, P11 (cheap), P6, then P4/P5, then P7 or the SDL3 bundle, then P3, P8, P9. P10 not planned.
