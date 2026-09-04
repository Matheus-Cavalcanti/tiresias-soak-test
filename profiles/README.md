# Soak-test firmware profiles

The soak campaign uses one firmware profile per SigmaStudio signal path. This
keeps measured current attributable to the intended application instead of to
unused radio, I2S, logging, or indicator activity.

| Profile | ADAU1787 signal path | nRF5340 workload | Status |
|---|---|---|---|
| `adau-pd` | Supply rails present; `!PD` explicitly held low; no control-port download | Assert one GPIO, log once, then idle | Implemented |
| `ha` | Local microphones through the hearing-aid filter/compressor design | Program ADAU1787 over I2C, then idle | Implemented |
| `ble-dac` | Transparent I2S-to-DAC design | Receive/decode BIS audio and drive I2S | Planned; awaiting SigmaStudio export |
| `ble-ha` | I2S through the hearing-aid filter/compressor design | Receive/decode BIS audio and drive I2S | Planned; awaiting SigmaStudio export |

The current transport implementation is LE Audio broadcast (BIS), so the two
radio profiles use `ble-*` instead of `tws-*` in filenames and reports.

## Build the HA image

The custom-board repository must be placed below a directory literally named
`boards`. `BOARD_ROOT` is the directory that contains `boards`, not the board
repository or the `tiresias_dk` directory itself. A working layout is:

```text
workspace/
|-- boards/
|   `-- eesc-usp/tiresias_dk/
`-- tiresias-firmware/
```

From `tiresias-firmware`, in an nRF Connect SDK environment, use a pristine
build directory. This command was verified with NCS v3.0.1:

```sh
west build -p always -b tiresias_dk/nrf5340/cpuapp --sysbuild . \
  -d build_soak_ha -- \
  -DBOARD_ROOT=/absolute/path/to/workspace \
  -DCONF_FILE=profiles/ha/app.conf \
  -DSB_CONF_FILE=profiles/ha/sysbuild.conf \
  -DEXTRA_DTC_OVERLAY_FILE=profiles/ha/ha.overlay
```

For the layout above, replace `/absolute/path/to/workspace` with the absolute
path of `workspace`. A checkout named `tiresias-boards` does not satisfy the
Zephyr board scanner by itself; either clone it as `boards` or copy its
`eesc-usp/tiresias_dk` directory into `workspace/boards/`.

For the powered-board control with the ADAU1787 explicitly held in hardware
power-down, use:

```sh
west build -p always -b tiresias_dk/nrf5340/cpuapp --sysbuild . \
  -d build_soak_adau_pd -- \
  -DBOARD_ROOT=/absolute/path/to/workspace \
  -DCONF_FILE=profiles/adau-pd/app.conf \
  -DSB_CONF_FILE=profiles/adau-pd/sysbuild.conf \
  -DEXTRA_DTC_OVERLAY_FILE=profiles/adau-pd/adau-pd.overlay
```

This image keeps the existing AVDD, IOVDD and DVDD supply arrangement intact,
configures only the ADAU1787 `!PD` GPIO as an asserted active-low output, emits
one RTT identification message and enters idle. It does not enable I2C and does
not execute a SigmaStudio download. The expected final message is:

```text
ADAU1787 !PD asserted; no I2C/SigmaStudio download; entering idle
```

In the nRF Connect for VS Code build configuration, select
`tiresias_dk/nrf5340/cpuapp`, enable sysbuild and use the same four CMake
arguments shown after `--`. Request a pristine build after changing a profile,
overlay, board definition or SigmaStudio export.

The expected configuration has Bluetooth, CPUNET, I2S, HFCLKAUDIO and LED
workers absent. The application core releases and downloads the ADAU1787 over
I2C, prints one final RTT message, exits `main`, and remains in the Zephyr idle
thread. The ADAU1787 MCLK must come from the external board oscillator.

A successful sysbuild produces `build_soak_ha/merged.hex`. Do not flash an
image just because compilation succeeded: first perform the configuration
checks below and retain the terminal output with the measurement record.

## Measurement acceptance checks

Before a soak run, verify the generated configuration rather than relying on
the build directory name:

```sh
rg 'CONFIG_(TIRESIAS_SOAK_PROFILE_HA|BT|I2S|NRFX_I2S0)=' \
  build_soak_ha/tiresias-firmware/zephyr/.config
rg 'SB_CONFIG_(NETCORE_NONE|NETCORE_IPC_RADIO)=' build_soak_ha/zephyr/.config
```

For `adau-pd`, check
`CONFIG_(TIRESIAS_SOAK_PROFILE_ADAU_PD|BT|GPIO|I2C|I2S|NRFX_I2S0)` in
`build_soak_adau_pd/tiresias-firmware/zephyr/.config` and verify
`SB_CONFIG_NETCORE_NONE=y` in `build_soak_adau_pd/zephyr/.config`. GPIO must be
enabled; Bluetooth, I2C and I2S must be disabled or absent.

Expected values are `TIRESIAS_SOAK_PROFILE_HA=y`, `BT=n`/absent,
`I2S=n`/absent, `NRFX_I2S0=n`/absent, `NETCORE_NONE=y`, and no IPC-radio child
image. NCS v3.0.1 can still display `SB_CONFIG_NRF_DEFAULT_IPC_RADIO=y`; this is
only a default selector and does not create CPUNET when `NETCORE_NONE=y` is the
selected `NETCORE` choice. Record the firmware commit, SigmaStudio export name,
board ID, supply mode and steady-state current with every result.

## SigmaStudio exports

The HA profile currently consumes the generated `tiresias-soak-ha*` files in
`src/SigmaStudioFiles/`. Keep each future export's prefix aligned with its
profile (`tiresias-soak-ble-dac*` or `tiresias-soak-ble-ha*`). The build must
select exactly one export; do not overwrite one profile with another while
retaining the same filename.
