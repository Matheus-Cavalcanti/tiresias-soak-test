# Soak-test firmware profiles

The soak campaign uses one firmware profile per SigmaStudio signal path. This
keeps measured current attributable to the intended application instead of to
unused radio, I2S, logging, or indicator activity.

| Profile | ADAU1787 signal path | nRF5340 workload | Status |
|---|---|---|---|
| `ha` | Local microphones through the hearing-aid filter/compressor design | Program ADAU1787 over I2C, then idle | Implemented |
| `ble-dac` | Transparent I2S-to-DAC design | Receive/decode BIS audio and drive I2S | Planned; awaiting SigmaStudio export |
| `ble-ha` | I2S through the hearing-aid filter/compressor design | Receive/decode BIS audio and drive I2S | Planned; awaiting SigmaStudio export |

The current transport implementation is LE Audio broadcast (BIS), so the two
radio profiles use `ble-*` instead of `tws-*` in filenames and reports.

## Build the HA image

From the firmware repository, use a pristine build directory:

```sh
west build -p always -b tiresias_dk/nrf5340/cpuapp --sysbuild . \
  -d build_soak_ha -- \
  -DCONF_FILE=profiles/ha/app.conf \
  -DSB_CONF_FILE=profiles/ha/sysbuild.conf \
  -DEXTRA_DTC_OVERLAY_FILE=profiles/ha/ha.overlay
```

The expected configuration has Bluetooth, CPUNET, I2S, HFCLKAUDIO and LED
workers absent. The application core releases and downloads the ADAU1787 over
I2C, prints one final RTT message, exits `main`, and remains in the Zephyr idle
thread. The ADAU1787 MCLK must come from the external board oscillator.

## Measurement acceptance checks

Before a soak run, verify the generated configuration rather than relying on
the build directory name:

```sh
rg 'CONFIG_(TIRESIAS_SOAK_PROFILE_HA|BT|I2S|NRFX_I2S0)=' \
  build_soak_ha/tiresias-firmware/zephyr/.config
rg 'SB_CONFIG_(NETCORE_NONE|NETCORE_IPC_RADIO)=' build_soak_ha/zephyr/.config
```

Expected values are `TIRESIAS_SOAK_PROFILE_HA=y`, `BT=n`/absent,
`I2S=n`/absent, `NRFX_I2S0=n`/absent, `NETCORE_NONE=y`, and no IPC-radio child
image. Record the firmware commit, SigmaStudio export name, board ID, supply
mode and steady-state current with every result.

## SigmaStudio exports

The HA profile currently consumes the generated `tiresias-soak-ha*` files in
`src/SigmaStudioFiles/`. Keep each future export's prefix aligned with its
profile (`tiresias-soak-ble-dac*` or `tiresias-soak-ble-ha*`). The build must
select exactly one export; do not overwrite one profile with another while
retaining the same filename.
