/**
 * @file
 * @brief LED peripheral module.
 *
 * Implements LED control using a Zbus subscriber. A worker thread waits for
 * messages on `led_chan` and handles commands (on/off/toggle/blink). GPIOs
 * are configured during `init_led()` and the module maintains simple state
 * to support blinking.
 */

#include "led.h"
#include "macros_common.h"
#include "zbus_common.h"
#include <errno.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>
#include <zephyr/zbus/zbus.h>

LOG_MODULE_REGISTER(led, LOG_LEVEL_INF);

#define LED1_NODE DT_ALIAS(led0)
#define LED2_NODE DT_ALIAS(led1)
#define LED3_NODE DT_ALIAS(led2)

BUILD_ASSERT(DT_NODE_HAS_STATUS(LED1_NODE, okay), "led0 alias missing or disabled");
BUILD_ASSERT(DT_NODE_HAS_STATUS(LED2_NODE, okay), "led1 alias missing or disabled");
BUILD_ASSERT(DT_NODE_HAS_STATUS(LED3_NODE, okay), "led2 alias missing or disabled");

static const struct gpio_dt_spec leds[] = {
  GPIO_DT_SPEC_GET(LED1_NODE, gpios),
  GPIO_DT_SPEC_GET(LED2_NODE, gpios),
  GPIO_DT_SPEC_GET(LED3_NODE, gpios),
};

#define N_LEDS ARRAY_SIZE(leds)
#define BLINK_FREQ_MS 500
#define LED_SUB_Q_SIZE 3

static uint32_t blinking_mask;

static void blink_timer_handler(struct k_timer* timer)
{
  ARG_UNUSED(timer);

  for (int i = 0; i < N_LEDS; i++) {
    if ((blinking_mask & BIT(i)) != 0) {
      gpio_pin_toggle_dt(&leds[i]);
    }
  }
}
K_TIMER_DEFINE(blink_timer, blink_timer_handler, NULL);

ZBUS_SUBSCRIBER_DEFINE(led_sub, LED_SUB_Q_SIZE);

ZBUS_CHAN_DEFINE(led_chan, led_chan_msg_t, NULL, NULL, ZBUS_OBSERVERS(led_sub), ZBUS_MSG_INIT(0));

#define LED_THREAD_STACK_SIZE 1024
#define LED_THREAD_PRIORITY 6

static int handle_led_msg(led_chan_msg_t msg)
{
  int ret = 0;
  int led_n = msg.led;

  if (led_n < 0 || led_n >= N_LEDS) {
    return -EINVAL;
  }

  switch (msg.cmd) {
  case TURN_OFF:
    blinking_mask &= ~BIT(led_n);
    ret = gpio_pin_set_dt(&leds[led_n], GPIO_OUTPUT_INACTIVE);
    ERR_CHK(ret);
    if (blinking_mask == 0) {
      k_timer_stop(&blink_timer);
    }
    break;

  case TURN_ON:
    blinking_mask &= ~BIT(led_n);
    ret = gpio_pin_set_dt(&leds[led_n], GPIO_OUTPUT_ACTIVE);
    ERR_CHK(ret);
    if (blinking_mask == 0) {
      k_timer_stop(&blink_timer);
    }
    break;

  case BLINK:
    blinking_mask |= BIT(led_n);
    k_timer_start(&blink_timer, K_MSEC(BLINK_FREQ_MS), K_MSEC(BLINK_FREQ_MS));
    break;

  case TOGGLE:
    ret = gpio_pin_toggle_dt(&leds[led_n]);
    ERR_CHK(ret);
    break;

  default:
    ret = -EINVAL;
    break;
  }
  return ret;
}

static void led_thread(void* arg1, void* arg2, void* arg3)
{
  const struct zbus_channel* chan;
  led_chan_msg_t msg;
  int ret;

  ARG_UNUSED(arg1);
  ARG_UNUSED(arg2);
  ARG_UNUSED(arg3);

  while (1) {
    ret = zbus_sub_wait(&led_sub, &chan, K_FOREVER);
    ERR_CHK(ret);

    ret = zbus_chan_read(chan, &msg, ZBUS_READ_TIMEOUT_MS);
    ERR_CHK(ret);

    ret = handle_led_msg(msg);
    ERR_CHK(ret);
  }
}

K_THREAD_DEFINE(led_thread_id, LED_THREAD_STACK_SIZE, led_thread, NULL, NULL, NULL, LED_THREAD_PRIORITY, 0, 0);

int led_init(void)
{
  int ret = 0;

  for (int i = 0; i < N_LEDS; i++) {
    if (!gpio_is_ready_dt(&leds[i])) {
      return -ENODEV;
    }

    ret = gpio_pin_configure_dt(&leds[i], GPIO_OUTPUT_INACTIVE);
    ERR_CHK(ret);
  }

  return ret;
}
