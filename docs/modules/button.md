# Button Module

The button module owns the board button GPIO setup and publishes button events
to zbus. It is intentionally small: the current board has one physical button,
so the module currently exposes one event, `BUTTON_1_PRESSED`.

## Files

- `src/modules/button_handler.c`: GPIO interrupt handling, debounce, and zbus publishing.
- `src/modules/button_handler.h`: public initialization and state-read API.
- `include/zbus_common.h`: button event and zbus message types.

## Devicetree Requirements

The module requires the `sw0` devicetree alias to have a `gpios` property.

At compile time, `button_handler.c` checks this with:

```c
BUILD_ASSERT(DT_NODE_HAS_PROP(DT_ALIAS(sw0), gpios), "sw0 alias missing gpios property");
```

## Public API

Call `button_handler_init()` during board/application initialization.

```c
int button_handler_init(void);
```

The init function configures each button GPIO as input, installs the GPIO
callback, and enables the interrupt.

The module also keeps a small polling helper:

```c
int button_pressed(gpio_pin_t button_pin, bool *button_pressed);
```

Use this only when code needs the current physical GPIO state. Normal button
handling should use zbus events.

## Zbus Contract

The module defines and publishes `button_chan`:

```c
ZBUS_CHAN_DEFINE(button_chan, btn_chan_msg_t, ...);
```

The message type is:

```c
typedef enum btn_event_t {
	BUTTON_1_PRESSED,
} btn_event_t;

typedef struct btn_chan_msg_t {
	enum btn_event_t event;
} btn_chan_msg_t;
```

Consumers subscribe to `button_chan` and switch on `msg.event`.

## Runtime Flow

1. The GPIO interrupt fires for the configured button.
2. The ISR ignores the interrupt if debounce is active.
3. The ISR maps the GPIO port/pin mask to a configured button event.
4. The ISR queues a `btn_chan_msg_t` into `button_queue`.
5. `button_publish_thread` reads from the queue and publishes on `button_chan`.
6. A debounce timer re-enables event capture after `CONFIG_BUTTON_DEBOUNCE_MS`.

## Extending For More Buttons

Add new events to `btn_event_t`, then add entries to the `buttons[]` table in
`button_handler.c`.

Example shape:

```c
{
	.name = "sw1",
	.spec = GPIO_DT_SPEC_GET(DT_ALIAS(sw1), gpios),
	.event = BUTTON_2_PRESSED,
},
```

Keep the ISR generic. New behavior should usually be implemented by zbus
consumers, not inside the button module.
