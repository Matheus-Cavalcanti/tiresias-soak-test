/**
 * @file
 * @brief LED peripheral module.
 *
 * Implements LED control using a Zbus subscriber. A worker thread waits for
 * messages on `led_chan`. LED GPIOs are left disconnected so they do not
 * affect power measurements.
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
#define LED_SUB_Q_SIZE 3

ZBUS_SUBSCRIBER_DEFINE(led_sub, LED_SUB_Q_SIZE);

ZBUS_CHAN_DEFINE(led_chan, led_chan_msg_t, NULL, NULL, ZBUS_OBSERVERS(led_sub), ZBUS_MSG_INIT(0));

#define LED_THREAD_STACK_SIZE 1024
#define LED_THREAD_PRIORITY 6

static int led_disconnect(int led_n)
{
  return gpio_pin_configure(leds[led_n].port, leds[led_n].pin, GPIO_DISCONNECTED);
}

static int handle_led_msg(led_chan_msg_t msg)
{
  int ret = 0;
  int led_n = msg.led;

  if (led_n < 0 || led_n >= N_LEDS) {
    return -EINVAL;
  }

  switch (msg.cmd) {
  case TURN_OFF:
  case TURN_ON:
  case BLINK:
  case TOGGLE:
    ret = led_disconnect(led_n);
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

    ret = led_disconnect(i);
    ERR_CHK(ret);
  }

  return ret;
}
