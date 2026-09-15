/*
 * ADAU1787 power-down diagnostic profile.
 *
 * All board supply rails remain present. The nRF5340 only asserts the
 * ADAU1787 active-low !PD input, initializes and suspends the same I2C bus
 * used by the other measurement profiles, and then falls through to Zephyr's
 * idle thread. No ADAU1787 control-port transfer or SigmaStudio download
 * occurs.
 */

#include "soak_i2c_sleep.h"

#include <errno.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main_adau_pd, LOG_LEVEL_INF);

#define ADAU1787_NODE DT_NODELABEL(adau_1787)

static const struct gpio_dt_spec codec_powerdown = GPIO_DT_SPEC_GET(ADAU1787_NODE, powerdown_gpios);

int main(void)
{
  if (!gpio_is_ready_dt(&codec_powerdown)) {
    LOG_ERR("ADAU1787 power-down GPIO controller is not ready");
    return -ENODEV;
  }

  /* GPIO_ACTIVE_LOW maps logical active to a physical low on ADAU1787 !PD. */
  int ret = gpio_pin_configure_dt(&codec_powerdown, GPIO_OUTPUT_ACTIVE);
  if (ret != 0) {
    LOG_ERR("Failed to assert ADAU1787 !PD: %d", ret);
    return ret;
  }

  const struct device* i2c_bus = soak_adau1787_i2c_bus();
  if (!device_is_ready(i2c_bus)) {
    LOG_ERR("ADAU1787 I2C controller is not ready");
    return -ENODEV;
  }

  ret = i2c_configure(i2c_bus, I2C_SPEED_SET(I2C_SPEED_STANDARD));
  if (ret != 0) {
    LOG_ERR("Failed to configure ADAU1787 I2C: %d", ret);
    return ret;
  }

  ret = soak_suspend_adau1787_i2c();
  if (ret != 0) {
    LOG_ERR("Failed to suspend ADAU1787 I2C: %d", ret);
    return ret;
  }

  LOG_INF("ADAU1787 !PD asserted; I2C suspended; no control-port transfer or SigmaStudio download; entering idle");

  /* Returning terminates the main thread; Zephyr's idle thread takes over. */
  return 0;
}
