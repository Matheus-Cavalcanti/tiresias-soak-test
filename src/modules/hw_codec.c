/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "hw_codec.h"

#include "adau1787.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(hw_codec, CONFIG_MODULE_HW_CODEC_LOG_LEVEL);

int hw_codec_volume_set(uint8_t set_val)
{
  (void)set_val;

  return 0;
}

int hw_codec_volume_adjust(int8_t adjustment)
{
  (void)adjustment;

  return 0;
}

int hw_codec_volume_decrease(void)
{
  return 0;
}

int hw_codec_volume_increase(void)
{
  return 0;
}

int hw_codec_volume_mute(void)
{
  return 0;
}

int hw_codec_volume_unmute(void)
{
  return 0;
}

int hw_codec_default_conf_enable(void)
{
  return 0;
}

int hw_codec_soft_reset(void)
{
  return 0;
}

int hw_codec_init(void)
{
  return adau1787_init();
}

void hw_codec_log_status_2(void)
{
  adau1787_log_status_2();
}
