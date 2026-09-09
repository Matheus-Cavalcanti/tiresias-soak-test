/*
 * TIR-39 EVAL-ADAU1787Z power-state controller.
 *
 * The independently powered nRF5340 Audio DK controls the EVAL through GND,
 * SDA, SCL and !PD only. The production ADAU1787 driver owns the control port
 * and performs a deterministic hardware reset before register access. No
 * SigmaStudio program is downloaded by this diagnostic profile.
 */

#include "adau1787.h"

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(main_eval_i2c, LOG_LEVEL_INF);

#define ADAU1787_REG_VENDOR_ID 0xC000U
#define ADAU1787_REG_POWER_BASE 0xC004U
#define ADAU1787_REG_CHIP_PWR 0xC00DU
#define ADAU1787_REG_STATUS2 0xC0ABU

#define ADAU1787_POWER_REGISTER_COUNT 10U
#define ADAU1787_POWER_UP_COMPLETE BIT(7)

#define HARDWARE_PD_MS 10U
#define CM_SETTLE_MS 35U
#define POWER_UP_TIMEOUT_MS 100U
#define BUTTON_POLL_MS 20U

static const struct gpio_dt_spec button_software_pd = GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios);
static const struct gpio_dt_spec button_digital_on = GPIO_DT_SPEC_GET(DT_ALIAS(sw1), gpios);

static const uint8_t software_power_down[ADAU1787_POWER_REGISTER_COUNT] = {
  0x00, /* C004 ADC_DAC_HP_PWR */
  0x00, /* C005 PLL_MB_PGA_PWR: PLL and crystal disabled */
  0x00, /* C006 DMIC_PWR */
  0x00, /* C007 SAI_CLK_PWR */
  0x00, /* C008 DSP_PWR */
  0x00, /* C009 ASRC_PWR */
  0x00, /* C00A FINT_PWR */
  0x00, /* C00B FDEC_PWR */
  0x00, /* C00C KEEPS: CM and DSP memories not retained */
  0x14, /* C00D: DLDO=0.9 V, CM boost off, blocks off, POWER_EN=0 */
};

static int verify_identity(void)
{
  uint8_t identity[4];
  int ret = adau1787_read(ADAU1787_REG_VENDOR_ID, identity, sizeof(identity));

  if (ret != 0) {
    LOG_ERR("ADAU1787 identity read failed at 0x2b: %d", ret);
    return ret;
  }

  if (identity[0] != 0x41U || identity[1] != 0x17U || identity[2] != 0x87U) {
    LOG_ERR("Unexpected identity %02x %02x %02x rev %02x", identity[0], identity[1], identity[2], identity[3]);
    return -ENODEV;
  }

  LOG_INF("ADAU1787 detected at 0x2b; identity 41 17 87, revision 0x%02x", identity[3]);
  return 0;
}

static int write_register(uint16_t address, uint8_t value)
{
  return adau1787_write_register(address, &value);
}

static int write_power_registers(const uint8_t values[ADAU1787_POWER_REGISTER_COUNT])
{
  for (uint16_t index = 0U; index < ADAU1787_POWER_REGISTER_COUNT; ++index) {
    const uint16_t address = ADAU1787_REG_POWER_BASE + index;
    int ret = write_register(address, values[index]);

    if (ret != 0) {
      LOG_ERR("Write failed at 0x%04x: %d", address, ret);
      return ret;
    }
  }

  return 0;
}

static int verify_power_registers(const uint8_t expected[ADAU1787_POWER_REGISTER_COUNT])
{
  uint8_t actual[ADAU1787_POWER_REGISTER_COUNT];
  int ret = adau1787_read(ADAU1787_REG_POWER_BASE, actual, sizeof(actual));

  if (ret != 0) {
    LOG_ERR("Power-register readback failed: %d", ret);
    return ret;
  }

  for (uint16_t index = 0U; index < ADAU1787_POWER_REGISTER_COUNT; ++index) {
    const uint16_t address = ADAU1787_REG_POWER_BASE + index;

    LOG_INF("ADAU1787 0x%04x = 0x%02x", address, actual[index]);
    if (actual[index] != expected[index]) {
      LOG_ERR("Readback mismatch at 0x%04x: expected 0x%02x", address, expected[index]);
      return -EIO;
    }
  }

  return 0;
}

static int wait_for_power_up_complete(void)
{
  for (uint32_t elapsed = 0U; elapsed <= POWER_UP_TIMEOUT_MS; ++elapsed) {
    uint8_t status;
    int ret = adau1787_read(ADAU1787_REG_STATUS2, &status, sizeof(status));

    if (ret != 0) {
      return ret;
    }
    if ((status & ADAU1787_POWER_UP_COMPLETE) != 0U) {
      LOG_INF("POWER_UP_COMPLETE=1 after %u ms", elapsed);
      return 0;
    }
    k_msleep(1);
  }

  return -ETIMEDOUT;
}

static int reset_and_start_control_port(void)
{
  int ret = adau1787_power_down();
  if (ret != 0) {
    return ret;
  }

  k_msleep(HARDWARE_PD_MS);

  ret = adau1787_release_control_port();
  if (ret != 0) {
    return ret;
  }

  /*
   * Deliberately make the first bus transaction a simple write. This mirrors
   * the SigmaStudio/Tiresias path and separates write ACK from the following
   * repeated-start register read.
   */
  ret = write_register(ADAU1787_REG_CHIP_PWR, 0x11U);
  if (ret != 0) {
    LOG_ERR("Write-first CHIP_PWR probe failed at 0x2b: %d", ret);
    return ret;
  }
  LOG_INF("Write-first CHIP_PWR=0x11 acknowledged at 0x2b");

  for (uint16_t index = 0U; index < ADAU1787_POWER_REGISTER_COUNT - 1U; ++index) {
    ret = write_register(ADAU1787_REG_POWER_BASE + index, 0x00U);
    if (ret != 0) {
      LOG_ERR("Failed to disable block at 0x%04x: %d", ADAU1787_REG_POWER_BASE + index, ret);
      return ret;
    }
  }

  k_msleep(CM_SETTLE_MS);

  ret = write_register(ADAU1787_REG_CHIP_PWR, 0x15U);
  if (ret != 0) {
    return ret;
  }

  ret = verify_identity();
  if (ret != 0) {
    return ret;
  }

  ret = wait_for_power_up_complete();
  if (ret != 0) {
    LOG_ERR("POWER_UP_COMPLETE timeout: %d", ret);
  }

  return ret;
}

static int apply_software_power_down(void)
{
  int ret = reset_and_start_control_port();
  if (ret != 0) {
    return ret;
  }

  ret = write_power_registers(software_power_down);
  if (ret != 0) {
    return ret;
  }

  ret = verify_power_registers(software_power_down);
  if (ret == 0) {
    LOG_INF("STATE=SOFTWARE_PD: !PD=1, POWER_EN=0; measure EVAL current and DVDD");
  }

  return ret;
}

static int apply_minimal_digital_on(void)
{
  int ret = reset_and_start_control_port();
  if (ret != 0) {
    return ret;
  }

  uint8_t expected[ADAU1787_POWER_REGISTER_COUNT];
  memcpy(expected, software_power_down, sizeof(expected));
  expected[ADAU1787_POWER_REGISTER_COUNT - 1U] = 0x15U;

  ret = verify_power_registers(expected);
  if (ret == 0) {
    LOG_INF("STATE=DIGITAL_ON_MINIMAL: !PD=1, POWER_EN=1, no PLL/DSP/ADC/DAC; measure EVAL current and DVDD");
  }

  return ret;
}

static int configure_button(const struct gpio_dt_spec* button)
{
  if (!gpio_is_ready_dt(button)) {
    return -ENODEV;
  }

  return gpio_pin_configure_dt(button, GPIO_INPUT);
}

int main(void)
{
  int ret = adau1787_prepare_control_port();
  if (ret != 0) {
    LOG_ERR("Failed to prepare ADAU1787 control port: %d", ret);
    return ret;
  }

  ret = configure_button(&button_software_pd);
  if (ret != 0) {
    LOG_ERR("Software-PD button configuration failed: %d", ret);
    return ret;
  }

  ret = configure_button(&button_digital_on);
  if (ret != 0) {
    LOG_ERR("Digital-on button configuration failed: %d", ret);
    return ret;
  }

  LOG_INF("TIR-39 driver-backed controller ready at 0x2b");
  LOG_INF("Audio DK D9=SDA, D10=SCL, D5=!PD; common GND; EVAL J15 must remain open");
  LOG_INF("STATE=HARDWARE_PD: !PD=0; measure EVAL current and DVDD now");
  LOG_INF("Press VOL- for SOFTWARE_PD or VOL+ for DIGITAL_ON_MINIMAL");

  bool previous_software_pd = false;
  bool previous_digital_on = false;

  while (true) {
    const int software_pd = gpio_pin_get_dt(&button_software_pd);
    const int digital_on = gpio_pin_get_dt(&button_digital_on);

    if (software_pd < 0 || digital_on < 0) {
      LOG_ERR("Button read failed: software_pd=%d digital_on=%d", software_pd, digital_on);
      return -EIO;
    }

    if (software_pd != 0 && !previous_software_pd) {
      ret = apply_software_power_down();
      if (ret != 0) {
        LOG_ERR("Failed to enter SOFTWARE_PD: %d", ret);
      }
    }

    if (digital_on != 0 && !previous_digital_on) {
      ret = apply_minimal_digital_on();
      if (ret != 0) {
        LOG_ERR("Failed to enter DIGITAL_ON_MINIMAL: %d", ret);
      }
    }

    previous_software_pd = software_pd != 0;
    previous_digital_on = digital_on != 0;
    k_msleep(BUTTON_POLL_MS);
  }
}
