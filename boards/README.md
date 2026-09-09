# Tiresias Boards

<img align="right" alt="Tiresias Logo" height="160px" src="https://tiresias-website.vercel.app/tiresias-icon-full.png" />

This repository contains the board files for the board used in the Tiresias Project.

## Tiresias DK

The complete design files for the **Tiresias Hearing Aid Development Board** (Tiresias DK) can be found on its [GitHub Repository](https://github.com/joaocolombari/Tiresias_HW).

### ADAU1787 devicetree binding

The Tiresias DK devicetree declares its onboard ADAU1787 with the
`adi,adau1787` compatible. Any application built for this target must provide a
matching binding at `dts/bindings/audio/adi,adau1787.yaml`; the board files do
not install the binding because its schema and driver-facing interface belong to
the application or to a reusable Zephyr module.

A sample binding is available at
`eesc-usp/tiresias_dk/samples/dts/bindings/audio/adi,adau1787.yaml`. Copy it into
the application's `dts/bindings/audio` directory and adapt it if the application
uses a different ADAU1787 interface. The MP3 through MP6 GPIO properties are
optional; applications must check that each property exists before using it.

![Board Assembly](board-pins.png)

### Firmware Pin and IO Map

Pin and IO mappings below were consolidated from `pins.md` and board DTS/DTSI pinctrl/overlay files (`tiresias_dk_nrf5340_*`).
Some pins are multiplexed across domains/features (for example `P1.09` appears as `LED2` and CPUNET `UART0_TX`), so active firmware configuration determines the effective function.

#### Core Interfaces

| Interface      | Signal                       | Pin   |
| -------------- | ---------------------------- | ----- |
| I2C1           | SDA                          | P1.02 |
| I2C1           | SCL                          | P1.03 |
| I2S0           | FSYNC / LRCK                 | P1.15 |
| I2S0           | BCLK / SCK                   | P1.14 |
| I2S0           | DOUT (codec out / nordic in) | P1.13 |
| I2S0           | DIN (codec in / nordic out)  | P1.12 |
| QSPI (MX25R16) | CS                           | P0.18 |
| QSPI (MX25R16) | SCK                          | P0.17 |
| QSPI (MX25R16) | IO0                          | P0.13 |
| QSPI (MX25R16) | IO1                          | P0.14 |
| QSPI (MX25R16) | IO2                          | P0.15 |
| QSPI (MX25R16) | IO3                          | P0.16 |

#### Debug

| Domain | Signal | Pin              |
| ------ | ------ | ---------------- |
| SWD    | SWDIO  | SWDIO            |
| SWD    | SWDCLK | SWDCLK           |
| SWD    | SWO    | P0.11/TRACEDATA0 |
| SWD    | RESET  | RESET            |

#### Board Controls and Sensors

| Block       | Signal | Pin                         |
| ----------- | ------ | --------------------------- |
| User LEDs   | LED0   | P0.26                       |
| User LEDs   | LED1   | P0.27                       |
| User LEDs   | LED2   | P1.09                       |
| User Button | BUTTON | P0.29 (active low, pull-up) |
| BMI270      | INT1   | P0.02/NFC1                  |
| BMI270      | INT2   | P0.03/NFC2                  |

#### ADAU1787 Control GPIOs

| Group        | Signal  | Pin   |
| ------------ | ------- | ----- |
| ADAU control | MP3     | P0.07 |
| ADAU control | MP4     | P0.06 |
| ADAU control | MP6     | P0.04 |
| ADAU control | MP5     | P0.05 |
| ADAU misc    | !PD     | P1.00 |
| ADAU misc    | EXT_CLK | P1.01 |

#### Battery Monitor Notes

| Source                                   | Signal               | Pin   | Status                                                      |
| ---------------------------------------- | -------------------- | ----- | ----------------------------------------------------------- |
| `pins.md`                                | BAT_MON              | P1.04 | Listed, unconfirmed in DTS                                  |
| `pins.md`                                | BAT_MON_EN           | P1.05 | Listed, unconfirmed in DTS                                  |
| `tiresias_dk_nrf5340_cpuapp_common.dtsi` | BAT_MON (SAADC AIN4) | P0.28 | ADC channel configured; voltage-divider nodes commented out |

### External conectors

#### Antenna

| Connector |
| --------- |
| J1        |

#### JST SH Connectors

| Connector | Pin 1   | Pin 2   |
| --------- | ------- | ------- |
| J2        | GND     | VBAT    |
| J3        | HPOUTP1 | HPOUTN1 |
| J4        | HPOUTN0 | HPOUTP0 |
| J5        | GND     | VBUS    |

#### Test Points (TP)

| Test Point | Pin Name             | Description                                                                     |
| ---------- | -------------------- | ------------------------------------------------------------------------------- |
| TP1        | NTC                  | Negative Temperature Coefficient thermistor input (battery temperature sensing) |
| TP2        | MIC0_OUT             | Analog Microphone 0 Output                                                      |
| TP3        | MIC1_OUT             | Analog Microphone 1 Output                                                      |
| TP4        | MICBIAS0             | Microphone 0 bias voltage output                                                |
| TP5        | MICBIAS1             | Microphone 1 bias voltage output                                                |
| TP6        | VBUS                 | Voltage Bus                                                                     |
| TP7        | VDD_LED              | LED supply voltage rail                                                         |
| TP8        | VBAT                 | Battery positive voltage                                                        |
| TP9        | VDD_PMIC_1V8_DIGITAL | 1.8 V digital supply from PMIC                                                  |
| TP10       | VDD_PMIC_0V9         | 0.9 V supply from PMIC                                                          |
| TP11       | BAT_MON_EN           | Battery monitor enable control signal                                           |
| TP12       | BAT_MON              | Battery voltage monitor sense output                                            |
| TP13       | FSYNC_0/MP0          | I²S Frame Sync 0 (multipurpose pin MP0)                                         |
| TP14       | BLCK_0/MP1           | I²S Bit Clock 0 (multipurpose pin MP1)                                          |
| TP15       | SDATAO_0             | Serial Audio Data Output 0                                                      |
| TP16       | SDATAI_0/MP2         | Serial Audio Data Input 0 (multipurpose pin MP2)                                |
| TP17       | FSYNC_1/MP3          | I²S Frame Sync 1 (multipurpose pin MP3)                                         |
| TP18       | BCLK_1/MP4           | I²S Bit Clock 1 (multipurpose pin MP4)                                          |
| TP19       | SDATAI_1/MP6         | Serial Audio Data Input 1 (multipurpose pin MP6)                                |
| TP20       | SDATAO_1/MP5         | Serial Audio Data Output 1 (multipurpose pin MP5)                               |
| TP21       | CS#                  | Quas-SPI Chip Select (active low)                                               |
| TP22       | SCLK                 | Quad-SPI Serial Clock                                                           |
| TP23       | SIO0                 | Quad-SPI IO0                                                                    |
| TP24       | SIO1                 | Quad-SPI IO1                                                                    |
| TP25       | SIO2                 | Quad-SPI IO2                                                                    |
| TP26       | SIO3                 | Quad-SPI IO3                                                                    |
| TP27       | SCL                  | I²C Serial Clock                                                                |
| TP28       | SDA                  | I²C Serial Data                                                                 |
| TP29       | TRACEDATA3           | Debug Trace Data bit 3                                                          |
| TP30       | TRACEDATA2           | Debug Trace Data bit 2                                                          |
| TP31       | TRACEDATA1           | Debug Trace Data bit 1                                                          |
| TP32       | TRACEDATA0           | Debug Trace Data bit 0                                                          |
| TP33       | TRACECLK             | Debug Trace Clock                                                               |
| TP34       | VDD_PMIC_1V8_ANALOG  | 1.8 V analog supply from PMIC                                                   |
| TP35       | GND                  | Analog Ground                                                                   |
| TP36       | GND                  | Analog Ground                                                                   |
| TP37       | GND                  | Ground (selectable with SB5 / NTC reference return)                             |
| TP38       | AIN2                 | Analog Input Channel 2                                                          |
| TP39       | AIN3                 | Analog Input Channel 3                                                          |
| TP40       | DMIC23/MP10          | Digital Microphone Data input (DMIC2/3) / Multipurpose pin MP10                 |
| TP41       | DMIC_CLK1/MP8        | Digital Microphone Clock 1 output / Multipurpose pin MP8                        |

#### Solder Bridges (SB)

| Solder Bridge | Pins             | Description                                                    |
| ------------- | ---------------- | -------------------------------------------------------------- |
| SB1           | VDD_LED - ISET   | Open for 100mA current option                                  |
| SB2           | ISET - GND       | Shorted for 100mA current option                               |
| SB3           | VDD_LED - MODE   | Open for automatic selection between hysteric and PWM modes    |
| SB4           | MODE - GND       | Shorted for automatic selection between hysteric and PWM modes |
| SB5           | NTC - GND        | Open. Short for external NTC                                   |
| SB7           | IOVDD - GND      | Open for internal DVDD regulator                               |
| SB8           | REG_EN - AVDD    | Shorted for internal DVDD regulator enable                     |
| SB9           | REG_EN - GND     | Open for internal DVDD regulator enable                        |
| SB10          | GND - STANDBY    | Open for enabling the oscilator                                |
| SB11          | OUTPUT - MCLKIN  | Shorted for feeding the ADAU with the internal oscilator       |
| SB12          | EXT_CLK - MCLKIN | Open. Short and cut SB11 for external CLK in the ADAU          |
| SB13          | VSYS - CE        | Shorted for CE active high                                     |
| SB14          | VSYS - CE        | Shorted for CE active high                                     |
