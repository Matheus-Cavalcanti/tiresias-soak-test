/*
 * Minimal firmware image for the local hearing-aid soak test.
 *
 * The nRF5340 only releases and programs the ADAU1787. Audio processing then
 * remains entirely inside the codec/DSP, clocked by the external oscillator.
 */

#include "adau1787.h"

#include <zephyr/autoconf.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main_ha, LOG_LEVEL_INF);

int main(void)
{
  int ret = adau1787_init();

  if (ret != 0) {
    LOG_ERR("ADAU1787 initialization failed: %d", ret);
    return ret;
  }

  ret = adau1787_apply_ha_power_trim(CONFIG_TIRESIAS_HA_INPUT_ADC, CONFIG_TIRESIAS_HA_OUTPUT_DAC);
  if (ret != 0) {
    LOG_ERR("ADAU1787 HA power trim failed: %d", ret);
    return ret;
  }

  LOG_INF("HA soak profile ready (ADC%d -> DAC%d); entering idle with external ADAU1787 MCLK",
      CONFIG_TIRESIAS_HA_INPUT_ADC, CONFIG_TIRESIAS_HA_OUTPUT_DAC);

  /* Returning terminates the main thread; Zephyr's idle thread takes over. */
  return 0;
}
