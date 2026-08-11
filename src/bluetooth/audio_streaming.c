/*
 * Copyright (c) 2026 Tiresias Firmware Team
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "audio_streaming.h"

#include "audio_streaming_actions.h"
#include "streamctrl.h"
#include "zbus_common.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/zbus/zbus.h>

#define AUDIO_STREAMING_THREAD_STACK_SIZE 1024
#define AUDIO_STREAMING_THREAD_PRIORITY 3
#define AUDIO_STREAMING_SUBSCRIBER_QUEUE_SIZE 8
#define AUDIO_STREAMING_OBSERVER_PRIORITY 0
#define AUDIO_STREAMING_ZBUS_TIMEOUT_MS 100

LOG_MODULE_REGISTER(audio_streaming, CONFIG_LOG_DEFAULT_LEVEL);

ZBUS_SUBSCRIBER_DEFINE(audio_streaming_sub, AUDIO_STREAMING_SUBSCRIBER_QUEUE_SIZE);

ZBUS_CHAN_DECLARE(bt_mgmt_chan);
ZBUS_CHAN_DECLARE(le_audio_chan);

ZBUS_CHAN_DEFINE(audio_streaming_cmd_chan, audio_streaming_cmd_chan_msg, NULL, NULL,
    ZBUS_OBSERVERS(audio_streaming_sub), ZBUS_MSG_INIT(.cmd = AUDIO_STREAMING_CMD_ENABLE_RECEIVER));

ZBUS_CHAN_DEFINE(audio_streaming_state_chan, audio_streaming_state_chan_msg, NULL, NULL, ZBUS_OBSERVERS_EMPTY,
    ZBUS_MSG_INIT(.state = AUDIO_STREAMING_STATE_DISABLED));

ZBUS_CHAN_DEFINE(audio_streaming_result_chan, audio_streaming_result_chan_msg, NULL, NULL, ZBUS_OBSERVERS_EMPTY,
    ZBUS_MSG_INIT(.cmd = AUDIO_STREAMING_CMD_ENABLE_RECEIVER, .result = AUDIO_STREAMING_RESULT_COMMAND_REJECTED,
        .error = 0));

ZBUS_CHAN_ADD_OBS(bt_mgmt_chan, audio_streaming_sub, AUDIO_STREAMING_OBSERVER_PRIORITY);
ZBUS_CHAN_ADD_OBS(le_audio_chan, audio_streaming_sub, AUDIO_STREAMING_OBSERVER_PRIORITY);

static audio_streaming_state current_state = AUDIO_STREAMING_STATE_DISABLED;

static int set_state(audio_streaming_state state)
{
  audio_streaming_state_chan_msg msg = {
    .state = state,
  };

  if (current_state == state) {
    return 0;
  }

  LOG_INF("State transition: %d -> %d", current_state, state);
  current_state = state;

  return zbus_chan_pub(&audio_streaming_state_chan, &msg, K_MSEC(AUDIO_STREAMING_ZBUS_TIMEOUT_MS));
}

/* === Helper Functions === */

static void enter_error(void)
{
  int ret;

  audio_streaming_actions_stop_pipeline();

  ret = set_state(AUDIO_STREAMING_STATE_ERROR);
  if (ret != 0) {
    LOG_ERR("Failed to publish ERROR state: %d", ret);
  }
}

static int read_command(audio_streaming_cmd* command)
{
  audio_streaming_cmd_chan_msg msg;
  int ret = zbus_chan_read(&audio_streaming_cmd_chan, &msg, K_MSEC(AUDIO_STREAMING_ZBUS_TIMEOUT_MS));

  if (ret == 0) {
    *command = msg.cmd;
  }

  return ret;
}

static int read_bt_mgmt_event(struct bt_mgmt_msg* msg)
{
  return zbus_chan_read(&bt_mgmt_chan, msg, K_MSEC(AUDIO_STREAMING_ZBUS_TIMEOUT_MS));
}

static int read_le_audio_event(struct le_audio_msg* msg)
{
  return zbus_chan_read(&le_audio_chan, msg, K_MSEC(AUDIO_STREAMING_ZBUS_TIMEOUT_MS));
}

static void finish_scan_restart(int action_result)
{
  int ret;

  if (action_result != 0) {
    LOG_ERR("Failed to restart broadcast scanning: %d", action_result);
    enter_error();
    return;
  }

  ret = set_state(AUDIO_STREAMING_STATE_SCANNING);
  if (ret != 0) {
    LOG_ERR("Failed to publish SCANNING state: %d", ret);
    enter_error();
  }
}

static void stop_and_enter_idle(void)
{
  int ret = audio_streaming_actions_stop();

  if (ret != 0) {
    LOG_ERR("Failed to stop audio streaming: %d", ret);
    enter_error();
    return;
  }

  ret = set_state(AUDIO_STREAMING_STATE_IDLE);
  if (ret != 0) {
    LOG_ERR("Failed to publish IDLE state: %d", ret);
    enter_error();
  }
}

/* === State Handlers === */

static void handle_state_disabled(const struct zbus_channel* channel)
{
  audio_streaming_cmd command;
  int ret;

  if (channel != &audio_streaming_cmd_chan) {
    return;
  }

  ret = read_command(&command);
  if (ret != 0) {
    LOG_ERR("Failed to read Audio Streaming command: %d", ret);
    enter_error();
    return;
  }

  if (command != AUDIO_STREAMING_CMD_START_SCAN) {
    LOG_WRN("Command %d is not supported while DISABLED", command);
    return;
  }

  ret = audio_streaming_actions_start();
  if (ret != 0) {
    LOG_ERR("Failed to initialize audio streaming: %d", ret);
    enter_error();
    return;
  }

  ret = set_state(AUDIO_STREAMING_STATE_SCANNING);
  if (ret != 0) {
    LOG_ERR("Failed to publish SCANNING state: %d", ret);
    enter_error();
  }
}

static void handle_state_idle(const struct zbus_channel* channel)
{
  audio_streaming_cmd command;
  int ret;

  if (channel != &audio_streaming_cmd_chan) {
    return;
  }

  ret = read_command(&command);
  if (ret != 0) {
    LOG_ERR("Failed to read Audio Streaming command: %d", ret);
    enter_error();
    return;
  }

  if (command == AUDIO_STREAMING_CMD_STOP) {
    return;
  }

  if (command != AUDIO_STREAMING_CMD_START_SCAN) {
    LOG_WRN("Command %d is not supported while IDLE", command);
    return;
  }

  ret = audio_streaming_actions_start_scan();
  if (ret != 0) {
    LOG_ERR("Failed to start broadcast scanning: %d", ret);
    enter_error();
    return;
  }

  ret = set_state(AUDIO_STREAMING_STATE_SCANNING);
  if (ret != 0) {
    LOG_ERR("Failed to publish SCANNING state: %d", ret);
    enter_error();
  }
}

static void handle_state_scanning(const struct zbus_channel* channel)
{
  audio_streaming_cmd command;
  struct bt_mgmt_msg msg;
  int ret;

  if (channel == &bt_mgmt_chan) {
    ret = read_bt_mgmt_event(&msg);
    if (ret != 0) {
      LOG_ERR("Failed to read Bluetooth management event: %d", ret);
      enter_error();
      return;
    }

    if (msg.event != BT_MGMT_PA_SYNCED) {
      return;
    }

    ret = audio_streaming_actions_set_pa_sync(msg.pa_sync, msg.broadcast_id);
    if (ret != 0) {
      LOG_ERR("Failed to accept periodic advertising sync: %d", ret);
      enter_error();
      return;
    }

    ret = set_state(AUDIO_STREAMING_STATE_PA_SYNCED);
    if (ret != 0) {
      LOG_ERR("Failed to publish PA_SYNCED state: %d", ret);
      enter_error();
    }
    return;
  }

  if (channel != &audio_streaming_cmd_chan) {
    return;
  }

  ret = read_command(&command);
  if (ret != 0) {
    LOG_ERR("Failed to read Audio Streaming command: %d", ret);
    enter_error();
    return;
  }

  if (command == AUDIO_STREAMING_CMD_STOP) {
    stop_and_enter_idle();
    return;
  }

  LOG_WRN("Command %d is not supported while SCANNING", command);
}

static void handle_state_pa_synced(const struct zbus_channel* channel)
{
  audio_streaming_cmd command;
  struct bt_mgmt_msg bt_msg;
  struct le_audio_msg audio_msg;
  int ret;

  if (channel == &le_audio_chan) {
    ret = read_le_audio_event(&audio_msg);
    if (ret != 0) {
      LOG_ERR("Failed to read LE Audio event: %d", ret);
      enter_error();
      return;
    }

    if (audio_msg.event == LE_AUDIO_EVT_CONFIG_RECEIVED) {
      ret = audio_streaming_actions_configure_pipeline();
      if (ret != 0) {
        LOG_ERR("Failed to configure the audio pipeline: %d", ret);
        enter_error();
        return;
      }

      ret = set_state(AUDIO_STREAMING_STATE_BIS_SYNCING);
      if (ret != 0) {
        LOG_ERR("Failed to publish BIS_SYNCING state: %d", ret);
        enter_error();
      }
      return;
    }

    if (audio_msg.event == LE_AUDIO_EVT_SYNC_LOST) {
      finish_scan_restart(audio_streaming_actions_restart_after_sync_loss(audio_msg.pa_sync));
      return;
    }

    if (audio_msg.event == LE_AUDIO_EVT_NO_VALID_CFG) {
      finish_scan_restart(audio_streaming_actions_restart_after_stream_stop());
    }
    return;
  }

  if (channel == &bt_mgmt_chan) {
    ret = read_bt_mgmt_event(&bt_msg);
    if (ret != 0) {
      LOG_ERR("Failed to read Bluetooth management event: %d", ret);
      enter_error();
      return;
    }

    if (bt_msg.event == BT_MGMT_PA_SYNC_LOST) {
      finish_scan_restart(audio_streaming_actions_restart_scan());
    }
    return;
  }

  if (channel != &audio_streaming_cmd_chan) {
    return;
  }

  ret = read_command(&command);
  if (ret != 0) {
    LOG_ERR("Failed to read Audio Streaming command: %d", ret);
    enter_error();
    return;
  }

  if (command == AUDIO_STREAMING_CMD_STOP) {
    stop_and_enter_idle();
    return;
  }

  LOG_WRN("Command %d is not supported while PA_SYNCED", command);
}

static void handle_state_bis_syncing(const struct zbus_channel* channel)
{
  audio_streaming_cmd command;
  struct bt_mgmt_msg bt_msg;
  struct le_audio_msg audio_msg;
  int ret;

  if (channel == &le_audio_chan) {
    ret = read_le_audio_event(&audio_msg);
    if (ret != 0) {
      LOG_ERR("Failed to read LE Audio event: %d", ret);
      enter_error();
      return;
    }

    if (audio_msg.event == LE_AUDIO_EVT_STREAMING) {
      audio_streaming_actions_start_pipeline();

      ret = set_state(AUDIO_STREAMING_STATE_STREAMING);
      if (ret != 0) {
        LOG_ERR("Failed to publish STREAMING state: %d", ret);
        enter_error();
      }
      return;
    }

    if (audio_msg.event == LE_AUDIO_EVT_SYNC_LOST) {
      finish_scan_restart(audio_streaming_actions_restart_after_sync_loss(audio_msg.pa_sync));
      return;
    }

    if (audio_msg.event == LE_AUDIO_EVT_NOT_STREAMING || audio_msg.event == LE_AUDIO_EVT_NO_VALID_CFG) {
      finish_scan_restart(audio_streaming_actions_restart_after_stream_stop());
    }
    return;
  }

  if (channel == &bt_mgmt_chan) {
    ret = read_bt_mgmt_event(&bt_msg);
    if (ret != 0) {
      LOG_ERR("Failed to read Bluetooth management event: %d", ret);
      enter_error();
      return;
    }

    if (bt_msg.event == BT_MGMT_PA_SYNC_LOST) {
      audio_streaming_actions_stop_pipeline();
      finish_scan_restart(audio_streaming_actions_restart_scan());
    }
    return;
  }

  if (channel != &audio_streaming_cmd_chan) {
    return;
  }

  ret = read_command(&command);
  if (ret != 0) {
    LOG_ERR("Failed to read Audio Streaming command: %d", ret);
    enter_error();
    return;
  }

  if (command == AUDIO_STREAMING_CMD_STOP) {
    stop_and_enter_idle();
    return;
  }

  LOG_WRN("Command %d is not supported while BIS_SYNCING", command);
}

static void handle_state_streaming(const struct zbus_channel* channel)
{
  audio_streaming_cmd command;
  struct bt_mgmt_msg bt_msg;
  struct le_audio_msg audio_msg;
  int ret;

  if (channel == &le_audio_chan) {
    ret = read_le_audio_event(&audio_msg);
    if (ret != 0) {
      LOG_ERR("Failed to read LE Audio event: %d", ret);
      enter_error();
      return;
    }

    if (audio_msg.event == LE_AUDIO_EVT_SYNC_LOST) {
      finish_scan_restart(audio_streaming_actions_restart_after_sync_loss(audio_msg.pa_sync));
      return;
    }

    if (audio_msg.event == LE_AUDIO_EVT_NOT_STREAMING || audio_msg.event == LE_AUDIO_EVT_NO_VALID_CFG) {
      finish_scan_restart(audio_streaming_actions_restart_after_stream_stop());
    }
    return;
  }

  if (channel == &bt_mgmt_chan) {
    ret = read_bt_mgmt_event(&bt_msg);
    if (ret != 0) {
      LOG_ERR("Failed to read Bluetooth management event: %d", ret);
      enter_error();
      return;
    }

    if (bt_msg.event == BT_MGMT_PA_SYNC_LOST) {
      audio_streaming_actions_stop_pipeline();
      finish_scan_restart(audio_streaming_actions_restart_scan());
    }
    return;
  }

  if (channel != &audio_streaming_cmd_chan) {
    return;
  }

  ret = read_command(&command);
  if (ret != 0) {
    LOG_ERR("Failed to read Audio Streaming command: %d", ret);
    enter_error();
    return;
  }

  if (command == AUDIO_STREAMING_CMD_STOP) {
    stop_and_enter_idle();
    return;
  }

  LOG_WRN("Command %d is not supported while STREAMING", command);
}

static void handle_state_recovering(const struct zbus_channel* channel)
{
  ARG_UNUSED(channel);

  LOG_WRN("RECOVERING is not implemented by the PoC");
}

static void handle_state_error(const struct zbus_channel* channel)
{
  ARG_UNUSED(channel);
}

void streamctrl_send(void const* const data, size_t size, uint8_t num_ch)
{
  ARG_UNUSED(data);
  ARG_UNUSED(size);
  ARG_UNUSED(num_ch);

  LOG_WRN("Sending is not possible for broadcast sink");
}

/* === State Machine === */

static void audio_streaming_state_machine(const struct zbus_channel* channel)
{
  switch (current_state) {
  case AUDIO_STREAMING_STATE_DISABLED:
    handle_state_disabled(channel);
    break;
  case AUDIO_STREAMING_STATE_IDLE:
    handle_state_idle(channel);
    break;
  case AUDIO_STREAMING_STATE_SCANNING:
    handle_state_scanning(channel);
    break;
  case AUDIO_STREAMING_STATE_PA_SYNCED:
    handle_state_pa_synced(channel);
    break;
  case AUDIO_STREAMING_STATE_BIS_SYNCING:
    handle_state_bis_syncing(channel);
    break;
  case AUDIO_STREAMING_STATE_STREAMING:
    handle_state_streaming(channel);
    break;
  case AUDIO_STREAMING_STATE_RECOVERING:
    handle_state_recovering(channel);
    break;
  case AUDIO_STREAMING_STATE_ERROR:
    handle_state_error(channel);
    break;
  default:
    LOG_ERR("Unknown Audio Streaming state: %d", current_state);
    enter_error();
    break;
  }
}

static void audio_streaming_thread(void)
{
  const struct zbus_channel* channel;

  while (1) {
    if (zbus_sub_wait(&audio_streaming_sub, &channel, K_FOREVER) != 0) {
      continue;
    }

    audio_streaming_state_machine(channel);
  }
}

K_THREAD_DEFINE(audio_streaming_thread_id, AUDIO_STREAMING_THREAD_STACK_SIZE, audio_streaming_thread, NULL, NULL, NULL,
    AUDIO_STREAMING_THREAD_PRIORITY, 0, 0);
