# Zbus channels

This document defines the intended Zbus contracts for the control-plane architecture
described in [control-plane.md](control-plane.md). These channels carry low-rate commands,
events, and state reports. Broadcast ISO SDUs, LC3 frames, PCM samples, and I2S buffers
remain in the data plane and must not be transported through these channels.

## Channel conventions

- A command requests behavior from the subsystem that owns it. The receiving subsystem
  validates the request and is the only subsystem allowed to change its own state.
- Every state machine is state-centric. Its authoritative current state is private to the
  owning subsystem; Zbus stores only a non-authoritative copy for observation.
- A state channel is a latest-value, read-only mirror of that private state. The owning
  subsystem is its only publisher and publishes after every accepted state transition.
- A subsystem dispatches commands and events using its private state. It must not read its
  own state channel to determine its current state or recover its internal state from the
  channel.
- The private initial state and the channel's initial message must match before observers
  can receive notifications.
- The Device Controller subsystem statically observes subsystem state channels so
  initialization, completion, and fault transitions wake the supervisor. Other subsystems
  may read the cached value without subscribing when they only need a snapshot.
- A successful command that changes durable state is acknowledged by the corresponding
  state report. A rejected command or a nonfatal failure that leaves the state unchanged is
  reported on the subsystem's result-event channel.
- `ERROR` or `FAULT` states are reserved for persistent subsystem or device failures. A
  rejected command does not by itself place a state machine in an error state.
- A failed state publication does not transfer authority to the stale channel value. The
  owning subsystem retains its private state and treats publication failure as a reporting
  fault that must be logged, retried, or escalated according to subsystem policy.
- At this initial stage, each command channel targeting another subsystem has one policy
  publisher: the Device Controller subsystem. Publishers serialize commands and allow only
  one unacknowledged command per destination. If commands or events later need burst
  delivery, their payloads must use a queued delivery mechanism rather than relying on a
  channel's latest value.

## Button Input event

Messages published on this channel represent debounced button-press events.

### Subscribers and listeners

- The Device Controller subsystem is statically subscribed.

### Publishers

- Only the Button Input subsystem publishes on this channel.

## LED Indicator command

Messages published on this channel request changes to the board's LED indications.

### Subscribers and listeners

- The LED Indicator subsystem receives commands through a static subscriber and processes
  them in its worker thread. It is not a synchronous listener running in the publisher's
  context.

### Publishers

- The Device Controller subsystem is the initial policy publisher.
- Additional publishers require an explicit indication-ownership or priority policy to
  prevent different subsystems from issuing conflicting LED commands.

## Device Controller command

Messages published on this channel request whole-device lifecycle or policy behavior, such
as `START`, `LOW_POWER`, `WAKE`, `POWER_OFF`, or `RECOVER`. Codec presentation requests do
not belong on this channel.

### Subscribers and listeners

- The Device Controller subsystem is statically subscribed.

### Publishers

- The main thread may publish the initial `START` request.
- The Control Link subsystem may publish normalized remote-control requests.
- The Device Controller subsystem performs its own internal transitions directly rather
  than publishing commands to itself.

## Device Controller state

Messages on this channel mirror the latest private internal state of the Device Controller
subsystem. The subsystem's private state remains authoritative.

### Subscribers and listeners

- No static subscriber is required at this initial stage.
- Subsystems that need a snapshot may read the latest message.
- The Control Link subsystem may observe this channel in the future if the companion
  application needs state-change notifications rather than on-demand reads.

### Publishers

- Only the Device Controller subsystem publishes, whenever its state changes.

## Codec Controller command

Messages on this channel request behavior from the Codec Controller subsystem. Initial
commands include `INITIALIZE`, `START_AUDIO`, `SELECT_LOCAL`, `SELECT_BROADCAST`,
`STOP_CODEC`, `POWER_DOWN`, and `RESET`.

### Subscribers and listeners

- The Codec Controller subsystem is statically subscribed.

### Publishers

- The Device Controller subsystem is the sole policy publisher at this initial stage.

## Codec Controller state

Messages on this channel mirror the latest private operational and presentation state owned
by the Codec Controller subsystem, including `OFF`, `INITIALIZING`, `READY`, `LOCAL_ONLY`,
`BROADCAST_ONLY`, and `ERROR`. The subsystem's private state remains authoritative.

### Subscribers and listeners

- The Device Controller subsystem is statically subscribed so codec initialization,
  presentation changes, and faults wake the supervisor.
- Other subsystems may read the latest message when they only need a snapshot.

### Publishers

- Only the Codec Controller subsystem publishes, whenever its state changes.

## Codec Controller result event

Messages on this channel report Codec Controller command outcomes that cannot be represented
by a state transition. For example, `SELECT_BROADCAST` may be rejected while the subsystem
remains `LOCAL_ONLY` because no broadcast stream is available.

### Subscribers and listeners

- The Device Controller subsystem is statically subscribed.

### Publishers

- Only the Codec Controller subsystem publishes on this channel.

## Control Link command

Messages on this channel request behavior from the Control Link subsystem, such as
`ENABLE_CONTROL`, `DISABLE_CONTROL`, and `RESET`.

### Subscribers and listeners

- The Control Link subsystem is statically subscribed.

### Publishers

- The Device Controller subsystem is the sole policy publisher at this initial stage.

## Control Link state

Messages on this channel mirror the latest private state of the Control Link subsystem. The
subsystem's private state remains authoritative. This channel describes the BLE connection
used by the phone or workstation for control and does not represent broadcast
synchronization.

### Subscribers and listeners

- The Device Controller subsystem is statically subscribed so connection and fault
  transitions wake the supervisor.
- Other subsystems may read the latest message when they only need a snapshot.

### Publishers

- Only the Control Link subsystem publishes, whenever its state changes.

## Control Link event

Messages on this channel carry Control Link command outcomes that do not cause a state
transition. Normalized requests received through GATT are published on the Device
Controller command channel. Bluetooth callbacks publish or enqueue semantic events; they
do not directly change another subsystem's state.

### Subscribers and listeners

- The Device Controller subsystem is statically subscribed.

### Publishers

- Only the Control Link subsystem publishes on this channel.

## Audio Streaming command

Messages on this channel request behavior from the Audio Streaming subsystem. Initial
commands include `ENABLE_RECEIVER`, `START_SCAN`, `STOP_SCAN`, `STOP`, `DISABLE_RECEIVER`,
and `RESET`.

### Subscribers and listeners

- The Audio Streaming subsystem is statically subscribed.

### Publishers

- The Device Controller subsystem is the sole policy publisher at this initial stage.

## Audio Streaming state

Messages on this channel mirror the latest private discovery and synchronization state of
the Audio Streaming subsystem. The subsystem's private state remains authoritative. The
mirror reports broadcast availability but does not decide whether the user should hear the
stream.

### Subscribers and listeners

- The Device Controller subsystem is statically subscribed so synchronization progress and
  faults wake the supervisor.
- The Codec Controller subsystem is statically subscribed so entering `STREAMING` makes the
  broadcast path available and leaving `STREAMING` causes `BROADCAST_ONLY` to fall back to
  `LOCAL_ONLY`.
- Other subsystems may read the latest message when they only need a snapshot.

### Publishers

- Only the Audio Streaming subsystem publishes, whenever its state changes.

## Audio Streaming result event

Messages on this channel report command rejections and nonfatal procedure failures that do
not produce a durable Audio Streaming state transition.

### Subscribers and listeners

- The Device Controller subsystem is statically subscribed.

### Publishers

- Only the Audio Streaming subsystem publishes on this channel.

Raw Bluetooth-management and LE Audio callback channels may remain internal implementation
details of the Control Link and Audio Streaming subsystems. Public control-plane
subsystems should depend on the semantic contracts above rather than on stack-specific
events or pointers.
