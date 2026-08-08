/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "fw_info_app.h"
#include "macros_common.h"
#include "tiresias_dk.h"

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main, CONFIG_MAIN_LOG_LEVEL);

int main(void)
{
  int ret;

  LOG_DBG("Main started");

  ret = tiresias_dk_init();
  ERR_CHK(ret);

  ret = fw_info_app_print();
  ERR_CHK(ret);

  return 0;
}
