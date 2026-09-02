# Tiresias DK Firmware

Zephyr/NCS firmware for the Tiresias DK hearing-aid prototype. The application
coordinates the nRF5340, ADAU1787 audio codec and DSP, LE Audio transport,
physical controls, and board indicators.

## Documentation

- [Documentation index](docs/README.md): documentation map, organization, and
  contribution conventions.
- [Development workflow and reminders](docs/development/workflow.md): build
  hygiene, SigmaStudio exports, and project-wide developer guidance.
- [Architecture](docs/architecture/): system context, firmware control plane,
  and hardware views.
- [Module reference](docs/modules/): implementation-level contracts for
  firmware modules.
- [Soak-test profiles](profiles/README.md): matched firmware/SigmaStudio
  configurations for HA, BLE-DAC, and BLE-HA current measurements.

Add cross-cutting workflow guidance and recurring development reminders to the
development guide. Keep module-specific details in the corresponding module
document.
