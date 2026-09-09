# TIR-39: external I2C controller for EVAL-ADAU1787Z

This profile turns an independently powered nRF5340 Audio DK into a minimal
ADAU1787 power-state controller. It replaces the USBi during current
measurements, so the EVAL remains powered exclusively by the Power Profiler.
It never downloads a SigmaStudio program.

## Electrical setup

Power the Audio DK from its own USB connector and power the EVAL only from the
Power Profiler at 4.2 V. Connect three signals only:

| Audio DK Arduino header | nRF5340 pin | EVAL | Recommended series resistor |
|---|---|---|---|
| `D9` | `P1.13` | `SDA` | 330 ohm to 470 ohm |
| `D10` | `P1.12` | `SCL` | 330 ohm to 470 ohm |
| `GND` | - | `GND` | Direct |

Do not connect either board's 1.8 V output to the other board. The EVAL must
provide the only SDA and SCL pull-ups, referenced to its IOVDD. The selected
Audio DK pins avoid the DK's on-board I2C bus and its INA231 pull-ups.

Before the measurement, move both I2C address switches, `S4` and `S1`, to
HIGH. This sets `ADDR1=1` and `ADDR0=1`, selecting the 7-bit address `0x2B`.
Disable self-boot with `S2=OFF` and select I2C control mode with `J25` in the
I2C position. As a diagnostic safeguard, the firmware scans all four valid
ADAU1787 addresses (`0x28` through `0x2B`), validates the device identity and
uses the address it finds for the subsequent register writes.

## Safe power order

1. Connect GND, SDA and SCL with both boards off.
2. Power the Audio DK. Do not press either control button.
3. Power the EVAL from the Power Profiler.
4. Perform the hardware-PD baseline with EVAL `J15` closed.
5. Open `J15`, then use the Audio DK buttons described below.
6. After the last measurement, leave both buttons released and power the EVAL
   off first. Power the Audio DK off last.

The TWIM pins are open-drain and have no nRF pull-up. No I2C transaction occurs
until a control button is pressed.

## Controls

| Audio DK button | ADAU1787 state | Final `CHIP_PWR` |
|---|---|---|
| `VOL-` / Button 1 | Software full-chip power-down; no keep-alives | `0x14` |
| `VOL+` / Button 2 | Minimal digital-on; no PLL, DSP, ADC, DAC or SAI | `0x15` |

Each action waits 20 ms for the internal regulator after the manual release of
`!PD`, verifies the ADAU1787 identity, writes the power registers and reads them
back. The digital-on action follows the staged `0x11`, 35 ms, `0x15` sequence
and requires `POWER_UP_COMPLETE=1` before reporting success.

The internal-DVDD experiment uses `J12` open and `J24` ON. For the
external-DVDD experiment, power the EVAL off, move `J24` to OFF, close `J12`,
select EXT on `JP1`, and connect a current-limited 0.9 V source to `J3`. Verify
that the unpowered EVAL has no continuity between that source and an on-board
DVDD regulator before energizing it. The same two firmware commands are used
in both cases.

## Build

In an nRF Connect SDK v3.0.1 environment, from the repository root:

```sh
west build -p always -b nrf5340_audio_dk/nrf5340/cpuapp --sysbuild . \
  -d build_tir39_eval_i2c -- \
  -DCONF_FILE=profiles/eval-i2c/app.conf \
  -DSB_CONF_FILE=profiles/eval-i2c/sysbuild.conf \
  -DEXTRA_DTC_OVERLAY_FILE=profiles/eval-i2c/eval-i2c.overlay
```

The expected output is `build_tir39_eval_i2c/merged.hex`. This profile does
not build or start the network core.

## Measurement matrix and stop condition

At each point, record the 4.2 V input current, DVDD, AVDD, IOVDD and the RTT
readback. Use three stabilized readings.

| ID | `!PD` | `POWER_EN` | DVDD source | Purpose |
|---|---:|---:|---|---|
| I1 | 0 | Not accessible | Internal selected; output collapses | Internal-DVDD hardware-PD baseline |
| I2 | 1 | 0 | Internal LDO | Cost of releasing `!PD` while digitally off |
| I3 | 1 | 1 | Internal LDO | Incremental cost of minimum digital domains |
| E1 | 0 | Not accessible | External 0.9 V remains applied | External-DVDD hardware-PD baseline |
| E2 | 1 | 0 | External 0.9 V | Like-for-like software-PD comparison |
| E3 | 1 | 1 | External 0.9 V | Like-for-like minimum-on comparison |

For external DVDD, include the 0.9 V supply power in the total; comparing only
the EVAL 4.2 V input would omit the digital-domain energy.

The earlier Tiresias measurements imply about 100 mW to 115 mW between
hardware PD and awake-idle. At 4.2 V, an EVAL increase of approximately 24 mA
to 28 mA reproduces that order of magnitude. After I1-I3 and E1-E3 are
repeatable, TIR-39 is complete; further audio or long-duration tests belong to
the soak campaign.
