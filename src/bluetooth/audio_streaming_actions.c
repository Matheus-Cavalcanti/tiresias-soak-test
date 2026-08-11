/*
 * Copyright (c) 2026 Tiresias Firmware Team
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "audio_streaming_actions.h"

#include "audio_datapath.h"
#include "audio_system.h"
#include "broadcast_sink.h"
#include "bt_mgmt.h"
#include "le_audio_rx.h"
#include "streamctrl.h"

#include <errno.h>
#include <zephyr/bluetooth/bluetooth.h>

static int scan_start(const char* broadcast_name)
{
  int ret = bt_mgmt_scan_start(0, 0, BT_MGMT_SCAN_TYPE_BROADCAST, broadcast_name, BRDCAST_ID_NOT_USED);

  return ret == -EALREADY ? 0 : ret;
}

int audio_streaming_actions_start(void)
{
  int ret;

  ret = bt_mgmt_init();
  if (ret != 0) {
    return ret;
  }

  ret = le_audio_rx_init();
  if (ret != 0) {
    return ret;
  }

  ret = broadcast_sink_enable(le_audio_rx_data_handler);
  if (ret != 0) {
    return ret;
  }

  ret = audio_system_init();
  if (ret != 0) {
    return ret;
  }

  return audio_streaming_actions_start_scan();
}

int audio_streaming_actions_start_scan(void)
{
  return scan_start(CONFIG_BT_AUDIO_BROADCAST_NAME);
}

int audio_streaming_actions_set_pa_sync(struct bt_le_per_adv_sync* pa_sync, uint32_t broadcast_id)
{
  return broadcast_sink_pa_sync_set(pa_sync, broadcast_id);
}

int audio_streaming_actions_configure_pipeline(void)
{
  uint32_t sampling_rate_hz;
  uint32_t presentation_delay_us;
  int ret;

  ret = broadcast_sink_config_get(NULL, &sampling_rate_hz, &presentation_delay_us);
  if (ret != 0) {
    return ret;
  }

  ret = audio_system_config_set(VALUE_NOT_SET, VALUE_NOT_SET, sampling_rate_hz);
  if (ret != 0) {
    return ret;
  }

  return audio_datapath_pres_delay_us_set(presentation_delay_us);
}

void audio_streaming_actions_start_pipeline(void)
{
  audio_system_start();
}

void audio_streaming_actions_stop_pipeline(void)
{
  if (stream_state_get() == STATE_STREAMING) {
    audio_system_stop();
  }
}

int audio_streaming_actions_restart_scan(void)
{
  return scan_start(NULL);
}

int audio_streaming_actions_restart_after_sync_loss(struct bt_le_per_adv_sync* pa_sync)
{
  int ret;

  audio_streaming_actions_stop_pipeline();

  if (pa_sync != NULL) {
    ret = bt_mgmt_pa_sync_delete(pa_sync);
    if (ret != 0 && ret != -EALREADY) {
      return ret;
    }
  }

  return audio_streaming_actions_restart_scan();
}

int audio_streaming_actions_restart_after_stream_stop(void)
{
  int ret;

  audio_streaming_actions_stop_pipeline();

  ret = broadcast_sink_disable();
  if (ret != 0) {
    return ret;
  }

  return audio_streaming_actions_restart_scan();
}

int audio_streaming_actions_stop(void)
{
  int ret;

  audio_streaming_actions_stop_pipeline();

  ret = bt_le_scan_stop();
  if (ret != 0 && ret != -EALREADY) {
    return ret;
  }

  return broadcast_sink_disable();
}
