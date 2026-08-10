/*
 * Copyright (c) 2026 Tiresias Firmware Team
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "device_controller.h"

#include "device_controller_actions.h"

#include "zbus_common.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/zbus/zbus.h>

#define DEVICE_CONTROLLER_SUBSCRIBER_QUEUE_SIZE 8
#define DEVICE_CONTROLLER_OBSERVER_PRIORITY 0
#define DEVICE_CONTROLLER_ZBUS_TIMEOUT_MS 100

LOG_MODULE_REGISTER(device_controller, CONFIG_LOG_DEFAULT_LEVEL);

ZBUS_SUBSCRIBER_DEFINE(device_controller_sub, DEVICE_CONTROLLER_SUBSCRIBER_QUEUE_SIZE);

ZBUS_CHAN_DECLARE(button_chan);
ZBUS_CHAN_DECLARE(codec_controller_state_chan);
ZBUS_CHAN_DECLARE(audio_streaming_state_chan);

ZBUS_CHAN_DEFINE(device_controller_cmd_chan, device_controller_cmd_chan_msg, NULL, NULL,
    ZBUS_OBSERVERS(device_controller_sub), ZBUS_MSG_INIT(.cmd = DEVICE_CONTROLLER_CMD_START));

ZBUS_CHAN_DEFINE(device_controller_state_chan, device_controller_state_chan_msg, NULL, NULL, ZBUS_OBSERVERS_EMPTY,
    ZBUS_MSG_INIT(.state = DEVICE_CONTROLLER_STATE_OFF));

ZBUS_CHAN_ADD_OBS(button_chan, device_controller_sub, DEVICE_CONTROLLER_OBSERVER_PRIORITY);
ZBUS_CHAN_ADD_OBS(codec_controller_state_chan, device_controller_sub, DEVICE_CONTROLLER_OBSERVER_PRIORITY);
ZBUS_CHAN_ADD_OBS(audio_streaming_state_chan, device_controller_sub, DEVICE_CONTROLLER_OBSERVER_PRIORITY);

static device_controller_state current_state = DEVICE_CONTROLLER_STATE_OFF;

/*
 * TODO: After the initial control flow is validated, add per-destination
 * outstanding-command tracking, result correlation, stale-result detection,
 * completion deadlines, bounded retries, and escalation policies.
 */

static int set_state(device_controller_state state)
{
  device_controller_state_chan_msg msg = {
    .state = state,
  };

  if (current_state == state) {
    return 0;
  }

  LOG_INF("State transition: %d -> %d", current_state, state);
  current_state = state;

  return zbus_chan_pub(&device_controller_state_chan, &msg, K_MSEC(DEVICE_CONTROLLER_ZBUS_TIMEOUT_MS));
}

/* === Helper Functions === */

static void enter_fault(void)
{
  int ret = set_state(DEVICE_CONTROLLER_STATE_FAULT);

  if (ret != 0) {
    LOG_ERR("Failed to publish FAULT state: %d", ret);
  }
}

static int read_codec_controller_state(codec_controller_state* state)
{
  codec_controller_state_chan_msg msg;
  int ret = zbus_chan_read(&codec_controller_state_chan, &msg, K_MSEC(DEVICE_CONTROLLER_ZBUS_TIMEOUT_MS));

  if (ret == 0) {
    *state = msg.state;
  }

  return ret;
}

static int read_audio_streaming_state(audio_streaming_state* state)
{
  audio_streaming_state_chan_msg msg;
  int ret = zbus_chan_read(&audio_streaming_state_chan, &msg, K_MSEC(DEVICE_CONTROLLER_ZBUS_TIMEOUT_MS));

  if (ret == 0) {
    *state = msg.state;
  }

  return ret;
}

static bool streaming_discovery_is_active(audio_streaming_state state)
{
  return state == AUDIO_STREAMING_STATE_SCANNING || state == AUDIO_STREAMING_STATE_PA_SYNCED
      || state == AUDIO_STREAMING_STATE_BIS_SYNCING || state == AUDIO_STREAMING_STATE_STREAMING;
}

/* === State Handlers === */

static void handle_state_off(const struct zbus_channel* channel)
{
  ARG_UNUSED(channel);
  int ret;

  ret = set_state(DEVICE_CONTROLLER_STATE_INITIALIZING);
  if (ret != 0) {
    LOG_ERR("Failed to publish INITIALIZING state: %d", ret);
    enter_fault();
    return;
  }

  ret = publish_codec_controller_command(CODEC_CONTROLLER_CMD_INITIALIZE);
  if (ret != 0) {
    LOG_ERR("Failed to request codec initialization: %d", ret);
    enter_fault();
    return;
  }

  ret = publish_audio_streaming_command(AUDIO_STREAMING_CMD_START_SCAN);
  if (ret != 0) {
    LOG_ERR("Failed to request audio streaming startup: %d", ret);
    enter_fault();
  }
}

static void handle_state_initializing(const struct zbus_channel* channel)
{
  codec_controller_state codec_state;
  audio_streaming_state streaming_state;
  int ret;

  if (channel != &codec_controller_state_chan && channel != &audio_streaming_state_chan) {
    return;
  }

  ret = read_codec_controller_state(&codec_state);
  if (ret != 0) {
    LOG_ERR("Failed to read Codec Controller state: %d", ret);
    enter_fault();
    return;
  }

  ret = read_audio_streaming_state(&streaming_state);
  if (ret != 0) {
    LOG_ERR("Failed to read Audio Streaming state: %d", ret);
    enter_fault();
    return;
  }

  if (codec_state == CODEC_CONTROLLER_STATE_ERROR || streaming_state == AUDIO_STREAMING_STATE_ERROR) {
    LOG_ERR("A required subsystem failed during initialization");
    enter_fault();
    return;
  }

  if (codec_state != CODEC_CONTROLLER_STATE_LOCAL_ONLY || !streaming_discovery_is_active(streaming_state)) {
    return;
  }

  ret = set_state(DEVICE_CONTROLLER_STATE_OPERATIONAL);
  if (ret != 0) {
    LOG_ERR("Failed to publish OPERATIONAL state: %d", ret);
    enter_fault();
  }
}

static void handle_state_operational(const struct zbus_channel* channel)
{
  codec_controller_state codec_state;
  audio_streaming_state streaming_state;
  int ret;

  if (channel == &button_chan) {
    ret = read_codec_controller_state(&codec_state);
    if (ret != 0) {
      LOG_ERR("Failed to read Codec Controller state: %d", ret);
      enter_fault();
      return;
    }

    ret = read_audio_streaming_state(&streaming_state);
    if (ret != 0) {
      LOG_ERR("Failed to read Audio Streaming state: %d", ret);
      enter_fault();
      return;
    }

    if (codec_state == CODEC_CONTROLLER_STATE_ERROR || streaming_state == AUDIO_STREAMING_STATE_ERROR) {
      LOG_ERR("A required subsystem reported an error");
      enter_fault();
      return;
    }

    if (codec_state == CODEC_CONTROLLER_STATE_LOCAL_ONLY) {
      if (streaming_state != AUDIO_STREAMING_STATE_STREAMING) {
        LOG_INF("Broadcast audio is not available yet");
        return;
      }

      ret = publish_codec_controller_command(CODEC_CONTROLLER_CMD_SELECT_BROADCAST);
      if (ret != 0) {
        LOG_ERR("Failed to request broadcast audio: %d", ret);
        enter_fault();
      }
      return;
    }

    if (codec_state == CODEC_CONTROLLER_STATE_BROADCAST_ONLY) {
      ret = publish_codec_controller_command(CODEC_CONTROLLER_CMD_SELECT_LOCAL);
      if (ret != 0) {
        LOG_ERR("Failed to request local audio: %d", ret);
        enter_fault();
      }
      return;
    }

    LOG_WRN("Codec state %d cannot handle a mode switch", codec_state);
    return;
  }

  if (channel == &codec_controller_state_chan) {
    ret = read_codec_controller_state(&codec_state);
    if (ret != 0) {
      LOG_ERR("Failed to read Codec Controller state: %d", ret);
      enter_fault();
      return;
    }

    if (codec_state == CODEC_CONTROLLER_STATE_ERROR) {
      LOG_ERR("Codec Controller entered ERROR");
      enter_fault();
      return;
    }

    return;
  }

  if (channel == &audio_streaming_state_chan) {
    ret = read_audio_streaming_state(&streaming_state);
    if (ret != 0) {
      LOG_ERR("Failed to read Audio Streaming state: %d", ret);
      enter_fault();
      return;
    }

    if (streaming_state == AUDIO_STREAMING_STATE_ERROR) {
      LOG_ERR("Audio Streaming entered ERROR");
      enter_fault();
    }
  }
}

static void handle_state_low_power(const struct zbus_channel* channel)
{
  ARG_UNUSED(channel);

  LOG_WRN("LOW_POWER is not implemented by the PoC");
}

static void handle_state_fault(const struct zbus_channel* channel)
{
  ARG_UNUSED(channel);
}

/* === State Machine === */

static void device_controller_state_machine(const struct zbus_channel* channel)
{
  switch (current_state) {
  case DEVICE_CONTROLLER_STATE_OFF:
    handle_state_off(channel);
    break;
  case DEVICE_CONTROLLER_STATE_INITIALIZING:
    handle_state_initializing(channel);
    break;
  case DEVICE_CONTROLLER_STATE_OPERATIONAL:
    handle_state_operational(channel);
    break;
  case DEVICE_CONTROLLER_STATE_LOW_POWER:
    handle_state_low_power(channel);
    break;
  case DEVICE_CONTROLLER_STATE_FAULT:
    handle_state_fault(channel);
    break;
  default:
    LOG_ERR("Unknown Device Controller state: %d", current_state);
    enter_fault();
    break;
  }
}

int device_controller_run(void)
{
  const device_controller_cmd_chan_msg start_msg = {
    .cmd = DEVICE_CONTROLLER_CMD_START,
  };
  const struct zbus_channel* channel;
  int ret;

  ret = zbus_chan_pub(&device_controller_cmd_chan, &start_msg, K_MSEC(DEVICE_CONTROLLER_ZBUS_TIMEOUT_MS));
  if (ret != 0) {
    LOG_ERR("Failed to publish initial START command: %d", ret);
    return ret;
  }

  LOG_INF("Running on the main thread");

  while (1) {
    ret = zbus_sub_wait(&device_controller_sub, &channel, K_FOREVER);
    if (ret != 0) {
      LOG_ERR("Failed to wait for a Device Controller notification: %d", ret);
      return ret;
    }

    device_controller_state_machine(channel);
  }
}
