# ZMK Config Context

My Corne wireless split keyboard with a TPS43 trackpad on the peripheral (right) side.

## Hardware

- **Keyboard**: Typeractive Corne Wireless PCB (standard ZMK Corne shield pinout)
- **Controllers**: 2x Nice!Nano v2 (nRF52840)
- **Layout**: Colemak-DH variant
- **Trackpad**: Azoteq TPS43 (IQS5xx-family), wired to the RIGHT half only
- **Mounting**: Nice!Nano is mounted components-DOWN on the PCB. No nice!view or OLED installed. Mounted sideways: physical left/right of the trackpad maps to on-screen up/down (hence the Y-axis invert via input processor).

## Trackpad wiring (right half only)

Soldered directly to Nice!Nano pads using a 6-conductor ribbon.

TPS43 FPC ribbon order top-to-bottom: SDA, SCL, 3V3, GND, RST, RDY

| TPS43 pin | Nano silkscreen | nRF GPIO |
|-----------|-----------------|----------|
| SDA       | 006             | P0.06    |
| SCL       | 008             | P0.08    |
| 3V3       | 3.3 (top-left)  | -        |
| GND       | GND (top-left)  | -        |
| RST       | 017             | P0.17    |
| RDY       | 020             | P0.20    |

The 3V3 wire arcs across to the top-left power pad. All signal wires drop straight down the right edge of the Nano.

## Software stack

- **ZMK**: zmkfirmware/zmk main
- **Trackpad driver**: `essenceotd/zmk_driver_azoteq` fork (chosen specifically for its ZMK power-management integration via `enable-power-management` and `idle-sleep`, dramatically reducing peripheral battery drain)
- **Raw HID**: `zzeneg/zmk-raw-hid` module provides the `raw_hid_adapter` shield applied to the left (central) half
- **Board naming**: Zephyr 4.1 / Hardware Model V2 format: `nice_nano//zmk`

## File layout

```
config/
  corne.conf           shared Kconfig: ZMK_POINTING, I2C, INPUT, INPUT_TPS43, split battery fetching/proxy
  corne.dtsi           shared devicetree: split_inputs parent, tps43_listener (disabled)
  corne.keymap         14 layers (0-13), combos, macros, &tp_toggle behavior
  corne.json           keymap editor metadata
  corne_left.overlay   central side: enables tps43_listener, layer-based sensitivity overrides
  corne_right.overlay  peripheral side: i2c0 bus, TPS43 node, pinctrl, input processors
  west.yml             modules: zmk, essenceotd zmk_driver_azoteq, zzeneg zmk-raw-hid
build.yaml             CI matrix: corne_left raw_hid_adapter + corne_right, both on nice_nano//zmk
zephyr/module.yml      makes this repo a Zephyr module (cmake: modules, dts_root: .)
modules/
  null_keys_viz/       raw HID stream for the Null Keys overlay (central only)
  tap_drag/            zmk,input-processor-tap-drag: laptop-style tap / tap-and-drag / double tap
  trackpad_toggle/     &tp_toggle behavior + zmk,input-processor-trackpad-gate
dts/bindings/          bindings for the custom behavior and input processors
```

## Key behaviors tied to the trackpad

- **Peripheral processor chain**: `zip_trackpad_gate` -> `zip_tap_drag` -> Y invert -> 0.9x scaler
- **Base sensitivity**: 0.9x via `zip_xy_scaler 9 10` on peripheral (one-time global reduction)
- **Y axis inverted**: `zip_xy_transform INPUT_TRANSFORM_Y_INVERT` on peripheral (because the pad is mounted sideways)
- **Hold P (lt 12 P)**: activates HighSens layer, applies `zip_xy_scaler 2 1` on central listener, effective 1.8x
- **Hold V (lt 13 V)**: activates LowSens layer, applies `zip_xy_scaler 1 2` on central listener, effective 0.45x
- **T+D combo**: fires `&mkp MCLK` (middle click), restricted to layer 0
- **N+I combo**: `&tp_toggle`, turns the trackpad off/on. Global behavior, so it runs on the right half; the state is saved to flash there (after ZMK's settings save debounce) and survives deep sleep. Has `require-prior-idle-ms = <150>` so typing "ni" doesn't trigger it.
- **Gestures**: tap = click (sent after the 200 ms drag window), tap then touch-and-move = drag, double tap = double click, two-finger tap = right click, two-finger scroll, zoom (zoom events are currently dropped by ZMK). Chip press-and-hold is off.
- **Smoothing**: `filter-settings = <0x0B>` (dynamic IIR) with `filter-dynamic-bottom = <5>`.

## Driver gotchas

- `rst-gpios` must be `GPIO_ACTIVE_HIGH`: the driver sets the pin "active" to release the chip from reset.
- The driver reports `INPUT_BTN_TOUCH` while a finger is down and ZMK maps that to a held left click. `zip_tap_drag` consumes it.
- Driver default `filter-settings` is `0x0F`, which selects the static IIR and ignores the dynamic filter settings.

## Battery reporting

`CONFIG_ZMK_SPLIT_BLE_CENTRAL_BATTERY_LEVEL_FETCHING` and `..._PROXY` are on. Windows only shows the first battery service (left half), so the right half's level is shown in the Null Keys overlay instead (`NK_MSG_BATTERY`, 0x05).

## Null Keys overlay

Godot 4 project at `D:\Code\null_keys` (not a git repo). A Python bridge (`bridge/nullkeys_bridge.py`) reads the raw HID reports and forwards JSON over UDP to the overlay. Protocol is documented at the top of `modules/null_keys_viz/src/null_keys_viz.c` and in the overlay README. The overlay keeps a copy of the module in `firmware/zmk-config/`; keep it in sync.

## Build and flash

Pushes to the `colemak` branch (default) trigger GitHub Actions. Artifacts show up as `artifact-corne_left-nice_nano_v2` and `artifact-corne_right-nice_nano_v2`. There is no local toolchain; CI is the only build.

Flash by double-tapping reset on the Nice!Nano, then dragging the UF2 onto the `NICENANO` USB drive that appears. Trackpad and processor changes live on the right half, so that's the one to reflash for them.

## GitHub

- Repo: https://github.com/BrendanTipney/zmk-config
- Working branch: `colemak`
- Build workflow: `.github/workflows/build.yml`

## Driver reference

The essenceotd fork's bindings use different property names than AYM1607's:

- `single-tap` (not `one-finger-tap`)
- `two-finger-tap`
- `three-finger-tap` (on specific branches; software-detected since the IQS5xx hardware suppresses tap gesture when a third finger lands)
- `enable-power-management` (required for sleep integration)
- `idle-sleep` (optional, also suspend trackpad during ZMK IDLE state)
- `sensitivity = <N>`, `scroll-sensitivity = <N>`, `zoom-sensitivity = <N>` (percent)
- `rst-gpios` (not `reset-gpios`)
- `rdy-gpios`
- compatible: `azoteq,tps43`

Full property list: `dts/bindings/input/azoteq,tps43-common.yaml` in the driver repo.

## Battery context

Peripheral battery was draining faster than expected prior to the driver migration (AYM1607 driver doesn't suspend the trackpad on ZMK idle). The essenceotd fork integrates with ZMK's activity state machine. CONFIG_ZMK_IDLE_SLEEP_TIMEOUT is 30 minutes.

## Preferences

Avoid "AI writing tells" in anything user-facing: no em-dashes, no filler openers, no clichés, no over-bulleting, no hedging. Direct and natural voice.
