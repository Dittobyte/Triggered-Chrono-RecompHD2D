<!--
SPDX-FileCopyrightText: 2026 jerbanpls
SPDX-License-Identifier: GPL-3.0-or-later
-->
# Steam Deck and ROG Ally

This page walks through playing the port on a **Steam Deck** or an **ASUS ROG Ally**
running **Bazzite** (or SteamOS on the Deck), started from Steam's **Game Mode**. You
need no keyboard after setup. Everything else, such as the legal note and the full
option list, is in the [player's guide](USER-GUIDE.md).

## 1. Get the AppImage

In **Desktop Mode**, either:

- download `CT_Recomp-x86_64.AppImage` from a release into `~/Applications` (create the
  folder if needed), or
- build it from a checkout with `packaging/appimage/build.sh` (see
  [PACKAGING.md](PACKAGING.md)). The result is `build-appimage/CT_Recomp-x86_64.AppImage`.
  Copy it to `~/Applications`.

Then make it executable, in Konsole:

```
chmod +x ~/Applications/CT_Recomp-x86_64.AppImage
```

## 2. Put your ROM in ~/Documents

Copy your own dump of the US 1.0 cartridge to `~/Documents` (a subfolder such as
`~/Documents/roms` works too). The engine also looks in `~/Downloads`, in the folder the
AppImage is in, and in its own data folder, `~/.local/share/ct-recomp/ct/`.

The file must be **headerless**, exactly **4,194,304 bytes**, with SHA-1
`de5822f4f2f7a55acb8926d4c0eaa63d5d989312`. The name doesn't matter, but it must end in
`.sfc` or `.smc` for the search to find it. A file of 4,194,816 bytes has a copier header:
the first-run screen says so. Remove the header with
`tail -c +513 dump.smc > dump.sfc`.

The search goes two folders deep and stops after about 2000 files, so don't bury the
ROM deep in a big folder.

## 3. Add it to Steam

Run the helper. It prints the exact steps with your paths filled in, and changes nothing
itself:

```
packaging/steam/add-to-steam.sh ~/Applications/CT_Recomp-x86_64.AppImage
```

(Without a checkout, the steps are these.)

1. Steam (Desktop Mode) > **Games** > **Add a Non-Steam Game to My Library...** >
   **Browse...**. Pick the AppImage (choose "All Files" as the file type if it isn't
   listed), then **Add Selected Programs**.
2. Right-click the new entry > **Properties...**. Name it `CT Recomp`. Leave the launch
   options empty: the game starts with the original look; turn HD-2D on in the menu (**Look** tab, renderer) or add `--hd2d`.
3. **Properties** > **Controller**: choose the **Gamepad** layout. A keyboard or desktop
   layout sends keys rather than buttons.
4. Optional artwork: `packaging/steam/artwork/` has a capsule (`grid.png`, 600x900), wide
   capsule (`capsule_wide.png`, 920x430), hero (`hero.png`, 1920x620), transparent logo
   (`logo.png`, 1280x720) and icon (`icon.png`, 256x256). Right-click the entry >
   **Manage** > **Set custom artwork** and pick each file in its slot. The helper prints
   the paths. The art is original to this project (GPL-3.0-or-later, made by
   `packaging/icons/make_icons.py --steam`); it uses no game imagery.

Switch back to Game Mode. The game is in your library under **Non-Steam**.

## 4. First launch

1. The engine finds the ROM in the folders above and checks it.
2. It **builds the game code from your ROM on the device**. A progress screen lists the
   steps with a seconds counter. This takes **about 40 seconds on a ROG Ally** and longer
   on a Steam Deck (its CPU is slower, so expect around one to two minutes). It happens
   once; later launches start right away.
3. The game starts.

If no ROM is found, the screen says where to put it and which file is needed (size and
SHA-1). Copy the ROM over, for example from Desktop Mode or over the network, then press
**A** to search again. The search also repeats every few seconds by itself.

If the build fails, the screen shows the last lines of its output. Press **A** to try
again, or **B** to play without the built code, in the interpreter. That is slower but
still full speed on the Ally; on the Deck it hasn't been measured.

## Controls

The built-in controls are read as an Xbox-style controller through Steam Input. Buttons
are mapped by position, as on the SNES pad:

| SNES | Steam Deck / ROG Ally |
| --- | --- |
| B | A (bottom) |
| A | B (right) |
| Y | X (left) |
| X | Y (top) |
| L / R | L1 / R1 |
| Start | Menu (☰) |
| Select | View (⧉) |
| D-pad | D-pad or left stick |

| Combo | What it does |
| --- | --- |
| "..." (the Deck's misc1 button), or F1 | Opens the quick menu (below) |
| R5 (back button), or L3 + R3 (click both sticks together) | Opens the settings menu (below) |
| R3 (click and let go of the right stick) | Cycle the look: original, subtle, HD-2D, HD-2D Steam Deck, 3D diorama, 3D diorama for the Steam Deck |
| L3 (click and let go of the left stick) | Cycle the picture shape: 4:3, pixel 8:7, stretch |
| Hold View + Menu for 2 s | Asks "Quit?": **A** quits, **B** goes back to the game |
| Steam button > Exit Game | Quits too |

While you hold View and Menu together, the game doesn't see them, so the combo doesn't
open the game's menu. The answer to the question doesn't reach the game either.

None of these clash with the game's own soft reset (L + R + Select + Start): the stick
clicks and the back buttons are not SNES buttons.

### The quick menu

The **"..." button** (misc1, below the right trackpad on a Deck) opens a sheet of big
tiles over the paused picture: resume, quick save and load, slot save and load, mute and
volume, reset, game speed, frame step and rewind, the look, aspect, integer scale, CRT
and fullscreen, the overlays, the controller and touch options, save file backups and
restore, and **All settings...** for the full menu. The game waits behind it. Move the ring with the d-pad or left
stick, **A** runs a tile, left / right adjust the volume slider, **L1 / R1** jump a
section, **B**, **Start** or "..." again close it. It is made for the 7-inch screen:
tiles of at least 72 px at 1280x800, four columns held wide, three held tall.

On a touchscreen, tap a tile, drag the volume slider, drag the sheet to scroll, tap above
it to close. To open it by touch, tap the small handle at the top edge's middle (it
shows while you use the touchscreen, fades after a few seconds, and is left out where
it would touch the on-screen controller) or tap **three fingers** together on empty
spots. The bottom line shows the buttons of what you used last.
[USER-GUIDE.md](USER-GUIDE.md#the-quick-menu) has the details.

### The menu

**R5**, or **L3 + R3** on a controller without back buttons (the ROG Ally's own buttons,
an Xbox or PlayStation pad), opens the settings menu. The game waits behind it, silent,
until you close it with **B**, **Start** or R5 again. It is made for the controller and
the 7-inch screen: large text, the Deck's or the Ally's button pictures at the bottom.

| Button | In the menu |
| --- | --- |
| D-pad or left stick | Move (hold to repeat); left / right change the value |
| A | Toggle, change, run |
| B | Back; closes the menu |
| L1 / R1 | The previous / next tab |

The tabs: **Look** (renderer, HD-2D look, aspect, the 3D camera's tilt and zoom, gyro),
**Display** (fullscreen, frame pacing, text size, integer scale, CRT, battery mode),
**Controls** (what each back button does, the quick keys, fast-forward, always run,
turbo, pausing, rumble and its test), **Co-op** (on / auto / off, who plays which party member), **Packs**,
**Save states** (load and save the slots, with a picture of each; resume; screenshot),
**Audio** (volume, mute in the background, sample interpolation) and **About** (version,
licenses, Quit). The **Look** tab also has the HD text style and size, once you have set a
font (below).
Every change is saved at once. The game waits between two frames while the menu is open,
so a state saved from it is (nearly always) the picture behind it. The touchscreen works
the menu too (tap a row; tap the left or right of a value).
[USER-GUIDE.md](USER-GUIDE.md#the-menu) has the details.

### Back buttons and grips

The Deck's four back buttons (and a Steam Controller's grips, an Xbox Elite pad's paddles)
are not SNES buttons. They drive the frontend instead:

| Button | Key | What it does |
| --- | --- | --- |
| L4 (upper left) | F6 | Quick save |
| R4 (upper right) | F9 | Quick load |
| L5 (lower left) | Tab (hold) | Fast-forward while held |
| R5 (lower right) | F10 | The settings menu |
| "..." (misc1) | F1 | The quick menu |
| | F8 | Screenshot |

What they do, and the rest of the quality-of-life features, is in [Quality of life on a
handheld](#quality-of-life-on-a-handheld) below; R5 opens [the menu](#the-menu). The keys
avoid F2-F4 and F11 (debug view, aspect, co-op, fullscreen), F5
(the asset packs' reload) and F12 (Steam's screenshot key).

To change them, use the menu's **Controls** tab, or edit `settings.ini` (in
`~/.local/share/ct-recomp/hd2d/`) while the game is closed:

```
bind_paddle2=quicksave     # L4
bind_paddle1=quickload     # R4
bind_paddle4=ffwd          # L5
bind_paddle3=menu          # R5
bind_misc1=quickmenu       # the Deck's ... button, a Switch Pro's capture button
bind_touchpad=none         # trackpad / touchpad click
key_quicksave=F6
key_quickload=F9
key_ffwd=Tab
key_menu=F10
key_quickmenu=F1
key_screenshot=F8          # "none" for no key
```

Actions: `none`, `quicksave`, `quickload`, `ffwd`, `menu`, `screenshot`, `quickmenu`. Keys use SDL's
key names. Keys the game or the frontend already uses (arrows, Z X A S Q W, Enter, Right
Shift, F2-F4, F11) are refused; F1 only goes to `quickmenu`. SDL numbers the Deck's back buttons paddle1 = R4,
paddle2 = L4, paddle3 = R5, paddle4 = L5; on the first Steam Controller paddle1 is the
right grip and paddle2 the left one.

### Without Steam Input (Desktop Mode)

Started outside Steam, the game opens the Deck's built-in controller (and a Steam
Controller) with SDL's own drivers, so the back buttons, trackpads and gyro reach it
directly. It sets `SDL_JOYSTICK_HIDAPI_STEAMDECK=1` and `SDL_JOYSTICK_HIDAPI_STEAM=1`
(SDL 2 leaves the Steam Controller driver off by default). An environment variable of the
same name wins; `steam_hidapi=off` in `settings.ini` turns the Steam Controller driver
off. These drivers read `/dev/hidraw*`, which needs Steam's udev rules (installed with
Steam; on SteamOS and Bazzite already there). While the Steam client runs in the
background, its desktop layout may also send keys for some buttons; the game ignores a key
that arrives with a controller button.

It also sets `SDL_GAMECONTROLLER_USE_BUTTON_LABELS=0`, so Nintendo pads' face buttons are
read by position like the rest: the bottom button is SNES B.

**The 2026 Steam Controller.** The SDL this was built and tested against
(sdl2-compat 2.32.74 on SDL 3.4.20, Fedora's packages) contains SDL 3's driver for it
(`SDL_hidapi_steam_triton.c`, Valve 28de:1302/1303 and the dongles 1304/1305, enabled by
`SDL_JOYSTICK_HIDAPI_STEAM`), with four back buttons, two trackpads and a gyro, and the
game shows it with the Deck's glyphs. That is from SDL's source at the `release-3.4.20`
tag, not from a real controller. An AppImage built with `SDL2=build` (SDL 2.32.10) does
not know this controller: there it works through Steam Input's virtual pad only.

### Bazzite / InputPlumber (ROG Ally, Legion Go, ... outside Steam)

On Bazzite, **InputPlumber** (a system service) takes over the built-in controls of a ROG
Ally, Legion Go, MSI Claw or Zotac Zone. It hides the real pad (the Ally's
"Microsoft X-Box 360 pad" event device is made root-only) and offers an **emulated Steam
Deck controller** instead, which Steam reads. Started outside Steam, SDL finds no
controller at all, so the game reads that emulated controller itself.

What it is, as checked on a ROG Ally (RC71L), Bazzite, InputPlumber 0.84.0 (composite
device "ASUS ROG Ally", target `deck-uhid`):

- A **uhid** device (`/sys/devices/virtual/misc/uhid/0003:28DE:12FD.*`), bound to
  `hid-generic`, seen as `/dev/hidrawN` (it was `hidraw10`). `HID_ID=0003:000028DE:000012FD`,
  `HID_NAME=ROG Ally Controller`. The desktop user gets read access through an ACL.
- Its report descriptor is the Deck controller interface's: one vendor-defined collection
  (usage page 0xFFFF, usage 1) with 64 input bytes and 64 feature bytes, no report IDs.
  The kernel makes no event device from it.
- It streams **Deck state reports** continuously (about 450 a second, idle too): 64 bytes,
  `01 00 09 40` (report version 1, type 9 = `ID_CONTROLLER_DECK_STATE`, length 64), then
  the packet number, the button words, trackpads, accelerometer (about 1 g on one axis),
  gyro, triggers and sticks: exactly SDL's `SteamDeckStatePacket_t`.
- The product ID is InputPlumber's own, one per handheld, so Steam can tell them apart
  (InputPlumber's `ProductId`: Steam Deck 1205, generic 12f0, MSI Claw 12fa, Legion Go 2
  12fb, Zotac Zone 12fc, **ROG Ally 12fd** (all Ally models), Legion Go 12fe, Legion Go S
  12ff). Its lizard (keyboard / mouse) mode is off; InputPlumber's own keyboard and mouse
  targets do that work. Nothing needs to be written to it to read it (rumble, below, is
  the one thing the game writes).
- The Ally's buttons arrive as the Deck's: A / B / X / Y, View, Menu, the sticks and
  triggers; the **left back button (M1) is L4** and the **right one (M2) is R4** (so quick
  save and quick load by default); Command Center (short press) is the Steam button and
  Armoury Crate (short press) the Deck's ... button. The Ally has no L5 / R5: open the
  menu with **L3 + R3**. (InputPlumber's capability map `ally_type1.yaml`; a profile can
  change this.)

Why SDL doesn't take it:

- **SDL 3.4.x** (Bazzite's sdl2-compat 2.32.74 on SDL 3.4.20; also `main` in November
  2026): hidapi lists the uhid device (its Linux backend handles uhid nodes without a USB
  parent), and Valve's vendor ID passes the usage-page filter, but no driver claims it.
  The Deck driver accepts only what `SDL_IsJoystickSteamDeck` calls a Deck, which is
  product **1205** alone (`controller_list.h`); the Steam Controller driver only the
  Steam Controller IDs. 12fd is in neither list, so there are 0 joysticks, with or without
  `SDL_JOYSTICK_HIDAPI_STEAMDECK=1` / `SDL_JOYSTICK_HIDAPI_STEAM=1`.
- **SDL 2.32.10** (the AppImage's `SDL2=build`): the same product check, and before that
  its hidapi enumeration drops every hidraw device without a USB parent, so a uhid device
  is never listed. Patching the product list alone would not be enough, which is why the
  AppImage's SDL is left as it is.

**What the game does.** `frontend/hid_deck.c` (Linux) looks in `/sys/class/hidraw` for a
device with Valve's vendor ID, a vendor-defined report descriptor, and Deck state reports
when read; opens it (**read-only**, or read-write while rumble is on: below); and shows it
to the rest of the game as an SDL virtual game controller with the device's name, vendor
and product. Everything else works as with any pad: the SNES mapping above, the back
buttons, the Deck's button pictures, co-op, the menu, rumble. The report parsing is ported
from SDL's Deck driver (zlib; see THIRD_PARTY.md). The log says:

```
ct_sdl: hid_deck: using "ROG Ally Controller" (/dev/hidraw10, 28de:12fd): Steam Deck reports, read-write for rumble, as a virtual game controller
ct_sdl: using "ROG Ally Controller" on pad 1
```

**Rumble through InputPlumber.** The console had no rumble; the game makes it from what
happens (USER-GUIDE.md, "Rumble"). On this device the virtual controller's rumble becomes
the Deck's **simple rumble command**, exactly as SDL's own Deck driver sends it
(`HIDAPI_DriverSteamDeck_RumbleJoystick`): a 64-byte feature report, `ID_TRIGGER_RUMBLE_CMD`
(`$EB`), the heavy (left) and light (right) motor speeds, gains 2 and 0. It goes out as
`HIDIOCSFEATURE` on the hidraw node from a thread of its own, since the call waits for
InputPlumber's answer. Nothing else is ever written: no lizard-mode or settings report, no
InputPlumber setting, no D-Bus call. The node is opened read-write only while rumble is on
(`rumble=off` keeps it read-only, as before); if write access is refused the game reads
it as before and says `rumble: can't write to ...`.

What InputPlumber 0.84.0 does with it (its source, `src/input/target/steam_deck_uhid.rs`
and `src/input/source/hidraw/rog_ally.rs`): uhid hands it a `SetReport`; the `deck-uhid`
target unpacks report `$EB` as `PackedTriggerVibration` (`cmd_id`, `report_size`,
intensity, left and right speed, left and right gain), answers the `SetReport`, and sends an
`OutputEvent::TriggerVibration` to the composite device, whose output capability is
`ForceFeedback`. The ROG Ally source takes that event and sets its two motors on the Ally's
0-100 scale (`speed x 10^(gain / 10) / 65535`), with no timeout of its own: they run until
the next command, so the game always ends a pulse with a zero. Output reports
(`write()`) are ignored by that target; only the feature report counts. The composite
device's force feedback is on (D-Bus `org.shadowblip.Output.ForceFeedback.Enabled`, read
only: true) and its source lists `ForceFeedback`.

Checked on this Ally (RC71L, Bazzite, InputPlumber 0.84.0, outside Steam): a single
120 ms pulse at 3/16 strength, then zero, written by hand to `/dev/hidraw10`: both
`HIDIOCSFEATURE` calls returned 65 (InputPlumber answered) in 4 ms and 2 ms; then the game
itself (`--rumble-test 180 --log-rumble`) logs
`ct_sdl: hid_deck: rumble: the device took the rumble command (0 ms)`. Whether the motors
spin is for a person holding it to say: run the controller test below, whose last step
asks. Under Steam Input the game sees Steam's virtual pad instead and rumbles through
`SDL_GameControllerRumble`, which Steam passes on to the same motors.

A note on the libraries: Bazzite's libSDL2 is sdl2-compat over SDL 3, which hands a
virtual controller's rumble callback to SDL 3 unchanged, where its `int` is read as a
`bool` (1 success, 0 failure, the reverse of SDL 2). The game asks which convention it runs
under when it attaches the device, so SDL's own stop at the end of a rumble arrives.

InputPlumber makes the device anew when Steam starts or stops and around suspend; the game
drops the old one when reads fail and looks again every 2 seconds. `settings.ini`
`hid_deck=auto|on|off` (the menu's **Controls** > **Built-in controls (InputPlumber)**):

- `auto` (the default): read it only while SDL has no other game controller. Started from
  Steam, Steam's virtual pad is there, so the game uses that and doesn't read the device
  twice; a USB pad plugged in also takes over. A real Deck (28de:1205) is left to SDL's
  own driver.
- `on`: read it next to the other controllers (and a real Deck's too, without turning its
  lizard mode off).
- `off`: never.

With `--log-input`, each button change of the device is printed
(`ct_sdl: hid_deck: A down`, `... R4 (paddle1) down`, `... L2 down (past half)`,
`... left stick left`), next to SDL's own `controller button ...` lines.

If the device can't be opened (no ACL on `/dev/hidrawN`), the log says
`ct_sdl: hid_deck: can't read /dev/hidrawN (Permission denied)`. If a handheld starts with
no controller at all, the game says so once on screen: "No controller found. Launch from
Steam, or see HANDHELD.md (InputPlumber)". Starting it from Steam always works: Steam reads
the emulated controller and gives the game its virtual pad.

While Steam runs in the background, a game started outside it still gets the device (SDL
hides Steam's virtual pads from programs Steam didn't start), and Steam's desktop layout
may send keys as well; see the paragraph above.

### Under Steam Input (Game Mode, or started from Steam)

Steam hides the real controllers from the game (`SDL_GAMECONTROLLER_IGNORE_DEVICES`) and
gives it a virtual Xbox-style pad, as before. That pad has no back buttons, so bind them
to the keys in the table above, by hand (**Controller** > **Edit Layout** > **Back Grip
Buttons**, each one > **Keyboard**) or with the recommended layout,
`packaging/steam/ct-recomp_neptune.vdf`: the gamepad layout with L4 = F6, R4 = F9,
L5 = Tab, R5 = F10, gyro and trackpads off. Steam has no import button for a file; copy it
into Steam's templates folder yourself (the helper script prints the commands and does not
do it), restart Steam, then choose it under **Controller** > **Templates**:

```
mkdir -p ~/.steam/steam/controller_base/templates
cp packaging/steam/ct-recomp_neptune.vdf ~/.steam/steam/controller_base/templates/
```

The template's back-button names (`button_back_left_upper` = L4, `button_back_left` = L5)
follow layouts exported from real Decks; the file itself has not been loaded by Steam yet.
If Steam doesn't list it, bind the four buttons by hand.

### Button glyphs

Messages on screen show the buttons of the device you used last: the Deck's, a Steam
Controller's, Xbox, PlayStation or Switch buttons, or keyboard keys after a key press. The
pad is recognized from SDL's controller type and its USB vendor and product. Steam Input's
virtual pad shows the Deck's buttons on a Deck and Xbox buttons elsewhere, unless Steam
tells SDL the real pad's type. The pictures are Kenney's Input Prompts (CC0,
`third_party/glyphs/`). Both the HD-2D and the original picture show messages; the
startup log lists each controller with its vendor:product and glyph set.

### Gyro aiming (optional)

The menu's **Look** tab (or `gyro=on` in `settings.ini`) lets the first controller's gyro tilt and turn the camera of
the HD-2D 3D diorama look a little (up to 8 degrees), easing back when the pad is still.
It is off by default and does nothing in the other looks. `gyro_strength=1.00` scales it.
The Deck's gyro reaches the game only without Steam Input (above); under Steam Input,
leave Steam's gyro unbound (the template does).

### Two players on one handheld

Connect a second controller (USB or Bluetooth) and co-op turns on: the built-in controls
are player 1, the second controller is player 2, and it plays party members 2 and 3 in
battle and walks party member 2 in the field ([USER-GUIDE.md](USER-GUIDE.md#two-players-co-op)).

Under Steam's Game Mode both controllers reach the game through Steam Input, as two
virtual Xbox controllers. Steam numbers them in the order they became active. The game
takes the first one it sees as player 1, and that is the built-in controls once you have pressed something on
them. If the order comes out wrong, reorder the controllers in Steam's controller settings,
or unplug the second one and plug it back in.
Give the second controller a gamepad layout in Steam, not a desktop one. A desktop
layout sends keyboard keys, and those reach player 1.

On the Ally, Armoury Crate's controller mode and Steam both have to treat the second pad
as a controller. A pad that shows up only as a joystick is listed as "joystick only: not
used" at startup (run the AppImage from a terminal to see the list). With no keyboard,
co-op can only be turned off by disconnecting the second controller, or by binding F4 to a
back button in Steam Input. The choice is saved.

None of this has been tried on a real Deck or Ally yet. It was tested with scripted
pad 2 input (the `coop_*` ctests).

## Looks, frame rate and battery

In Game Mode the engine starts fullscreen at the screen's size, with no mouse cursor and
the original look. Turn HD-2D on in the menu (**Look** tab) or add `--hd2d` to the launch options; the Deck then gets its lighter preset by default.

- **Steam Deck:** with no look saved yet, the engine picks the **deck** preset. It is the
  HD-2D look with a shorter depth-of-field blur and a single bloom level. On the 16:10
  screen the narrow side bars are plain black rather than a blurred fill. The Deck is
  recognized by its GPU (`VANGOGH` or `Custom GPU 0405` in the OpenGL renderer name) or
  its firmware product name (Jupiter or Galileo).
- **ROG Ally:** the full HD-2D look. The renderer takes about 2 ms per frame there.
- **3D diorama on a handheld:** the `deck3d` look is the deck look with the 3D diorama,
  its scene drawn at 67% and upscaled with AMD FSR 1, its ambient occlusion at a quarter
  size. On the Ally it costs about what the full 3D look (`hd2d3d`) cost before those
  options. The menu's **Look** tab sets the 3D render scale, sharpening and ambient
  occlusion for either 3D look ([HD2D.md](HD2D.md#render-scale-sharpening-and-ambient-occlusion)).

Once you change the look with R3 or in the menu, your choice is saved and used from then
on, on any device. The menu's **Display** tab also switches the frame pacing between the
timer and vsync (`--pacing` at launch still wins).

**Frame pacing.** The console runs at 60.0988 frames per second. The engine paces itself
with a precise timer, sleeping to each frame's time, and the sound's rate control keeps
the audio in step. Under gamescope the newest frame is shown at each refresh, so how
smooth it looks depends on the refresh rate:
- **Deck OLED (90 Hz):** set the refresh rate to **60 Hz** in the Performance menu (the
  **...** button). At 90 Hz, frames alternate between one and two refreshes, which shows
  as judder.
- **Deck LCD:** already 60 Hz. About every 10 seconds one frame is never shown, since
  the console's 60.0988 Hz is a little faster than the panel's 60 Hz. `--pacing vsync`
  avoids that.
- **ROG Ally:** at 120 Hz with VRR the frames are shown as they come.
- **Frame limiter:** not needed. If you set one, set it to 60.
- **`--pacing vsync`:** paces by the display's vertical sync instead, for 60 Hz
  screens. The sound's rate control absorbs the 0.16% difference. When frames run late
  and the sound queue runs low, vsync is switched off until the queue refills, so the
  game briefly runs a little fast instead of the sound cutting out. It isn't the default
  yet: it hasn't been measured in a real Game Mode session.
- **`--log-pacing`:** prints frame-time statistics when started from a terminal, and
  every exit prints them for the whole run.

## Your own music and fonts

Both are optional, and the project ships neither: you add files of your own to the data
folder (`~/.local/share/ct-recomp/ct/`; in Desktop Mode, the Dolphin file manager shows
it with hidden files on, Ctrl+.).

- **Audio packs** replace the game's songs with Ogg Vorbis, FLAC or WAV files: a
  `[[music]]` entry per song in a pack's `pack.toml`, under `ct/packs/NAME/`. Start once
  from a terminal with `--music-log` to see each song's id as it plays. Add
  `--pack NAME` to the launch options once (it is remembered). Sound effects stay the
  console's; while a song is replaced they play without echo. **A pack made from the
  game's music is for your personal use only: never share or upload it.** Details:
  [ASSET-PACKS.md](ASSET-PACKS.md#music-audio-packs). Decoding a song costs well under a
  millisecond a frame.
- **HD text:** put a `.ttf` or `.otf` font in `ct/fonts/` and add
  `--hd-font NotoSans-Bold.ttf` to the launch options once (it is remembered, as
  `hd_font=` in `settings.ini`). The dialogue boxes' text is then drawn with it at the
  screen's resolution; the menu's **Look** tab sets its style and size. It needs the GL
  renderer (the HD-2D renderer; with the original look the game switches to it, as for
  asset packs). Free fonts to download: [USER-GUIDE.md](USER-GUIDE.md#hd-text).
- **Sample interpolation** (menu, **Audio**): the console's Gaussian (default), or
  cubic or sinc for a crisper sound on the handheld's speakers or headphones.

## Quality of life on a handheld

The full description is in the [player's guide](USER-GUIDE.md#quality-of-life). On a
Deck or an Ally:

| Feature | How | Default |
| --- | --- | --- |
| Quick save / quick load | L4 / R4 | on |
| Save slots 1-4 | Shift+F6 / Shift+F9, F7 picks the slot (keyboard) | |
| Fast-forward | hold L5 (x3, sound muted; `ffwd_speed=2..4`) | |
| Resume where you left off | quit (View+Menu, Steam's Exit Game), or put the handheld to sleep; the next start asks **Resume? [A] yes [B] no** | `resume=on` |
| Pause when a controller goes | press a button to go on | `pause_on_disconnect=on` |
| Pause when the window loses focus | | `pause_on_focus_loss=auto`: off in Game Mode |
| Battery mode | `battery_mode=light` (deck look on battery) or `saver` (also a 30 Hz picture, game at 60) | `off` |
| Screenshot | F8, or Steam's own (Steam + R1) | |
| Always run | Caps Lock, or `always_run=on` | `off` |
| Turbo | Ctrl+T while holding the buttons, or `turbo=a,b` | none |
| Original picture: integer scale, CRT look | `integer_scale=on`, `crt=on` (Ctrl+R) | integer on, CRT off |

All settings are in `~/.local/share/ct-recomp/hd2d/settings.ini`; edit it while the game
is closed.

- **Steam's Exit Game** stops the program with SIGTERM: the engine treats it as a quit,
  saves the resume point (a few frames' work) and your in-game save, and exits.
- **Sleep** (the power button): the engine hears about it from logind and writes the
  resume point before the system sleeps (it holds a "delay" lock for that, at most a few
  seconds). The sound stops first, so there is no buzz, and on waking it starts again
  cleanly; the frame timer starts over instead of racing to catch up. If the battery runs
  out while asleep, the next start offers that resume point.
- **Focus in Game Mode:** gamescope moves the focus around (the Steam overlay, the
  quick-access menu), so pausing on focus loss is off there unless you set
  `pause_on_focus_loss=on`.
- **Battery:** the engine reads `/sys/class/power_supply` every few seconds. The Deck's
  and the Ally's USB-C chargers count as outside power. `saver` halves the GPU's work
  (the HD-2D picture is drawn every other frame), the game itself keeps its 60 Hz timing.

## Saves and where files live

Everything is in `~/.local/share/ct-recomp/`:

| File | What it is |
| --- | --- |
| `ct/ct_game.so` | The game code built from your ROM. Don't share it. |
| `ct/ct_game.stamp` | What it was built from |
| `ct/rom_path` | Where your ROM is |
| `ct/ct.srm`, `ct/ct.srm.bak` | Your in-game saves, and the copy before the last write |
| `ct/states/` | Save states, the resume point (`auto.state`) and their pictures |
| `hd2d/settings.ini` | Everything the menu sets: the look, the buttons, co-op, the volume, the settings above |

Saves are written a few seconds after you save in the game, and again on quit,
including through the Steam button. To back them up, copy `ct/ct.srm`. More detail is in
the [player's guide](USER-GUIDE.md#your-saves).

## Troubleshooting

- **It doesn't start from Game Mode.** Run the AppImage once in Desktop Mode from Konsole
  (`~/Applications/CT_Recomp-x86_64.AppImage`) and read the `ct_sdl:` lines. If it
  complains about FUSE, add `--appimage-extract-and-run` as the first launch option.
- **"No ROM found"** although the file is there. Check the size (4,194,304 bytes), the
  extension (`.sfc` or `.smc`) and the depth (at most two folders below `~/Documents`).
  The terminal shows `ct_sdl: search:` lines for each file it rejected and why.
- **Buttons do the wrong thing, or two things.** Set the controller layout to
  **Gamepad** in the game's Steam properties. A desktop layout sends Enter and Esc along
  with A and B.
- **Judder on a Deck OLED:** set the screen to 60 Hz (see above).
- **Crackling sound.** Run from Konsole with `--log-pacing`. Each exit prints
  `ct_sdl: audio: N underruns` and the frame-time statistics. Under heavy load from other
  programs, frames run late and the sound queue empties.
- **Start over:** delete `~/.local/share/ct-recomp/ct/ct_game.stamp` to rebuild the game
  code, or `ct/rom_path` to search for the ROM again. Your saves are left alone.

## Testing your controller

```
ct_sdl --pad-test                 # the AppImage: CT_Recomp-x86_64.AppImage --pad-test
```

A checklist of every control appears in the top-right corner while the game runs,
muted (add `--no-mute` for sound). Press each control once, in any order: it turns
green, the next one is highlighted, and each step is also printed to the terminal
(`pad test: ok ...`). The back buttons do a real quick save and quick load, so add
`--states-dir /tmp/padtest` to keep your own slots untouched. The last step is
**rumble**: once the 18 controls are ticked, the controller pulses every 2 seconds (at
your rumble strength, or Light when rumble is off) until you answer: **A** if you felt it,
**B** if not; the answer is printed (`pad test: rumble: felt it (A: yes)`). Then the game
says every control works and quits a few seconds later. `--mute` alone silences any run without
changing your volume setting.

## Not yet tested on real hardware

These parts were tested on a ROG Ally running Bazzite, but only headless (SDL's
offscreen driver, an SDL virtual controller) or under
`gamescope --backend headless -W 1280 -H 800 -r 60`, never from Steam's real Game Mode:
- the fullscreen size
- the vsync pacing, which held 60.000 Hz under headless gamescope
- the Steam Input layout and the quit combo on the real buttons

The Steam Deck paths (the deck preset's speed, GPU detection on real Deck drivers, build
time) haven't been run on a Deck at all. Reports are welcome.

The back buttons, glyphs and gyro were tested with SDL virtual controllers
(`test_actions`: a virtual Deck 28de:1205 with paddles 1-4, a Steam Controller, Xbox,
DualSense and Switch Pro pads) against sdl2-compat 2.32.74 / SDL 3.4.20, and the message
pictures in headless HD-2D and plain renders. Not tried on real hardware: SDL's
Deck and Steam Controller drivers in Desktop Mode, the 2026 Steam Controller, a real gyro,
and the Steam Input template.

The InputPlumber reader (`hid_deck`) was checked on the Ally itself: the device's
descriptor and two seconds of its reports were read, and `ct_sdl_engine` found it and
used it on pad 1 (`hid_deck=off` gave the no-controller message instead). Its parsing,
hot-plugging and auto mode are tested with reports made from the documented layout
(`test_hid_deck`: a fake sysfs and a FIFO). On 2026-11-14 every control was pressed on
the Ally outside Steam with `--pad-test` (below), and all 18 passed: face buttons,
d-pad, left stick, L1/R1, L2/R2, View, Menu, both back buttons (a real quick save and
quick load) and the L3+R3 menu chord. Steam starting and stopping, and suspend, have
not been tried yet.

The quality-of-life features were tested headless (SDL's dummy drivers, scripted with
`--qol-action`; `sdl_qol` and `savestate_replay` in ctest) and on a desktop, including
a real logind delay lock (`systemd-inhibit --list` shows `ct-recomp ... sleep ... delay`).
Not tried on real hardware: an actual suspend and wake on a Deck or an Ally (the resume
point written before sleep, the sound after waking), Steam's Exit Game in Game Mode,
the battery mode's switch on a real unplug, and how the 30 Hz picture looks under
gamescope.
