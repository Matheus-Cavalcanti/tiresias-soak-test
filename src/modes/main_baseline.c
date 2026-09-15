/*
 * VDD-domain baseline for the PMIC-less board.
 *
 * The application deliberately does not configure the ADAU1787 control bus or
 * release its !PD pin. After this one log line, only the Zephyr idle thread is
 * left runnable.
 */

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main_baseline, LOG_LEVEL_INF);

int main(void)
{
  LOG_INF("VDD baseline profile ready; ADAU1787 untouched; entering idle");
  return 0;
}
