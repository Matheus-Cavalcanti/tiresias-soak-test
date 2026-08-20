# Bluetooth Management

## Purpose

Bluetooth Management is the shared mechanism layer between Zephyr's Bluetooth host and
the Tiresias control-plane subsystems. It performs global host initialization, controller
configuration, callback registration, scanning and advertising procedures, and publication
of physical Bluetooth lifecycle events.

It is not a stateful product subsystem. It does not decide whether remote control or
broadcast reception should be available, and it does not own Control Link or Audio
Streaming state. Those policies remain in their respective subsystems.

The distinction is intentional:

```text
Device Controller policy
    ├── Control Link policy ───────┐
    └── Audio Streaming policy ────┤
                                   v
                         Bluetooth Management
                     shared stack and procedures
                                   v
                       Zephyr Bluetooth host
```

Control Link and Audio Streaming are independent users of one Bluetooth host. Allowing
both to call Bluetooth Management avoids making either subsystem a proxy for the other.
The shared module centralizes global mechanisms and callback fan-out so that independent
access does not become duplicate initialization or shared-state mutation.

## Initialization contract

`bt_mgmt_init()` initializes the following global facilities:

- Zephyr's Bluetooth host through `bt_enable()`;
- persisted Bluetooth settings;
- optional test address and bond handling;
- Bluetooth controller configuration and watchdog support;
- the shared connection callback set; and
- the advertising worker used by `bt_mgmt_adv_start()`.

The function is safe for Control Link and Audio Streaming to call independently:

1. A mutex admits only one initialization caller.
2. The first caller performs the complete initialization attempt while later callers wait.
3. The result of that first attempt is stored.
4. Every later caller receives the stored result without repeating side effects.

The first failure is cached for the rest of the boot. Retrying after an unknown partial
failure could register callbacks twice, reload global settings unexpectedly, or configure
the controller twice. Recovery currently requires rebooting the device. `-EALREADY` from
`bt_enable()` is treated as an already-enabled host, after which the remaining
Tiresias-owned setup is still performed.

The `bt_enable()` completion callback records the asynchronous result and releases the
waiting initializer. It does not execute the fatal error policy inside Bluetooth callback
context; the caller receives the error and its owning subsystem decides how to report it.

Correctness does not depend on startup order. Device Controller sends the Control Link and
Audio Streaming startup commands, but either subsystem may reach `bt_mgmt_init()` first.
The mutex and cached result make both schedules equivalent.

## Connection event contract

Bluetooth Management registers the global Zephyr connection callbacks and is the sole
publisher of ACL lifecycle events on `bt_mgmt_chan`. ACL event messages are initialized
before publication and include filtering metadata:

| Field | Meaning |
|---|---|
| `event` | Physical Bluetooth event type. |
| `peripheral` | Whether Tiresias has the peripheral role for this ACL. |
| `index` | Zephyr connection index used for lifecycle correlation. |
| `conn` | Borrowed Bluetooth connection pointer for immediate handling. |

Control Link accepts only a peripheral connection while its connectable advertising is
active or pending. It stores the scalar connection index, not the borrowed pointer, and
restarts advertising only when a disconnection carries the same index. An unrelated
central-role connection cannot change Control Link state.

A consumer that must retain `conn` beyond handling the copied event must call
`bt_conn_ref()` and later `bt_conn_unref()`. Copying the pointer in a Zbus message does not
transfer a Bluetooth reference.

Bluetooth Management previously restarted connectable advertising after every peripheral
disconnection. That behavior was removed from the generic callback because it embedded
Control Link policy in the shared mechanism layer. The callback now publishes the event;
Control Link decides whether the disconnected ACL belonged to it and requests the restart.
Generic recovery for failed connection establishment and directed-advertising timeout
remains in Bluetooth Management because those failures belong to the physical procedure.

## Advertising module

`bt_mgmt_adv_start()` is asynchronous. It retains the supplied advertising-data pointers,
queues the set index, and schedules the advertising worker. Returning zero means the
request was admitted; it does not mean the controller has started advertising.

The advertising worker now publishes an explicit completion event:

| Event | Meaning |
|---|---|
| `BT_MGMT_EXT_ADV_STARTED` | The indexed extended advertising set started successfully. |
| `BT_MGMT_EXT_ADV_FAILED` | Creation, configuration, or start failed; `error` contains the cause. |
| `BT_MGMT_EXT_ADV_WITH_PA_READY` | Existing event indicating that periodic advertising is also ready. |

All events carry the advertising-set index. Control Link waits for the result for its own
index before publishing `ADVERTISING`. It can therefore distinguish a queued request from
an observable radio state and show the correct LED indication.

The current build has one advertising set and assigns index 0 to Control Link. Its
advertising data is `static const`, so it remains valid for the advertising module's
retained-pointer lifetime and can be reused after disconnection. Audio Streaming does not
mutate that set; it uses observer scanning and periodic synchronization instead.

## Event delivery and concurrency

`bt_mgmt_chan` is an internal physical-event fan-out, not a subsystem state channel.
Control Link and Audio Streaming use separate Zbus message subscribers. Each publication
is copied into each subscriber's ordered FIFO, so one consumer cannot remove an event from
the other and neither rereads a latest value that may already have been overwritten.

The consumers divide the event space:

| Consumer | Events and procedures it owns |
|---|---|
| Control Link | Indexed advertising completion/failure and its peripheral connection/disconnection lifecycle; security is reserved for the later authorized session. |
| Audio Streaming | Broadcast scan, PA synchronization/loss, broadcast code, and LE Audio lifecycle. |

Only fields defined for the selected event type are valid. Some existing PA and BASS
publishers assign only their event-specific fields, so consumers must switch on `event`
before reading any union-like payload field and must not infer meaning from unused bytes.

The following invariants prevent resource conflicts in the current configuration:

- only Bluetooth Management calls `bt_enable()` and registers global callbacks;
- both subsystems enter that setup through the same mutex-protected initializer;
- Control Link is the only subsystem policy client for advertising set 0; Bluetooth
  Management may continue or recover that same physical procedure after a connection
  establishment failure or directed-advertising timeout;
- only Audio Streaming requests broadcast scanning and PA/BIG/BIS lifecycle work;
- Bluetooth Management is the only `bt_mgmt_chan` publisher;
- both consumers receive ordered copies and filter by event type, role, and resource index;
- subsystem private state is changed only by its owning subsystem thread; and
- Control Link admits stop or restart only from lifecycle conditions that exclude an
  overlapping advertising start.

The Bluetooth callbacks and advertising worker publish without blocking. The Zbus message
buffer pool is bounded, so publication can fail under exhaustion. New advertising-result
paths log that failure; several inherited callback paths use `ERR_CHK`, which escalates it
to a kernel oops. The one-peer, one-advertising-set, low-rate lifecycle is expected to
remain within the configured bound, but stress testing is required. A unified nonfatal
overflow policy, advertising completion timeouts, and recovery from a lost completion
event remain future reliability work.

If another feature needs its own advertising set or must modify the Control Link set, it
must introduce an explicit advertising resource allocator/composer and update controller
limits. It must not become a second uncoordinated caller for set index 0.

## Current subsystem flow

At startup:

1. Device Controller sends `ENABLE_CONTROL` to Control Link.
2. Device Controller independently starts Audio Streaming.
3. The first subsystem reaching `bt_mgmt_init()` initializes the shared host; the other
   receives the cached result.
4. Control Link submits connectable advertising for set 0.
5. The advertising worker publishes the indexed outcome.
6. Control Link enters `ADVERTISING` only on confirmed success.

At connection teardown:

1. Bluetooth Management publishes the indexed `BT_MGMT_DISCONNECTED` event.
2. Both message subscribers receive a copy.
3. Audio Streaming ignores it because it is not a broadcast lifecycle event.
4. Control Link compares the connection index with its retained peer.
5. A match causes Control Link to request advertising restart.
6. Control Link returns to `ADVERTISING` only after the new started event is received.

The complete subsystem rationale and future custom-service design are described in
[Control Link and Remote Management Architecture](../architecture/control-link.md).
