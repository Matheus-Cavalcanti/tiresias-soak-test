#ifndef SOAK_I2C_SLEEP_H_
#define SOAK_I2C_SLEEP_H_

#include <errno.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/pm/device.h>

#define SOAK_ADAU1787_NODE DT_NODELABEL(adau_1787)

static inline const struct device* soak_adau1787_i2c_bus(void)
{
  return DEVICE_DT_GET(DT_BUS(SOAK_ADAU1787_NODE));
}

static inline int soak_suspend_adau1787_i2c(void)
{
  const struct device* bus = soak_adau1787_i2c_bus();

  if (!device_is_ready(bus)) {
    return -ENODEV;
  }

  /* The nRF TWIM suspend action disables the peripheral and applies i2c1_sleep. */
  int ret = pm_device_action_run(bus, PM_DEVICE_ACTION_SUSPEND);
  if (ret != 0 && ret != -EALREADY) {
    return ret;
  }

  enum pm_device_state state;
  ret = pm_device_state_get(bus, &state);
  if (ret != 0) {
    return ret;
  }

  return state == PM_DEVICE_STATE_SUSPENDED ? 0 : -EIO;
}

#endif /* SOAK_I2C_SLEEP_H_ */
