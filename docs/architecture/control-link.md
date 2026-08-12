# Control Link and Remote Management Architecture

## Status and purpose

This document is the conceptual design for the Control Link subsystem and its BLE
interface. It defines the intended boundaries, protocol shape, security model, Codec
Controller integration, and future relationship with an Auracast Broadcast Assistant. It
does not define final UUID values, byte-level packet encodings, or source-level APIs.

The first Control Link foundation is implemented: the Device Controller enables it during
startup, shared Bluetooth Management performs idempotent stack initialization, the device
advertises connectably, and the standard Device Information Service is available to one
connected peer. The custom Tiresias service, authorization/session negotiation, status,
parameter catalog, and codec-memory operations below remain the target for later
milestones.

## Architectural decision

Tiresias should expose **one vendor-specific Tiresias Control Link Service** for behavior
that is unique to this product, but it should not put every BLE function into that service.
The service should contain several focused characteristics and a versioned application
protocol. Bluetooth SIG services should remain separate whenever they already define the
required interoperable behavior.

The intended GATT server therefore contains:

| Service | Responsibility |
|---|---|
| Tiresias Control Link Service | Tiresias-specific commands, dynamic status, events, a discoverable parameter catalog, and authorized parameter access. |
| Device Information Service | Manufacturer, model, firmware revision, and other standard static identity fields. This is already enabled. |
| Broadcast Audio Scan Service (BASS) | Standard Broadcast Assistant to Scan Delegator procedures for Auracast source assistance. |
| Firmware-update service | Existing MCUmgr/SMP transport and image-management protocol. It must not be reimplemented inside the Tiresias service. |
| Battery Service | Battery status if a future board can measure it. The current board cannot provide a valid battery level. |
| Other adopted audio services | VCS, HAS, PACS, CSIS, or related services when their standardized semantics match a product feature. The custom protocol should not duplicate them merely for application convenience. |

This is a single custom service, not a single all-purpose characteristic and not a single
service for the entire product. Keeping Tiresias-specific operations together gives the
client one capabilities and version contract. Keeping BASS, device information, firmware
update, and future standardized controls separate preserves interoperability and avoids a
vendor-specific Auracast protocol.

A second vendor service should be introduced only if a future function has a materially
different lifecycle, authorization boundary, or sustained data-rate requirement. A
diagnostic sample stream could meet that threshold; another low-rate command would not.

## Responsibilities and boundaries

The Control Link subsystem owns:

- the availability and lifecycle of the remote-management session;
- protocol negotiation, request parsing, transaction identifiers, and response delivery;
- peer-level security and application authorization policy for the custom service;
- conversion between the wire protocol and internal semantic requests;
- read-only aggregation of subsystem state for remote visualization;
- exposure of the build-time parameter catalog and routing of parameter operations;
- flow control and buffer ownership for future bulk management transfers; and
- cancellation of peer-owned operations when the peer disconnects.

It does not own:

- Bluetooth stack initialization or global controller resources;
- ADAU1787 I2C access or parameter validation against codec state;
- system lifecycle, audible-mode, or broadcast-selection policy;
- PA, BASE, BIG, or BIS synchronization procedures;
- BASS receive-state semantics;
- firmware-image transfer and validation; or
- ISO, LC3, PCM, or I2S audio data.

Those responsibilities remain with the shared Bluetooth Management module, Codec
Controller subsystem, Device Controller subsystem, Audio Streaming subsystem, Zephyr/NCS
management infrastructure, and audio data plane respectively.

## Independent BLE control and broadcast reception

The control peer and broadcast source are independent. The control peer establishes a
bidirectional LE ACL connection to the Tiresias GATT server. A broadcast source transmits
periodic advertising and one or more BISes; it does not establish an ACL connection to the
hearing aid. Consequently, all of these conditions are valid:

| Control Link | Audio Streaming | Meaning |
|---|---|---|
| `DISABLED` or `ADVERTISING` | `STREAMING` | The device receives a broadcast without a control application connected. |
| `READY` | `IDLE` or `SCANNING` | A phone or workstation controls and observes the device while no broadcast is playing. |
| `READY` | `STREAMING` | Remote management and broadcast reception operate simultaneously. |
| `LINKED` | Any enabled state | A peer is using standard GATT services, such as BASS, without an authorized Tiresias protocol session. |

The nRF5340 radio and controller still share finite scheduling and memory resources. The
combination of one peripheral ACL, active scanning or periodic-advertising synchronization,
and BIG/BIS synchronization must be supported by the selected controller configuration and
verified under worst-case audio load. Connection interval, supervision timeout, scan duty
cycle, ATT MTU, ACL buffers, periodic-sync count, ISO channels, and controller memory are
resource settings, not reasons to couple the two subsystem state machines.

The initial product policy should allow one incoming control/Broadcast Assistant ACL. A
future multi-peer design must add explicit connection ownership and conflict policy rather
than treating every `BT_MGMT_CONNECTED` event as the Control Link. Coordinated left/right
devices are outside the intended V1 scope.

## Control Link state model

The proposed state model distinguishes the physical ACL from an authorized custom-protocol
session:

| State | Meaning |
|---|---|
| `DISABLED` | The connectable control interface is unavailable. |
| `ADVERTISING` | The device is accepting an incoming ACL connection. |
| `LINKED` | An ACL exists, but a Tiresias custom-protocol session is not yet authorized and negotiated. Standard services remain available according to their own permissions. |
| `READY` | The peer is secure, authorized for its role, and may exchange Tiresias requests and responses. |
| `ERROR` | A persistent local Control Link failure prevents continued operation. |

The intended transitions are:

- `ENABLE_CONTROL` registers or enables the service, requests connectable advertising,
  and moves `DISABLED` to `ADVERTISING` after advertising actually starts.
- An accepted ACL moves `ADVERTISING` to `LINKED`.
- Successful security, authorization, and protocol negotiation move `LINKED` to `READY`.
- Failure or expiry of authorization disconnects the peer or leaves standard services
  available in `LINKED`, according to product policy.
- Disconnection from `LINKED` or `READY` cancels peer-owned transactions and returns to
  `ADVERTISING`.
- `DISABLE_CONTROL` stops advertising, closes the session, cancels transactions, and
  reaches `DISABLED` from any non-error state.
- Only persistent local failures enter `ERROR`; malformed, unauthorized, busy, or
  rejected remote requests receive protocol errors without changing subsystem state.

The foundation implementation currently uses `CONNECTED` for the physical ACL because it
exposes only read-only DIS information. It must add the `LINKED`/`READY` distinction, or
preserve an equivalent internal session state, before custom remote writes are accepted.

## Tiresias Control Link Service

The service should use a project-owned 128-bit UUID and the following logical
characteristics. Exact UUID assignments and maximum encoded sizes belong in the later wire
protocol specification.

| Characteristic | GATT properties | Purpose |
|---|---|---|
| Protocol Information | Read | Supported protocol major/minor versions, feature bits, maximum request and transfer sizes, parameter-layout identity, and current boot/session identifier. |
| Parameter Catalog | Read, including offset reads | Versioned description of every remotely configurable parameter: stable identifier, current codec address, data format, bounds, units, and access flags. |
| Status | Read, Notify | Coherent device-state snapshot and low-rate state-change notification. |
| Request | Write | Reliable submission of a framed command or transaction-control request. |
| Response | Indicate | Correlated acceptance, progress when necessary, terminal result, and structured error information. |
| Event | Notify | Unsolicited, recoverable events that do not require one indication acknowledgement per event. |
| Transfer Data | Write, optionally Write Without Response; Notify | Flow-controlled codec parameter chunks for operations too large for the Request or Response characteristic. |

The Parameter Catalog should be available in the first read-only milestone. Its value may
be larger than the negotiated ATT MTU, so it supports ordinary GATT offset/Read Blob access
over one immutable snapshot. A header supplies total encoded length, catalog version,
layout identifier, entry count, and an integrity value. V1 must assert at build time that
the complete encoded catalog fits the supported GATT characteristic-value limit. If it
does not, the build must fail until the same records are exposed by a catalog-read request
and Transfer Data; the service must never truncate the catalog.

`Transfer Data` can otherwise be omitted from V1. Reserving it in the protocol design
prevents future parameter dumps, batched updates, or profile transfer from being forced
through a command point. Write Without Response should be enabled only when the protocol
has explicit credits or a window that prevents a client from overrunning the bounded
receive pool. Ordinary Write remains the reliable fallback.

The service should not expose native C structures. Every multi-byte value has an explicit
wire byte order and width. Each message includes at least a protocol version, message type
or opcode, transaction identifier, payload length, and flags. Extensible fields should use
a length-delimited encoding so a newer peer can skip data it does not understand.

The protocol major version changes only for incompatible framing or semantic changes. A
minor version adds backward-compatible operations or fields. Capability bits, rather than
minor-version guesses, determine whether an optional operation is available. A client
must read Protocol Information before issuing a modifying request and must reject a codec
layout it does not recognize.

### Parameter Catalog

The Parameter Catalog is the authoritative remote allowlist. The workstation uses it to
construct controls and encode values without carrying a hard-coded copy of the current
SigmaStudio map. Each record contains at least:

- a stable parameter identifier used by read and write requests;
- the current ADAU1787 parameter-memory byte address and number of words;
- a format identifier, width, signedness, byte order, and scale or fractional-bit
  information needed to encode the value;
- minimum, maximum, and optional step values expressed in the declared wire format;
- access flags such as readable, writable, volatile, and live-update-safe;
- a unit identifier and a stable display-name or localization key; and
- enumerated choices when the value is not a continuous numeric range.

The catalog may also expose a default value and a short description when useful to the
research interface. Descriptive strings must remain length-delimited and bounded.

The workstation reads and presents the codec address for inspection, but normal
`GET_PARAMETER` and `SET_PARAMETER` requests identify the parameter by its stable ID rather
than trusting a client-supplied address. Control Link resolves the ID through the active
catalog, and Codec Controller validates the declared type, encoded length, range, access
flags, and current codec state before using the cataloged address. A request with a stale
layout/catalog identity is rejected.

The catalog and the firmware-side lookup table must be generated at build time from one
source of truth associated with the SigmaStudio export. The firmware must not parse a
host-supplied parameter description at runtime. Compile-time generation keeps the exposed
records, validation metadata, and codec addresses identical.

The catalog describes parameters but does not duplicate their changing live values.
The workstation uses `GET_PARAMETER` to read a current value and `SET_PARAMETER` to request
a new one using the encoding declared by the corresponding catalog record.

V1 reads or writes one cataloged parameter per correlated request. This fits ordinary
Request and Response messages for the expected one-word and small multi-word parameters
and does not require a bulk transfer session. Batched values and full parameter snapshots
remain later uses of Transfer Data.

### Requests, responses, and events

A successful ATT Write response means only that the Request was well formed and accepted
into a bounded firmware queue. It does not mean the requested device or codec operation
has completed. The later Response indication carries the same transaction identifier and
reports the authoritative outcome from the owning subsystem.

The protocol needs correlation from its first modifying implementation. The optimistic
latest-state model used by the initial internal PoC is insufficient at an external API
boundary because the client must distinguish completion, rejection, timeout, partial
application, and disconnect cancellation.

At minimum, terminal results should distinguish:

- success;
- malformed or unsupported request;
- incompatible protocol or parameter layout;
- unauthenticated or unauthorized operation;
- invalid state, range, alignment, or value;
- busy or no buffer/credit available;
- timeout or cancellation;
- codec communication failure; and
- partially applied operation, only for an operation whose documented semantics permit
  partial application.

Unsolicited Event notifications carry a monotonically increasing sequence number. If the
client detects a gap, it rereads Status rather than assuming it can reconstruct current
state from notifications. Status is a snapshot; Event is a prompt to refresh or a record of
a transient occurrence.

### Remote information model

Protocol Information and the standard Device Information Service provide static or
session-stable data. Status should provide the minimum coherent dynamic view needed by a
control application:

- Device Controller state;
- Control Link state and authorized role;
- Audio Streaming state;
- Codec Controller state and active presentation mode;
- selected/active broadcast summary when one exists;
- active parameter-layout identifier and parameter revision;
- last relevant fault category and error code; and
- capability or availability flags for commands that depend on current state.

The custom Status value may summarize the active broadcast for one Tiresias user
interface, but BASS Broadcast Receive State remains the normative interoperable view for a
Broadcast Assistant. Broadcast codes and other secrets must never be copied into generic
status or diagnostic notifications.

Low-rate state and counters belong here. Audio frames, PCM samples, raw microphone audio,
and unbounded logs do not. A future sustained diagnostic or sensor stream needs its own
bounded data path and explicit resource policy.

## Codec parameter catalog and memory access

### Meaning of full access

V1 should expose every parameter intentionally made remotely configurable by the active
SigmaStudio image. The Parameter Catalog, not the physical RAM range, defines that set.
Every catalog entry can be inspected and, when its access flags permit, changed by the
trusted research workstation. Uncataloged words cannot be addressed through normal V1
requests.

This does not expose ADAU1787 program RAM, data RAM, control registers, power controls, the
safeload machinery, or arbitrary parameter RAM as a generic remote memory bus. A raw
developer-memory operation can be designed later if research demonstrates a need, but it
must be a distinct compile-time capability rather than an undocumented escape from catalog
validation.

The source manifest used to generate the catalog contains:

- the SigmaStudio image/layout identifier or cryptographic digest;
- parameter-memory base and end addresses;
- the four-byte parameter-word width and required alignment;
- reserved ranges, including the safeload mailbox and firmware-owned control words;
- the stable ID, address, word count, format, range, units, and access flags of every
  cataloged parameter; and
- the conversion metadata needed to turn the declared wire value into its SigmaDSP word
  representation.

The current driver declares `0x2000` through `0x3FFF`, an 8192-byte space containing 2048
aligned parameter words. The generated manifest, not those range constants alone, must
identify which words belong to the application and which are reserved for codec or
firmware machinery.

Catalog addresses use the ADAU1787 external control-port byte-address namespace used by the
generated SigmaStudio headers. The wire value uses the format declared by the record;
Codec Controller performs the defined conversion to or from SigmaDSP words. The device
does not infer a host numeric representation or accept an encoded length different from
the catalog entry.

### Ownership and routing

Only the Codec Controller subsystem may read or write the ADAU1787. The Control Link
validates framing, connection authorization, catalog/layout identity, parameter ID,
encoded size, and queue capacity, then submits a copied codec request to the Codec
Controller's queued request port. The Codec Controller independently resolves the catalog
entry and validates codec state, access flags, type, range, address, alignment, and safety
policy before calling the hardware codec or ADAU1787 driver.

The request and response payloads must not use a latest-value Zbus channel. V1 descriptors
and bounded parameter values use an ordered message queue or message subscriber. Future
bulk chunks are burstable, require once-and-in-order delivery, and add a fixed buffer pool.
A response owns its copied data until Control Link has transmitted or discarded it; neither
subsystem may pass a pointer to a callback-owned or stack buffer across the boundary.

The initial implementation should allow one outstanding parameter operation. This
serializes I2C ownership, bounds RAM, and makes cancellation deterministic. The Control
Link thread should remain event driven while it waits: it records the outstanding
transaction and processes completion or disconnection events instead of blocking on the
Codec Controller.

### Future bulk transfer session

V1 single-parameter operations do not require a transfer session. A future parameter dump,
batch update, or profile operation should use an application-level transfer session rather
than a GATT long write. A session identifies the operation, catalog/layout identity,
parameter set or declared range, total length, transfer mode, integrity value, and
transaction identifier. Chunks carry the session identifier, offset, length, and data. The
negotiated ATT MTU influences chunk size but never changes operation semantics.

For a read:

1. The peer opens a read session for a declared set of cataloged parameter IDs.
2. Codec Controller serializes the read with all codec writes.
3. Firmware emits flow-controlled Transfer Data chunks and a final integrity value.
4. Response reports success only after all data has been handed to the link, or reports
   cancellation/failure with the last confirmed offset.

For a write:

1. The peer opens a write session with catalog/layout identity, a declared parameter set,
   mode, total length, and an expected integrity value.
2. Control Link admits chunks according to credits and stores them in bounded owned
   buffers.
3. Codec Controller resolves and validates every catalog entry and applies it using the
   declared mode.
4. Codec Controller returns the applied revision and exact outcome.
5. Control Link emits the terminal Response and releases the session.

Disconnect, authorization loss, Device Controller shutdown, codec reset, or timeout
cancels the session. Cancellation must define whether no words, complete safeload groups,
or a staged image may already have been applied.

### V1 live writes and future maintenance writes

V1 supports only volatile writes to catalog entries marked live-update-safe. Codec
Controller applies each value through the ADAU1787 safeload mechanism. It rejects a write
to an entry that cannot be safely updated while the current audio path is active. V1 does
not promise atomicity across several independent `SET_PARAMETER` requests and does not
implement whole-profile replacement or a muted maintenance mode.

The longer-term modes are:

| Mode | Intended behavior |
|---|---|
| Live patch | Apply eligible words through ADAU1787 safeload in groups supported by the driver. Each accepted safeload group is atomic at the audio-frame boundary; a multi-group transaction is not globally atomic. |
| Maintenance apply | Mute or quiesce presentation, validate a staged range or image, apply parameters that are not live-safe, verify the result, and restore presentation or a known baseline. |

The current driver supports safeload groups of at most five four-byte words. A V1 catalog
entry must fit the supported live update or be exposed as read-only. A future bulk request
may be split into several safeload groups, but must report partial application if a later
group fails. A future all-or-nothing profile replacement should stage and verify the
complete image before entering a short muted apply phase; it must not claim atomicity
merely because individual safeload groups are atomic.

V1 parameter writes are volatile and are lost when the codec is reinitialized.
Persistent clinical profiles require a separate versioned storage design with integrity,
power-loss-safe commit, rollback, layout migration, safe-limit validation, and an explicit
commit operation. Persistence should not be an accidental flag on an individual write.

### Cataloged and standardized parameters before raw parameters

Common wearer operations such as volume, mute, listening mode, preset selection, or a
bounded personalization control should use semantic commands or an adopted standard
service. They should not require the application to know a SigmaStudio address. Semantic
operations remain stable when the DSP graph changes and can enforce clinical limits.

The catalog is the V1 bridge for Tiresias-specific research parameters. It exposes their
DSP addresses for transparency while keeping stable IDs and device-side range enforcement.
Raw address operations are outside V1. If later added for diagnostics or migration, a
matching layout identifier remains mandatory and the device remains authoritative for
address, range, state, and authorization checks.

## Control plane, management transfer, and audio data plane

Parameter bytes are larger than ordinary commands, but they are still configuration data;
they are not real-time audio data. The architecture should distinguish three paths:

| Path | Examples | Firmware transport |
|---|---|---|
| Control plane | Commands, authorization, transaction open/commit/abort, state mirrors, results, low-rate events, BASS procedure requests | GATT request/response/status characteristics, semantic subsystem messages, and Zbus state channels |
| Bulk management transfer | Future parameter dumps, batched writes, profiles, or a bounded diagnostic snapshot | GATT Transfer Data, a fixed buffer pool, and ordered queues with credits |
| Audio data plane | Broadcast ISO SDUs, LC3 frames, decoded PCM, I2S blocks | Bluetooth ISO callbacks, dedicated FIFOs, audio workers, DMA buffers, and hardware DSP |

The bulk management path supports a control-plane operation and is scheduled below audio
deadlines. It must never share the ISO/LC3/PCM queues, and audio buffers must never pass
through the custom GATT service. High-rate telemetry would become a separate data-plane
concern; ordinary visualization remains low-rate control-plane state.

## Integration with the existing firmware

### Shared Bluetooth initialization and resources

Bluetooth is a platform resource shared by Control Link and Audio Streaming. The current
compiled path initializes Bluetooth from `audio_streaming_actions_start()`. That ownership
must be changed before Control Link can start independently.

The Bluetooth Management module should provide one idempotent initialization path for
`bt_enable()`, settings, callbacks, controller configuration, and advertising support.
Device startup coordinates that shared readiness, after which:

- Control Link requests the connectable advertising lifecycle and consumes ACL/security
  events relevant to its retained peer;
- Audio Streaming requests scanning, PA/BIG/BIS procedures, and consumes broadcast
  lifecycle events; and
- the advertising implementation composes the required custom-service discovery data,
  BASS solicitation data, and other enabled service data into the available advertising
  set rather than letting two subsystems create conflicting sets.

This shared module performs Bluetooth procedures but does not become the owner of remote
policy, broadcast-selection policy, or either subsystem's semantic state.

### Callback and thread flow

The intended request flow is:

1. A GATT callback checks attribute permissions and the minimum framing needed to reject a
   malformed write.
2. It copies the request into a bounded Control Link queue and returns without waiting.
3. The Control Link thread validates protocol/session policy and assigns the request to its
   semantic owner.
4. A device-wide or audible-policy request goes to Device Controller. A codec parameter
   request goes through the dedicated queued Codec Controller request port. A BASS request
   follows the Audio Streaming integration described below.
5. The owning subsystem validates current state and performs the operation in its own
   execution context.
6. A correlated result returns to Control Link, which constructs the Response indication
   and updates Status when durable state changed.

ATT callbacks never perform I2C, wait for Zbus, allocate an unbounded buffer, or decide
device policy. The Control Link thread never calls the ADAU1787 driver directly.

Connection, security, advertising, and disconnection events are ordered lifecycle events.
Control Link consumes copied messages through a Zbus message subscriber so a
`CONNECTED` to `SECURITY_CHANGED` to `DISCONNECTED` burst cannot collapse into one latest
channel value.

### Internal message contracts

Existing state channels remain useful for visualization. Control Link may statically
observe Device Controller, Codec Controller, and Audio Streaming state channels, maintain
a compact remote-status snapshot, and notify the peer after a change. The state channel is
still only a mirror; Control Link does not modify it or infer a remote operation's terminal
result solely from an unrelated state notification.

Remote modifying requests require new queued contracts:

- a Control Link inbound-request queue, owned by Control Link;
- correlated semantic request/result messages for Device Controller operations exposed
  remotely;
- a Codec Controller parameter request queue and result queue with fixed payload-buffer
  ownership; and
- an outbound response/event queue so Bluetooth transmission backpressure cannot block the
  owning subsystem.

Latest-value command channels remain suitable only for the existing single-publisher,
non-bursting internal lifecycle commands. They are not the transport for externally
originated commands that can overlap, retry, or require an exact result.

### Device startup and failure policy

On `START`, Device Controller should request shared Bluetooth readiness once, then start
Codec Controller, Audio Streaming, and Control Link according to product policy. Control
Link normally receives `ENABLE_CONTROL` and begins advertising, but it does not need a
connected peer for Device Controller to reach `OPERATIONAL`. The existing codec and Audio
Streaming readiness conditions remain the required PoC path; inability to advertise is a
reported degraded capability unless the selected operating mode explicitly requires a
remote peer.

Control Link should be optional for audio operation. A failure to advertise or establish a
remote session must not stop local audio or an already synchronized broadcast unless the
selected product mode explicitly declares remote control mandatory. Device Controller may
report degraded remote availability while remaining `OPERATIONAL`.

Codec and Audio Streaming failures remain authoritative in their owning subsystems. A
remote protocol or authorization error is returned to the peer. A persistent local BLE
failure enters Control Link `ERROR`; only a truly shared Bluetooth-stack failure should be
escalated as a device-level fault that may also affect Audio Streaming.

## Evolution into an Auracast companion

The companion application can evolve into a Broadcast Assistant without changing the
Tiresias custom protocol. The same physical ACL can carry both the custom Tiresias service
and standard BASS procedures:

1. The phone or workstation connects to the hearing aid as a GATT client.
2. It discovers the Tiresias Control Link Service for product control and BASS for
   Broadcast Assistant procedures.
3. It scans for Broadcast Sources, presents discovered broadcasts to the user, and obtains
   a Broadcast Code when necessary.
4. As a BAP Broadcast Assistant, it writes the selected source and requested synchronization
   information through the BASS Broadcast Audio Scan Control Point. PAST may be used when
   supported to reduce scanning by the hearing aid.
5. The Scan Delegator callbacks enqueue semantic source requests for Audio Streaming.
   Audio Streaming owns PA synchronization, BASE validation, BIS selection, stream startup,
   recovery, and teardown.
6. Audio Streaming updates BASS Broadcast Receive State as PA/BIS synchronization changes,
   allowing the assistant to observe the standard result. Device Controller separately
   decides whether an available stream should become audible, and Codec Controller owns the
   actual local/broadcast presentation switch.

The current configuration enables Zephyr's Scan Delegator capability, and the Bluetooth
Management sources already contain BASS callbacks. The compiled subsystem path does not
currently call `bt_mgmt_scan_delegator_init()`: only the excluded legacy `bluetooth.c` path
does so. It therefore does not register the Scan Delegator, advertise BASS solicitation
data, or route assistant requests through the new Audio Streaming state machine. That work
belongs to a later BASS integration milestone, not to vendor-specific source-selection
opcodes.

A control peer may also be collocated with a Broadcast Source. The ACL control connection
and broadcast BIS are still distinct links and roles even when one phone implements both.
Simultaneous operation remains subject to controller resources and radio scheduling.

## Security and authorization

V1 has one application access class: a trusted research workstation. The device accepts
one retained peer, opens pairing during an explicit physical-presence window, bonds that
workstation, and enters `READY` only for the authorized bond on an encrypted connection.
Protocol Information may remain minimally discoverable before authorization; Status,
Parameter Catalog, commands, responses, events, and parameter values should require the
authorized session.

Trusting the workstation does not remove device-side validation. V1 still:

- accepts only cataloged parameter IDs and their exact declared encodings;
- checks every value against the catalog range and access flags;
- permits only one outstanding parameter operation;
- bounds request sizes, rates, queues, and timeouts before allocation or I2C access;
- cancels outstanding work on disconnect, authorization loss, or codec reset;
- avoids logging or exposing Broadcast Codes, credentials, or other secrets;
- defines bond removal and research recovery procedures; and
- reviews MCUmgr/SMP permissions under the same research threat model rather than assuming
  they are safe because they use a separate service.

Wearer, clinician, and developer roles are deliberately outside V1. The wire protocol
retains an authorization context so those roles and a clinical safety envelope can be
introduced later without changing parameter IDs or request framing. Raw arbitrary-memory
access, if ever added, belongs to a future developer-only capability and is not implied by
the trusted-workstation role.

## Resource and quality-of-service policy

Audio deadlines have priority over remote management. The first implementation should:

- limit the system to one Control Link peer and one outstanding parameter operation;
- use fixed-size request descriptors and bounded encoded parameter values;
- support an immutable offset-readable catalog snapshot for the duration of a boot;
- reserve transfer buffers, chunk-size negotiation, and credits for later bulk operations;
- rate-limit status and parameter notifications;
- reject writes to parameters that are not marked live-update-safe;
- abort transactions promptly on disconnect or codec reset; and
- measure stack, queue, controller-memory, and audio underrun behavior before increasing
  concurrency or throughput.

Remote throughput is an optimization target only after uninterrupted audio and bounded
latency are demonstrated.

## Incremental implementation plan

### Milestone 1: shared BLE foundation and read-only control link

- Shared, idempotent Bluetooth initialization, connectable advertising, ordered copied
  lifecycle events, automatic advertising restart, DIS exposure, and LED 1 lifecycle
  indication are implemented as the foundation.
- Register the custom service and add its UUID to connectable advertising data.
- Implement retained peer handling and authorization/session negotiation.
- Implement `DISABLED`, `ADVERTISING`, `LINKED`, `READY`, and `ERROR` semantics.
- Generate the Parameter Catalog and firmware lookup table from one build-time source.
- Add Protocol Information, the offset-readable Parameter Catalog, and a read-only Status
  snapshot.
- Fail the build if the V1 catalog cannot be exposed completely within the supported GATT
  characteristic-value limit.
- Require an encrypted authorized bond to the single trusted research workstation.

### Milestone 2: V1 commands and volatile parameter access

- Add Request, Response, and Event characteristics.
- Define the versioned wire header, capability bits, result codes, timeouts, and
  cancellation behavior.
- Route device-wide requests through Device Controller and codec-owned semantic controls
  through Codec Controller.
- Implement `GET_PARAMETER` and `SET_PARAMETER` by stable ID with exact format, bounds,
  catalog identity, and live-update-safe validation.
- Apply writable catalog entries through safeload and keep all V1 changes volatile.
- Add correlation and bounded pending-operation storage from the beginning.

### Milestone 3: post-V1 bulk transfer and persistence

- Add Transfer Data, fixed transfer buffers, credits, and one active bulk transaction only
  when parameter snapshots, batches, or profiles are required.
- Define exact partial-application and cancellation behavior for multi-parameter updates.
- Add maintenance apply and rollback only if future parameters require muted updates.
- Design a power-loss-safe, versioned profile store before adding an explicit persistent
  commit operation.

### Milestone 4: Broadcast Assistant integration

- Register BASS/Scan Delegator in the compiled subsystem path.
- Add required solicitation and service advertising data.
- Route BASS callbacks to ordered Audio Streaming requests.
- Keep BASS Broadcast Receive State synchronized with PA and BIS state.
- Exercise simultaneous custom control, BASS procedures, scanning, and BIS reception.

### Milestone 5: production hardening

- Finalize wearer, clinician, and maintenance credentials and permissions.
- Add audit-safe operation records without sensitive payload logging.
- Measure queue depths, stack high-water marks, radio coexistence, throughput, timeouts,
  malformed-request handling, and recovery under sustained audio.

## Confirmed V1 scope

- The Parameter Catalog defines the complete remotely configurable set and exposes stable
  IDs, current addresses, formats, ranges, units, and access flags.
- One trusted, bonded research workstation is the only custom-service client role.
- Parameter writes are volatile; persistence is a future explicit profile feature.
- V1 performs individual live-safe parameter updates and does not require muted apply,
  multi-parameter atomicity, or whole-profile replacement.
- Coordinated left/right device management is out of scope.
- Standard services such as VCS or HAS can be adopted later for matching product controls
  without changing the Tiresias parameter catalog.
- A future phone may be a control peer, Broadcast Assistant, Broadcast Source, or a
  combination of those roles; V1 does not depend on that deployment choice.

## Standards references

- [Bluetooth SIG Broadcast Audio Scan Service 1.0.1](https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/BASS_v1.0.1/out/en/index-en.html)
- [Bluetooth SIG Basic Audio Profile](https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/16212-BAP-html5/out/en/index-en.html)
- [Zephyr LE Audio architecture](https://docs.zephyrproject.org/latest/services/connectivity/bluetooth/api/audio/bluetooth-le-audio-arch.html)
- [Zephyr BAP Broadcast Assistant sample](https://docs.zephyrproject.org/latest/samples/bluetooth/bap_broadcast_assistant/README.html)
