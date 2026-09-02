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
- The Device Controller subsystem statically observes the state channels required by the
  current PoC so initialization, completion, and fault transitions wake the supervisor.
  Other subsystems may read the latest channel mirror without subscribing when they only
  need a snapshot.
- Completion of a successful command that changes durable state is visible in the
  corresponding state report. Result-event channels are reserved for a later reliability
  stage; the initial PoC does not publish or consume them.
- `ERROR` or `FAULT` states are reserved for persistent subsystem or device failures. A
  rejected command does not by itself place a state machine in an error state.
- A failed state publication does not transfer authority to the stale channel value. The
  owning subsystem retains its private state and treats publication failure as a reporting
  fault that must be logged, retried, or escalated according to subsystem policy.
- At this initial stage, each command channel targeting another subsystem has one policy
  publisher: the Device Controller subsystem. It publishes one semantic request for the
  desired condition and observes state channels for completion. The first implementation
  does not keep per-destination outstanding-command records or correlate results with
  stored command metadata.
- Multiple commands must not be published back-to-back on the same latest-value channel.
  A terminal request such as `POWER_DOWN`, `STOP`, or `DISABLE_RECEIVER` makes the receiving
  subsystem responsible for its internal stop and cleanup sequence.
- If commands or events later need burst delivery, their payloads must use a queued
  delivery mechanism rather than relying on a channel's latest value.
- Externally originated modifying requests require correlation and exact terminal results
  from their first implementation. They must not inherit the initial PoC's optimistic
  no-correlation shortcut merely because an internal lifecycle command uses it.

### Future reliability guardrails

After the initial control flow is validated, consider adding:

- one outstanding-command record per destination;
- command/result correlation and stale-result rejection;
- completion deadlines and timeout events;
- bounded retry policies; and
- escalation rules for repeated or unrecoverable failures.

These guardrails must not change state ownership: state channels remain mirrors of the
receiving subsystem's private state.

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

- The Control Link subsystem is the sole publisher for `LED_1`: blinking means
  advertising, continuously on means connected, and off means disabled or error.
- The Codec Controller subsystem publishes its presentation indication on `LED_2`.
- Additional publishers require an explicit indication-ownership or priority policy to
  prevent different subsystems from issuing conflicting LED commands.

## Device Controller command

Messages published on this channel request whole-device lifecycle or policy behavior, such
as `START`, `LOW_POWER`, `WAKE`, `POWER_OFF`, or `RECOVER`. Codec presentation requests do
not belong on this channel. Only `START` is implemented in the initial PoC; the other
commands reserve the future lifecycle contract.

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
commands include `INITIALIZE`, `SELECT_LOCAL`, `SELECT_BROADCAST`, `POWER_DOWN`, and
`RESET`. `INITIALIZE` configures the codec and starts the local audio path, reaching
`LOCAL_ONLY`. `POWER_DOWN` is a terminal semantic request: when necessary, the Codec
Controller stops presentation internally before reporting `OFF`. The initial PoC implements
only initialization and presentation selection; power-down and reset remain reserved.

### Subscribers and listeners

- The Codec Controller subsystem is statically subscribed.

### Publishers

- The Device Controller subsystem is the sole policy publisher at this initial stage.

## Codec Controller state

Messages on this channel mirror the latest private operational and presentation state owned
by the Codec Controller subsystem, including `OFF`, `INITIALIZING`, `LOCAL_ONLY`,
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

- No subscriber is registered in the initial PoC.

### Publishers

- Codec Controller will be the sole publisher when result reporting is implemented.

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

The target states are `DISABLED`, `ADVERTISING`, `LINKED`, `READY`, and `ERROR`.
`LINKED` represents an ACL without an authorized vendor-protocol session; `READY`
represents a secure and authorized Tiresias session. The current read-only DIS foundation
uses `CONNECTED` for the physical ACL and must be refined before custom writes are added.

### Subscribers and listeners

- The Device Controller subsystem may subscribe when Control Link behavior is implemented.
- Other subsystems may read the latest message when they only need a snapshot.

### Publishers

- Only the Control Link subsystem publishes, whenever its state changes.

## Control Link event

Messages on this channel may carry low-rate Control Link lifecycle outcomes that do not
cause a state transition. It is not the transport for arbitrary remote commands, codec
parameter chunks, or remote responses. Bluetooth callbacks publish or enqueue semantic
events; they do not directly change another subsystem's state.

### Subscribers and listeners

- No subscriber is registered in the initial PoC.

### Publishers

- Control Link will be the sole publisher when its behavior is implemented.

## Remote request and result queues

GATT requests originate outside the firmware and may overlap, retry, carry transaction
identifiers, and require exact errors. The Control Link implementation therefore needs
bounded ordered queues rather than a latest-value public Zbus command channel.

- A GATT callback copies an admitted request to the Control Link inbound queue and returns.
- Control Link routes device-wide policy to a correlated Device Controller request/result
  contract.
- Control Link routes cataloged parameter operations to a dedicated Codec Controller
  request/result contract.
- Result messages preserve the remote transaction identifier and distinguish accepted,
  rejected, completed, failed, timed out, cancelled, and partially applied outcomes as
  appropriate.
- An outbound queue isolates the owning subsystem from GATT indication and notification
  backpressure.

V1 descriptors and bounded single-parameter values may use message queues or Zbus message
subscribers. Future bulk parameter payloads use a fixed buffer pool with explicit
ownership. Neither may be embedded in a latest-value channel or referenced through
callback-owned pointers. The complete remote protocol and codec transfer model is in
[control-link.md](control-link.md).

## Audio Streaming command

Messages on this channel request behavior from the Audio Streaming subsystem. Initial
commands include `ENABLE_RECEIVER`, `START_SCAN`, `STOP_SCAN`, `STOP`, `DISABLE_RECEIVER`,
and `RESET`. `STOP` is valid from any active enabled state and owns the required stop and
cleanup before transitioning to `IDLE`. `DISABLE_RECEIVER` is valid from any enabled state
and owns the required stop, synchronization cleanup, and transition to `DISABLED`.
The initial PoC implements `START_SCAN` (including first-time receiver initialization) and
`STOP`; the other commands reserve the future lifecycle contract.

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

- No subscriber is registered in the initial PoC.

### Publishers

- Audio Streaming will be the sole publisher when result reporting is implemented.

Raw Bluetooth-management and LE Audio callback channels remain internal implementation
details of the Control Link and Audio Streaming subsystems. Both subsystems must consume
ordered copied lifecycle messages. Both now use Zbus message subscribers; a normal
subscriber that rereads only the latest channel value is not sufficient for either the
`CONNECTED` to `SECURITY_CHANGED` to `DISCONNECTED` sequence or the `CONFIG_RECEIVED` to
`STREAMING` sequence. Public control-plane subsystems should depend on the semantic
contracts above rather than on stack-specific events or pointers.

Bluetooth Management is the sole publisher of `bt_mgmt_chan`. Advertising completion and
failure events carry the advertising-set index; ACL lifecycle events carry the connection
index and local role. This lets Control Link accept only its peripheral ACL and advertising
set while Audio Streaming independently handles PA and broadcast events from the same
ordered internal fan-out.

The channel does not grant ownership merely because a subsystem receives every event.
Consumers apply the following filters:

| Consumer | Accepted Bluetooth Management events |
|---|---|
| Control Link | Start/failure for advertising set 0 and connection/disconnection for its peripheral connection index; security is reserved for the later authorized session. |
| Audio Streaming | PA synchronization/loss and other broadcast-reception events. |

Both observers are message subscribers, so each receives its own ordered copy. They do not
compete for one queue entry, and one cannot consume an event before the other sees it.
Borrowed pointer fields are valid only under their Bluetooth lifetime rules; Control Link
uses copied scalar indexes for persistent correlation.

The advertising completion events are necessary because `bt_mgmt_adv_start()` queues work.
Its synchronous return reports admission only. `BT_MGMT_EXT_ADV_STARTED` confirms the
physical procedure, while `BT_MGMT_EXT_ADV_FAILED` carries its asynchronous error. A
subsystem must not publish an advertising semantic state based only on admission.

See [Bluetooth Management](../modules/bluetooth-management.md) for initialization,
resource-ownership, and concurrency details.
