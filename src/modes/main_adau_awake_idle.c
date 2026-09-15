/*
 * ADAU1787 software full-chip power-down diagnostic profile.
 *
 * Supply rails remain present and !PD is released. The control port remains
 * available while POWER_EN, MASTER_BLOCK_EN and every block-level power
 * enable stay off. The control I2C bus is suspended after register readback.
 * No SigmaStudio download occurs.
 */

#include "soak_i2c_sleep.h"

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(main_adau_awake_idle, LOG_LEVEL_INF);

#define ADAU1787_NODE DT_NODELABEL(adau_1787)

#define ADAU1787_REG_ADC_DAC_HP_PWR 0xC004U
#define ADAU1787_REG_PLL_MB_PGA_PWR 0xC005U
#define ADAU1787_REG_DMIC_PWR 0xC006U
#define ADAU1787_REG_SAI_CLK_PWR 0xC007U
#define ADAU1787_REG_DSP_PWR 0xC008U
#define ADAU1787_REG_ASRC_PWR 0xC009U
#define ADAU1787_REG_FINT_PWR 0xC00AU
#define ADAU1787_REG_FDEC_PWR 0xC00BU
#define ADAU1787_REG_KEEPS 0xC00CU
#define ADAU1787_REG_CHIP_PWR 0xC00DU

/* DLDO_CTRL = 01; CM_STARTUP_OVER = 1; MASTER_BLOCK_EN = POWER_EN = 0. */
#define ADAU1787_CHIP_PWR_AWAKE_IDLE 0x14U

static const struct gpio_dt_spec codec_powerdown = GPIO_DT_SPEC_GET(ADAU1787_NODE, powerdown_gpios);
static const struct i2c_dt_spec codec_i2c = I2C_DT_SPEC_GET(ADAU1787_NODE);

static int write_and_verify_register(uint16_t address, uint8_t value, const char* name)
{
  const uint8_t address_bytes[] = { (uint8_t)(address >> 8), (uint8_t)address };
  const uint8_t write_buffer[] = { address_bytes[0], address_bytes[1], value };
  uint8_t readback = 0;

  int ret = i2c_write_dt(&codec_i2c, write_buffer, sizeof(write_buffer));
  if (ret != 0) {
    LOG_ERR("Failed to write ADAU1787 %s (0x%04x): %d", name, address, ret);
    return ret;
  }

  ret = i2c_write_read_dt(&codec_i2c, address_bytes, sizeof(address_bytes), &readback, sizeof(readback));
  if (ret != 0) {
    LOG_ERR("Failed to read ADAU1787 %s (0x%04x): %d", name, address, ret);
    return ret;
  }

  if (readback != value) {
    LOG_ERR("ADAU1787 %s mismatch: wrote 0x%02x, read 0x%02x", name, value, readback);
    return -EIO;
  }

  LOG_INF("ADAU1787 %-17s = 0x%02x", name, readback);
  return 0;
}

int main(void)
{
  if (!gpio_is_ready_dt(&codec_powerdown)) {
    LOG_ERR("ADAU1787 power-down GPIO controller is not ready");
    return -ENODEV;
  }

  if (!device_is_ready(codec_i2c.bus)) {
    LOG_ERR("ADAU1787 I2C controller is not ready");
    return -ENODEV;
  }

  int ret = gpio_pin_configure_dt(&codec_powerdown, GPIO_OUTPUT_ACTIVE);
  if (ret != 0) {
    LOG_ERR("Failed to assert ADAU1787 !PD: %d", ret);
    return ret;
  }

  ret = i2c_configure(codec_i2c.bus, I2C_SPEED_SET(I2C_SPEED_STANDARD));
  if (ret != 0) {
    LOG_ERR("Failed to configure ADAU1787 I2C: %d", ret);
    return ret;
  }

  /* Release !PD only after the nRF output is known to have started low. */
  ret = gpio_pin_set_dt(&codec_powerdown, 0);
  if (ret != 0) {
    LOG_ERR("Failed to release ADAU1787 !PD: %d", ret);
    return ret;
  }

  /* REG_EN is tied to AVDD on Tiresias. Allow DVDD and CM to settle. */
  k_msleep(100);

  const struct {
    uint16_t address;
    uint8_t value;
    const char* name;
  } writes[] = {
    { ADAU1787_REG_CHIP_PWR, 0x10U, "CHIP_PWR" },
    { ADAU1787_REG_ADC_DAC_HP_PWR, 0x00U, "ADC_DAC_HP_PWR" },
    { ADAU1787_REG_PLL_MB_PGA_PWR, 0x00U, "PLL_MB_PGA_PWR" },
    { ADAU1787_REG_DMIC_PWR, 0x00U, "DMIC_PWR" },
    { ADAU1787_REG_SAI_CLK_PWR, 0x00U, "SAI_CLK_PWR" },
    { ADAU1787_REG_DSP_PWR, 0x00U, "DSP_PWR" },
    { ADAU1787_REG_ASRC_PWR, 0x00U, "ASRC_PWR" },
    { ADAU1787_REG_FINT_PWR, 0x00U, "FINT_PWR" },
    { ADAU1787_REG_FDEC_PWR, 0x00U, "FDEC_PWR" },
    { ADAU1787_REG_KEEPS, 0x00U, "KEEPS" },
    { ADAU1787_REG_CHIP_PWR, ADAU1787_CHIP_PWR_AWAKE_IDLE, "CHIP_PWR" },
  };

  LOG_INF("Applying ADAU1787 awake-idle power state");
  for (size_t i = 0; i < ARRAY_SIZE(writes); i++) {
    ret = write_and_verify_register(writes[i].address, writes[i].value, writes[i].name);
    if (ret != 0) {
      return ret;
    }
  }

  ret = soak_suspend_adau1787_i2c();
  if (ret != 0) {
    LOG_ERR("Failed to suspend ADAU1787 I2C: %d", ret);
    return ret;
  }

  LOG_INF("ADAU1787 awake-idle ready; POWER_EN=0; I2C suspended; no SigmaStudio download; entering idle");
  return 0;
}
