# Documentation

The documentation is organized by purpose so readers can move from system
intent to implementation details without mixing abstraction levels.

## Architecture

Architecture documents describe system boundaries, major components, and
design responsibilities. They should explain what the system is and why it is
structured that way, without becoming source-level API references.

- [System context](architecture/system-context.md): intended product context
  and the current proof-of-concept environment.
- [Firmware control plane](architecture/control-plane.md): subsystem naming,
  state-machine ownership, and coordination.
- [Threads and execution contexts](architecture/threads-and-contexts.md): control-plane
  threads, callback boundaries, data-plane workers, and implementation status.
- [Zbus channels](architecture/zbus.md): private-state mirroring, control-plane publishers,
  subscribers, acknowledgement behavior, and delivery constraints.
- [Hardware](architecture/hardware.md): principal hardware components and
  physical interfaces.

## Development

Development documents describe workflows, generated artifacts, build
expectations, tool limitations, and recurring troubleshooting guidance.

- [Development workflow and reminders](development/workflow.md): build hygiene,
  SigmaStudio export handling, and the canonical home for cross-cutting
  developer reminders.

## Modules

Module documents describe implementation-level responsibilities, public APIs,
configuration requirements, and message contracts.

- [Button Input subsystem](modules/button.md): button GPIO handling and Zbus events.
- [ADAU1787 startup](modules/adau1787-startup.md): codec reset, I2C programming,
  SigmaStudio download, I2S activation, status checks, and failure behavior.
- [LED Indicator subsystem](modules/led.md): LED commands, GPIO handling, and blink behavior.

## Where New Documentation Belongs

- Put system boundaries, major design decisions, and cross-subsystem behavior
  in `architecture/`.
- Put build, tooling, generated-code, flashing, and repository-wide workflow
  guidance in `development/`.
- Put source-level contracts and module extension guidance in `modules/`.
- Keep assets next to the document that owns them unless several documents
  share the same asset.

Prefer focused documents with descriptive names. Link new documents from this
index and from the repository README when they are important entry points.
