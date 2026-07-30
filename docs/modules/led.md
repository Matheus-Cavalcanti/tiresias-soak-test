# LED Module

The LED module owns LED GPIO setup and receives LED commands through zbus. It
keeps LED handling simple: commands are published to `led_chan`, and a local
worker thread applies them to the configured GPIOs.

## Files

- `src/modules/led.c`: GPIO configuration, zbus subscriber, LED command handling, and blink timer.
- `src/modules/led.h`: public initialization API.
- `include/zbus_common.h`: LED command and zbus message types.

## Devicetree Requirements

The module currently expects three LED aliases:

- `led0`
- `led1`
- `led2`

Each alias must be enabled and provide a `gpios` property. The module checks
this at compile time with `BUILD_ASSERT()` before creating the static LED table.

## Public API

Call `led_init()` during board/application initialization.

```c
int led_init(void);
```

The init function verifies that each GPIO controller is ready and configures all
LED pins as inactive outputs.

## Zbus Contract

The module defines and subscribes to `led_chan`:

```c
ZBUS_CHAN_DEFINE(led_chan, led_chan_msg_t, ...);
```

The message type is:

```c
typedef enum board_led_t {
	LED_1,
	LED_2,
	LED_3,
} board_led_t;

typedef enum led_cmd_t {
	TURN_ON,
	TURN_OFF,
	TOGGLE,
	BLINK,
} led_cmd_t;

typedef struct led_chan_msg_t {
	enum board_led_t led;
	enum led_cmd_t cmd;
} led_chan_msg_t;
```

Consumers publish a command to `led_chan`; the LED thread receives it and
updates the selected LED.

## Commands

- `TURN_ON`: sets the selected LED active and removes it from the blink mask.
- `TURN_OFF`: sets the selected LED inactive and removes it from the blink mask.
- `TOGGLE`: toggles the selected LED once.
- `BLINK`: adds the selected LED to the blink mask and starts the blink timer.

The blink timer toggles every `BLINK_FREQ_MS`. The timer stops automatically
when no LEDs remain in the blink mask.

## Runtime Flow

1. `led_init()` configures the LED GPIOs.
2. A zbus subscriber thread waits for `led_chan` messages.
3. `handle_led_msg()` validates the LED index and command.
4. GPIO state is updated immediately, or the LED is added to the blink mask.
5. The timer toggles all LEDs currently present in the blink mask.

## Extending For More LEDs

Add another devicetree alias and extend the static LED table:

```c
GPIO_DT_SPEC_GET(DT_ALIAS(led3), gpios),
```

Then add the matching value to `board_led_t`. Keep `N_LEDS` derived from
`ARRAY_SIZE(leds)` so bounds checking follows the table automatically.
