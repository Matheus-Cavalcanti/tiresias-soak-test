/*
 * TIR-39 EVAL-ADAU1787Z power-state controller.
 *
 * The nRF5340 Audio DK is powered independently and connects to the EVAL
 * through GND, SDA and SCL only. J15 remains the manual hardware !PD control.
 * No SigmaStudio program is downloaded.
 */

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(main_eval_i2c, LOG_LEVEL_INF);

#define EVAL_I2C_NODE DT_NODELABEL(i2c2)
#define ADAU1787_I2C_ADDRESS 0x2BU

#define ADAU1787_REG_VENDOR_ID 0xC000U
#define ADAU1787_REG_POWER_BASE 0xC004U
#define ADAU1787_REG_CHIP_PWR 0xC00DU
#define ADAU1787_REG_STATUS2 0xC0ABU

#define ADAU1787_POWER_REGISTER_COUNT 10U
#define ADAU1787_POWER_UP_COMPLETE BIT(7)

#define REG_EN_SETTLE_MS 20
#define CM_SETTLE_MS 35
#define POWER_UP_TIMEOUT_MS 100
#define BUTTON_POLL_MS 20

static const struct device* const eval_i2c = DEVICE_DT_GET(EVAL_I2C_NODE);
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

static int adau1787_write(uint16_t reg, uint8_t value)
{
  const uint8_t tx[] = {
    (uint8_t)(reg >> 8),
    (uint8_t)(reg & 0xFFU),
    value,
  };

  return i2c_write(eval_i2c, tx, sizeof(tx), ADAU1787_I2C_ADDRESS);
}

static int adau1787_read(uint16_t reg, uint8_t* data, size_t length)
{
  const uint8_t address[] = {
    (uint8_t)(reg >> 8),
    (uint8_t)(reg & 0xFFU),
  };

  return i2c_write_read(eval_i2c, ADAU1787_I2C_ADDRESS, address, sizeof(address), data, length);
}

static int verify_identity(void)
{
  uint8_t identity[4];
  int ret = adau1787_read(ADAU1787_REG_VENDOR_ID, identity, sizeof(identity));

  if (ret != 0) {
    LOG_ERR(
        "ADAU1787 did not acknowledge at 0x%02x: %d; open EVAL J15 and check ADDR1/ADDR0", ADAU1787_I2C_ADDRESS, ret);
    return ret;
  }

  if (identity[0] != 0x41U || identity[1] != 0x17U || identity[2] != 0x87U) {
    LOG_ERR("Unexpected identity: %02x %02x %02x rev %02x", identity[0], identity[1], identity[2], identity[3]);
    return -ENODEV;
  }

  LOG_INF("ADAU1787 detected at 0x%02x; revision 0x%02x", ADAU1787_I2C_ADDRESS, identity[3]);
  return 0;
}

static int write_power_registers(const uint8_t values[ADAU1787_POWER_REGISTER_COUNT])
{
  for (uint16_t index = 0U; index < ADAU1787_POWER_REGISTER_COUNT; ++index) {
    int ret = adau1787_write(ADAU1787_REG_POWER_BASE + index, values[index]);
    if (ret != 0) {
      LOG_ERR("Write failed at 0x%04x: %d", ADAU1787_REG_POWER_BASE + index, ret);
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
    const uint16_t reg = ADAU1787_REG_POWER_BASE + index;

    LOG_INF("ADAU1787 0x%04x = 0x%02x", reg, actual[index]);
    if (actual[index] != expected[index]) {
      LOG_ERR("Readback mismatch at 0x%04x: expected 0x%02x", reg, expected[index]);
      return -EIO;
    }
  }

  return 0;
}

static int apply_software_power_down(void)
{
  k_msleep(REG_EN_SETTLE_MS);

  int ret = verify_identity();
  if (ret != 0) {
    return ret;
  }

  ret = write_power_registers(software_power_down);
  if (ret != 0) {
    return ret;
  }

  ret = verify_power_registers(software_power_down);
  if (ret == 0) {
    LOG_INF("STATE=SOFTWARE_PD: POWER_EN=0, internal DLDO selected; measure EVAL current and DVDD");
  }

  return ret;
}

static int wait_for_power_up_complete(void)
{
  for (int elapsed = 0; elapsed <= POWER_UP_TIMEOUT_MS; ++elapsed) {
    uint8_t status;
    int ret = adau1787_read(ADAU1787_REG_STATUS2, &status, sizeof(status));

    if (ret != 0) {
      return ret;
    }
    if ((status & ADAU1787_POWER_UP_COMPLETE) != 0U) {
      LOG_INF("POWER_UP_COMPLETE=1 after %d ms", elapsed);
      return 0;
    }
    k_msleep(1);
  }

  return -ETIMEDOUT;
}

static int apply_minimal_digital_on(void)
{
  k_msleep(REG_EN_SETTLE_MS);

  int ret = verify_identity();
  if (ret != 0) {
    return ret;
  }

  ret = write_power_registers(software_power_down);
  if (ret != 0) {
    return ret;
  }

  /* POWER_EN=1, DLDO=0.9 V, CM fast-charge active, all blocks still off. */
  ret = adau1787_write(ADAU1787_REG_CHIP_PWR, 0x11U);
  if (ret != 0) {
    return ret;
  }

  k_msleep(CM_SETTLE_MS);

  /* End CM fast charge; keep MASTER_BLOCK_EN=0 and every block disabled. */
  ret = adau1787_write(ADAU1787_REG_CHIP_PWR, 0x15U);
  if (ret != 0) {
    return ret;
  }

  ret = wait_for_power_up_complete();
  if (ret != 0) {
    LOG_ERR("POWER_UP_COMPLETE timeout: %d", ret);
    return ret;
  }

  uint8_t expected[ADAU1787_POWER_REGISTER_COUNT];
  memcpy(expected, software_power_down, sizeof(expected));
  expected[ADAU1787_POWER_REGISTER_COUNT - 1U] = 0x15U;

  ret = verify_power_registers(expected);
  if (ret == 0) {
    LOG_INF("STATE=DIGITAL_ON_MINIMAL: POWER_EN=1, no PLL/DSP/ADC/DAC; measure EVAL current and DVDD");
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
  if (!device_is_ready(eval_i2c)) {
    LOG_ERR("External EVAL I2C controller is not ready");
    return -ENODEV;
  }

  int ret = configure_button(&button_software_pd);
  if (ret != 0) {
    LOG_ERR("Software-PD button configuration failed: %d", ret);
    return ret;
  }

  ret = configure_button(&button_digital_on);
  if (ret != 0) {
    LOG_ERR("Digital-on button configuration failed: %d", ret);
    return ret;
  }

  LOG_INF("TIR-39 controller ready; Audio DK D9=SDA, D10=SCL, common GND only");
  LOG_INF("Measure HARDWARE_PD with EVAL J15 closed and no button pressed");
  LOG_INF("Open J15; press VOL- for SOFTWARE_PD or VOL+ for DIGITAL_ON_MINIMAL");

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
