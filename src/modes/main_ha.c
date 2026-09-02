/*
 * Minimal firmware image for the local hearing-aid soak test.
 *
 * The nRF5340 only releases and programs the ADAU1787. Audio processing then
 * remains entirely inside the codec/DSP, clocked by the external oscillator.
 */

#include "adau1787.h"

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main_ha, LOG_LEVEL_INF);

int main(void)
{
  int ret = adau1787_init();

  if (ret != 0) {
    LOG_ERR("ADAU1787 initialization failed: %d", ret);
    return ret;
  }

  LOG_INF("HA soak profile ready; entering idle with external ADAU1787 MCLK");

  /* Returning terminates the main thread; Zephyr's idle thread takes over. */
  return 0;
}
