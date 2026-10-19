# NullKeys

ZMK firmware for NullKeys, my wireless split keyboard: a Corne cut down to 36 keys, with an Azoteq TPS43 trackpad built into the right half.

The layout is built around one idea. Moving a hand between the mouse and the keyboard is one of the biggest slowdowns on a computer, and in CAD, video editing and other work that jumps between programs you can't just stay on the keyboard. So every letter, every hotkey and every modifier combination can be reached from the left hand while the right hand stays on the mouse.

Case study, 3D model and an interactive layout explorer: [nullmoniker.ca/keyboard.html](https://nullmoniker.ca/keyboard.html)

Printable case files: [Thingiverse](https://www.thingiverse.com/thing:7424328)

## Hardware

| Part | Detail |
|---|---|
| PCB | Typeractive Corne Wireless, outer columns broken off (5 columns + 3 thumb keys per half) |
| Controllers | 2x nice!nano v2 (nRF52840), mounted components down |
| Switches | Kailh Choc v1 low profile, white |
| Trackpad | Azoteq TPS43 (IQS5xx family), right half only, I2C |
| Case | My own design in Fusion 360, printed in PLA at 0.1 mm layers, M2 screws and heat set inserts, fold out tenting legs |
| Magnets | Three in the case, so the halves snap together and attach to accessories like a pocket mount |
| Connection | Bluetooth (5 profiles) or USB to the left half |

The keyboard reports itself as `NullDeck` over Bluetooth and USB.

### Trackpad wiring

The TPS43 is soldered straight to the right nice!nano on a six wire ribbon. FPC order, top to bottom: SDA, SCL, 3V3, GND, RST, RDY.

| TPS43 | nice!nano pad | nRF GPIO |
|---|---|---|
| SDA | 006 | P0.06 |
| SCL | 008 | P0.08 |
| 3V3 | 3.3 | |
| GND | GND | |
| RST | 017 | P0.17 |
| RDY | 020 | P0.20 |

The pad is mounted sideways, so the firmware inverts the Y axis to make moving across the pad move the pointer the way you'd expect.

## Features

**Colemak with QWERTY hotkeys.** The letters start from Colemak, with a few moved back so the important shortcuts stay where every program expects them. Z, X, C and V match QWERTY exactly, and S stays under the ring finger.

**Home row combos for editing.** Press keys together instead of reaching:

| Press together | Result |
|---|---|
| S + R | Backspace |
| R + T | Delete |
| S + T | Tab |
| Y + ; | Backspace (right hand) |
| Q + W + F + P | Esc |

**Every letter on one hand.** Hold R or T and the right half mirrors onto the left (the Hand Swap layer). Hold N or E for the opposite. With one hand on the mouse, the other can still type anything.

**Every modifier from the thumbs.** The left thumbs carry all four modifiers:

* First thumb: tap dance. Hold it for Alt, tap it twice for a sticky Win key. Alt stays holdable for things like Alt+Tab.
* Second thumb: Space when tapped, Shift when held.
* Third thumb: Enter when tapped, Ctrl when held.

The right thumbs mirror Space/Shift and Enter/Ctrl. The outer right thumb types Alt, and holding it sends the media Repeat key.

**Gaming mode.** Press A + S + T to toggle the Gaming layer. It puts QWERTY back one column in, with WASD under the fingers, Shift and Ctrl on the pinky, and T, G and Space on the thumbs. Hold Q on that layer for the number row.

**Trackpad with laptop gestures.** Tap to click, tap then drag to hold the button, double tap to double click, two finger tap, scroll and zoom. A custom input processor (`modules/tap_drag`) handles tap and drag. Hold P for high sensitivity (1.8x) and V for low sensitivity (0.45x) when you need speed or precision.

**Long battery life.** The trackpad driver fork sleeps the pad with the keyboard, and the halves go to deep sleep after 30 minutes idle.

**NullKeys overlay support.** The left half streams key positions and layer state over Raw HID (`modules/null_keys_viz`) so the NullKeys overlay can show exactly what the keyboard is doing. It only streams while the overlay is asking for data.

## Layers

Hold a key to reach its layer. `▽` means the key falls through to the layer below.

### 0 Base

```
Q         W         F         P         G           J         L         U         Y         ;
A         S         R         T         D           H         N         E         I         O
Z         X         C         V         B           K         M         ,         .         /
                    Alt       Space     Enter       Enter     Space     Alt
                              Shift     Ctrl        Ctrl      Shift     Repeat
```

Hold keys:

| Hold | Layer |
|---|---|
| A | Num / Sym |
| Z | Arrows |
| R, T, N, E | Hand Swap |
| S | Media |
| D | VS Code |
| Q | Bluetooth |
| P | High trackpad sensitivity |
| V | Low trackpad sensitivity |
| A + left Enter | Function |

### 1 Num / Sym (hold A)

```
          7         8         9         /                     (         )         \
          4         5         6         -                     {         }         '
          1         2         3         =                     [         ]         ~
                    .         0         Enter
                              Shift     Ctrl
```

### 2 Arrows (hold Z)

```
          Home      ↑         End       PgUp
          ←         ↓         →         PgDn
          Ctrl+←              Ctrl+→
          Home                End
```

On the bottom row, tap for word jumps (Ctrl+← and Ctrl+→) and hold for Home or End.

### 3 Hand Swap (hold R, T, N or E)

The right half's letters mirrored onto the left hand, and the left half's onto the right.

```
;         Y         U         L         J           G         P         F         W         Q
O         I         E         N         H           D         T         R         S         A
/         .         ,         M         K           B         V         C         X         Z
                                                    Enter     Space     Alt
                                                    Ctrl      Shift
```

### 4 Gaming (toggle with A + S + T)

```
Gaming Nu Q         W         E         R
Shift     A         S         D         F
Ctrl      Z         X         C         V
                    T         G         Space
```

Hold the top left key for **5 Gaming Num**: 1 2 3 4, 5 6 7 8 and 9 0 on the Q W E R, A S D F and Z X positions.

### 6 Media (hold S)

```
                              Vol +
                              Vol −
Play      Prev      Next      Mute      Win+Shift+K
```

### 7 Function (hold A and the left Enter thumb)

```
          F7        F8        F9        F10                                       ↑
          F4        F5        F6        F11                             ←         ↓         →
          F1        F2        F3        F12
```

### 8 Bluetooth (hold Q)

```
          BT 1      BT 2      BT 3
          BT 4      BT 5
                                        BT clear
```

### Other layers

| Layer | How to reach it | What it does |
|---|---|---|
| 9 VS Code | Hold D | Line and selection shortcuts on the left hand |
| 10 QWERTY | Toggle with S + T + D | Plain QWERTY with the same layer keys and thumbs |
| 11 Mouse | Not bound | Pointer keys, scroll and clicks |
| 12 High Sens | Hold P | Every key passes through; the trackpad runs at 1.8x |
| 13 Low Sens | Hold V | Every key passes through; the trackpad runs at 0.45x |

## All combos

| Keys | Action | Layers |
|---|---|---|
| S + T | Tab | All |
| S + R | Backspace | Base, Num / Sym, Arrows, Function |
| R + T | Delete | Base, Num / Sym, Arrows, Function, Bluetooth |
| Y + ; | Backspace | All |
| Q + W + F + P | Esc | All |
| L + U | ( | All |
| U + Y | ) | All |
| N + E | { | All |
| E + I | } | All |
| M + , | [ | All |
| , + . | ] | All |
| M + , + left Space | < | All |
| , + . + left Space | > | All |
| Q + W + F | Win+← (window left) | All |
| F + P + G | Win+→ (window right) | All |
| W + F + P | Win+↑ (maximize) | All |
| X + C + V | Win+↓ (minimize) | All |
| S + R + T | Alt+Tab | All |
| A + S + R + T | Alt+Space | All |
| A + S + T | Toggle Gaming | All |
| S + T + D | Toggle QWERTY | All |
| T + D | Middle click | Base |
| A + left Enter | Function layer | Base |
| Q + S + Z + C | Shut down Windows (Win+X, U, U) | All |

## Building and flashing

Pushing to the `colemak` branch runs the GitHub Actions build. When it finishes, download the artifacts from the run:

* `corne_left-nice_nano_v2`: the left half. It includes the `raw_hid_adapter` shield, because the left half is the split central that owns layer state and the connection to the computer.
* `corne_right-nice_nano_v2`: the right half, with the trackpad.

To flash a half:

1. Plug it in over USB.
2. Double tap the reset button. A drive called `NICENANO` appears.
3. Drag that half's `.uf2` file onto the drive. It reboots on its own when the copy finishes.

Flash both halves after changes to the keymap or anything shared. To pair, clear the profile on your computer and on the keyboard (Bluetooth layer, BT clear), then connect to `NullDeck`.

### Modules

Set up in `config/west.yml`:

* [zmkfirmware/zmk](https://github.com/zmkfirmware/zmk) on `main` (Zephyr 4.1, boards named `nice_nano//zmk`)
* [essenceotd/zmk_driver_azoteq](https://github.com/essenceotd/zmk_driver_azoteq): the TPS43 driver, chosen for its ZMK power management (`enable-power-management`, `idle-sleep`)
* [zzeneg/zmk-raw-hid](https://github.com/zzeneg/zmk-raw-hid): the Raw HID interface the overlay reads

And in this repo, as a Zephyr module (`zephyr/module.yml`):

* `modules/tap_drag`: laptop style tap, tap and drag and double tap for the trackpad
* `modules/null_keys_viz`: streams key and layer events to the NullKeys overlay

## Files

| File | What it holds |
|---|---|
| `config/corne.keymap` | Layers, combos, macros and behaviors |
| `config/corne.conf` | Shared Kconfig: keyboard name, sleep, Bluetooth power, pointing |
| `config/corne.dtsi` | Split input plumbing shared by both halves |
| `config/corne_left.overlay` | Central side: trackpad listener and the sensitivity layer overrides |
| `config/corne_right.overlay` | Peripheral side: I2C bus, TPS43 node and pins, input processors |
| `config/west.yml` | External modules |
| `build.yaml` | Build matrix for the two halves |

## Tuning the trackpad

All in `config/corne_right.overlay` unless noted:

* Base speed: `zip_xy_scaler 9 10` (0.9x)
* Axis direction: `zip_xy_transform INPUT_TRANSFORM_Y_INVERT`
* Tap and drag window: `window-ms` on `zip_tap_drag` (200 ms)
* Smoothing: `filter-settings` and `filter-dynamic-bottom`
* Sensitivity layers: the `high_sens_override` and `low_sens_override` blocks in `config/corne_left.overlay`

## License

MIT. See `LICENSE`.
