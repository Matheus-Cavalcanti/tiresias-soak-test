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

Before the measurement, set the EVAL to I2C address `0x2B` (`ADDR1=1`,
`ADDR0=1`), disable self-boot and select I2C control mode.

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

The internal-DVDD experiment uses `J12` open and `J24` ON. The external-DVDD
experiment uses `REG_EN=0`, the EVAL external-DVDD routing and a separately
measured 0.9 V source. The same two firmware commands are used in both cases.

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
| A | 0 | Not accessible | Internal selected; output collapses | Hardware-PD baseline |
| B | 1 | 0 | Internal LDO | Cost of releasing `!PD` while digitally off |
| C | 1 | 1 | Internal LDO | Incremental cost of minimum digital domains |
| D | 1 | 0 | External 0.9 V | Like-for-like software-PD comparison |
| E | 1 | 1 | External 0.9 V | Like-for-like minimum-on comparison |

For external DVDD, include the 0.9 V supply power in the total; comparing only
the EVAL 4.2 V input would omit the digital-domain energy.

The earlier Tiresias measurements imply about 100 mW to 115 mW between
hardware PD and awake-idle. At 4.2 V, an EVAL increase of approximately 24 mA
to 28 mA reproduces that order of magnitude. After A-E are repeatable, TIR-39
is complete; further audio or long-duration tests belong to the soak campaign.
