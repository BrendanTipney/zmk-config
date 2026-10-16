\# ZMK Config Context



My Corne wireless split keyboard with a TPS43 trackpad on the peripheral (right) side.



\## Hardware



\- \*\*Keyboard\*\*: Typeractive Corne Wireless PCB (standard ZMK Corne shield pinout)

\- \*\*Controllers\*\*: 2x Nice!Nano v2 (nRF52840)

\- \*\*Layout\*\*: Colemak-DH variant

\- \*\*Trackpad\*\*: Azoteq TPS43 (IQS5xx-family), wired to the RIGHT half only

\- \*\*Mounting\*\*: Nice!Nano is mounted components-DOWN on the PCB. No nice!view or OLED installed. Mounted sideways: physical left/right of the trackpad maps to on-screen up/down (hence the Y-axis invert via input processor).



\## Trackpad wiring (right half only)



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



\## Software stack



\- \*\*ZMK\*\*: zmkfirmware/zmk main

\- \*\*Trackpad driver\*\*: `essenceotd/zmk\_driver\_azoteq` fork (chosen specifically for its ZMK power-management integration via `enable-power-management` and `idle-sleep`, dramatically reducing peripheral battery drain)

\- \*\*Raw HID\*\*: `zzeneg/zmk-raw-hid` module provides the `raw\_hid\_adapter` shield applied to the left (central) half

\- \*\*Board naming\*\*: Zephyr 4.1 / Hardware Model V2 format: `nice\_nano//zmk`



\## File layout

config/

corne.conf -- shared Kconfig, enables ZMK\_POINTING, I2C, INPUT, INPUT\_TPS43

corne.dtsi -- shared devicetree: split\_inputs parent, tps43\_listener (disabled)

corne.keymap -- 14 layers (0-13), extensive combos and macros

corne.json -- keymap editor metadata

corne\_left.overlay -- central side: enables tps43\_listener, defines layer-based sensitivity overrides

corne\_right.overlay -- peripheral side: i2c0 bus, TPS43 node, pinctrl, input processors

west.yml -- modules: zmk, essenceotd zmk\_driver\_azoteq, zzeneg zmk-raw-hid

build.yaml -- CI matrix: corne\_left raw\_hid\_adapter + corne\_right, both on nice\_nano//zmk



\## Key behaviors tied to the trackpad



\- \*\*Base sensitivity\*\*: 0.9x via `zip\_xy\_scaler 9 10` on peripheral (one-time global reduction)

\- \*\*Y axis inverted\*\*: `zip\_xy\_transform INPUT\_TRANSFORM\_Y\_INVERT` on peripheral (because the pad is mounted sideways)

\- \*\*Hold P (lt 12 P)\*\*: activates HighSens layer, applies `zip\_xy\_scaler 2 1` on central listener → effective 1.8x

\- \*\*Hold V (lt 13 V)\*\*: activates LowSens layer, applies `zip\_xy\_scaler 1 2` on central listener → effective 0.45x

\- \*\*T+D combo\*\*: fires `\&mkp MCLK` (middle click), restricted to layer 0



\## Build and flash



Pushes to the `colemak` branch (default) trigger GitHub Actions. Artifacts show up as `artifact-corne\_left-nice\_nano\_v2` and `artifact-corne\_right-nice\_nano\_v2`.



Flash by double-tapping reset on the Nice!Nano, then dragging the UF2 onto the `NICENANO` USB drive that appears.



\## Known pending fix



As of 2026-10-16, `config/west.yml` is missing the `zzeneg/zmk-raw-hid` module entry. This causes the LEFT side build to fail because the `raw\_hid\_adapter` shield can't be found. The right side still builds. \*\*First task\*\*: restore the module in west.yml. Correct contents:



```yaml

manifest:

&#x20; remotes:

&#x20;   - name: zmkfirmware

&#x20;     url-base: https://github.com/zmkfirmware

&#x20;   - name: essenceotd

&#x20;     url-base: https://github.com/essenceotd

&#x20;   - name: zzeneg

&#x20;     url-base: https://github.com/zzeneg

&#x20; projects:

&#x20;   - name: zmk

&#x20;     remote: zmkfirmware

&#x20;     revision: main

&#x20;     import: app/west.yml

&#x20;   - name: zmk\_driver\_azoteq

&#x20;     remote: essenceotd

&#x20;     revision: main

&#x20;   - name: zmk-raw-hid

&#x20;     remote: zzeneg

&#x20;     revision: main

&#x20; self:

&#x20;   path: config

```



After pushing that fix, there's a secondary known risk: the `raw\_hid\_adapter` shield from `zzeneg/zmk-raw-hid` may not yet be updated to work cleanly with the new Zephyr 4.1 / `nice\_nano//zmk` board variant. If the build still fails with "The selected board is not set up for ZMK and there is a ZMK variant available," the raw\_hid module itself needs attention (either patch its shield to declare ZMK compatibility, or pin the module to a specific commit).



\## GitHub



\- Repo: https://github.com/BrendanTipney/zmk-config

\- Working branch: `colemak`

\- Build workflow: `.github/workflows/build.yml`



\## Driver reference



The essenceotd fork's bindings use different property names than AYM1607's:

\- `single-tap` (not `one-finger-tap`)

\- `two-finger-tap`

\- `three-finger-tap` (on specific branches; software-detected since the IQS5xx hardware suppresses tap gesture when a third finger lands)

\- `enable-power-management` (required for sleep integration)

\- `idle-sleep` (optional, also suspend trackpad during ZMK IDLE state)

\- `sensitivity = <N>`, `scroll-sensitivity = <N>`, `zoom-sensitivity = <N>` (percent)

\- `rst-gpios` (not `reset-gpios`)

\- `rdy-gpios`

\- compatible: `azoteq,tps43`



\## Battery context



Peripheral battery was draining faster than expected prior to the driver migration (AYM1607 driver doesn't suspend the trackpad on ZMK idle). The essenceotd fork integrates with ZMK's activity state machine. CONFIG\_ZMK\_IDLE\_SLEEP\_TIMEOUT is 30 minutes. After the migration takes effect, peripheral drain should drop significantly during idle periods.



\## Preferences



Avoid "AI writing tells" in anything user-facing: no em-dashes, no filler openers, no clichés, no over-bulleting, no hedging. Direct and natural voice.

