/*
 * Copyright (c) 2026 Tiresias Firmware Team
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "device_controller_actions.h"

#include "zbus_common.h"

#include <zephyr/kernel.h>
#include <zephyr/zbus/zbus.h>

#define DEVICE_CONTROLLER_ACTION_TIMEOUT_MS 100

ZBUS_CHAN_DECLARE(codec_controller_cmd_chan);
ZBUS_CHAN_DECLARE(control_link_cmd_chan);
ZBUS_CHAN_DECLARE(audio_streaming_cmd_chan);

int publish_codec_controller_command(codec_controller_cmd command)
{
  codec_controller_cmd_chan_msg msg = {
    .cmd = command,
  };

  return zbus_chan_pub(&codec_controller_cmd_chan, &msg, K_MSEC(DEVICE_CONTROLLER_ACTION_TIMEOUT_MS));
}

int publish_audio_streaming_command(audio_streaming_cmd command)
{
  audio_streaming_cmd_chan_msg msg = {
    .cmd = command,
  };

  return zbus_chan_pub(&audio_streaming_cmd_chan, &msg, K_MSEC(DEVICE_CONTROLLER_ACTION_TIMEOUT_MS));
}

int publish_control_link_command(control_link_cmd command)
{
  control_link_cmd_chan_msg msg = {
    .cmd = command,
  };

  return zbus_chan_pub(&control_link_cmd_chan, &msg, K_MSEC(DEVICE_CONTROLLER_ACTION_TIMEOUT_MS));
}
