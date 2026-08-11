/*
 * Copyright (c) 2026 Tiresias Firmware Team
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "codec_controller.h"

#include "hw_codec.h"
#include "zbus_common.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/zbus/zbus.h>

#define CODEC_CONTROLLER_THREAD_STACK_SIZE 8192
#define CODEC_CONTROLLER_THREAD_PRIORITY 3
#define CODEC_CONTROLLER_SUBSCRIBER_QUEUE_SIZE 8
#define CODEC_CONTROLLER_OBSERVER_PRIORITY 0
#define CODEC_CONTROLLER_ZBUS_TIMEOUT_MS 100

LOG_MODULE_REGISTER(codec_controller, CONFIG_LOG_DEFAULT_LEVEL);

ZBUS_SUBSCRIBER_DEFINE(codec_controller_sub, CODEC_CONTROLLER_SUBSCRIBER_QUEUE_SIZE);

ZBUS_CHAN_DECLARE(audio_streaming_state_chan);
ZBUS_CHAN_DECLARE(led_chan);

ZBUS_CHAN_DEFINE(codec_controller_cmd_chan, codec_controller_cmd_chan_msg, NULL, NULL,
    ZBUS_OBSERVERS(codec_controller_sub), ZBUS_MSG_INIT(.cmd = CODEC_CONTROLLER_CMD_INITIALIZE));

ZBUS_CHAN_DEFINE(codec_controller_state_chan, codec_controller_state_chan_msg, NULL, NULL, ZBUS_OBSERVERS_EMPTY,
    ZBUS_MSG_INIT(.state = CODEC_CONTROLLER_STATE_OFF));

ZBUS_CHAN_DEFINE(codec_controller_result_chan, codec_controller_result_chan_msg, NULL, NULL, ZBUS_OBSERVERS_EMPTY,
    ZBUS_MSG_INIT(.cmd = CODEC_CONTROLLER_CMD_INITIALIZE, .result = CODEC_CONTROLLER_RESULT_COMMAND_REJECTED,
        .error = 0));

ZBUS_CHAN_ADD_OBS(audio_streaming_state_chan, codec_controller_sub, CODEC_CONTROLLER_OBSERVER_PRIORITY);

static codec_controller_state current_state = CODEC_CONTROLLER_STATE_OFF;

static int set_state(codec_controller_state state)
{
  codec_controller_state_chan_msg msg = {
    .state = state,
  };

  if (current_state == state) {
    return 0;
  }

  LOG_INF("State transition: %d -> %d", current_state, state);
  current_state = state;

  return zbus_chan_pub(&codec_controller_state_chan, &msg, K_MSEC(CODEC_CONTROLLER_ZBUS_TIMEOUT_MS));
}

/* === Helper Functions === */

static void enter_error(void)
{
  int ret = set_state(CODEC_CONTROLLER_STATE_ERROR);

  if (ret != 0) {
    LOG_ERR("Failed to publish ERROR state: %d", ret);
  }
}

static int read_audio_streaming_state(audio_streaming_state* state)
{
  audio_streaming_state_chan_msg msg;
  int ret = zbus_chan_read(&audio_streaming_state_chan, &msg, K_MSEC(CODEC_CONTROLLER_ZBUS_TIMEOUT_MS));

  if (ret == 0) {
    *state = msg.state;
  }

  return ret;
}

static void publish_led_command(led_cmd_t command)
{
  led_chan_msg_t msg = {
    .led = LED_2,
    .cmd = command,
  };
  int ret = zbus_chan_pub(&led_chan, &msg, K_MSEC(CODEC_CONTROLLER_ZBUS_TIMEOUT_MS));

  if (ret != 0) {
    LOG_ERR("Failed to publish codec indication: %d", ret);
  }
}

static int select_local_mode(void)
{
  int ret = hw_codec_select_local();

  if (ret != 0) {
    return ret;
  }

  ret = set_state(CODEC_CONTROLLER_STATE_LOCAL_ONLY);
  if (ret != 0) {
    return ret;
  }

  publish_led_command(BLINK);

  return 0;
}

static int select_broadcast_mode(void)
{
  int ret = hw_codec_select_i2s();

  if (ret != 0) {
    return ret;
  }

  ret = set_state(CODEC_CONTROLLER_STATE_BROADCAST_ONLY);
  if (ret != 0) {
    return ret;
  }

  publish_led_command(TURN_ON);

  return 0;
}

/* === State Handlers === */

static void handle_state_off(const struct zbus_channel* channel)
{
  codec_controller_cmd_chan_msg msg;
  int ret;

  if (channel != &codec_controller_cmd_chan) {
    return;
  }

  ret = zbus_chan_read(channel, &msg, K_MSEC(CODEC_CONTROLLER_ZBUS_TIMEOUT_MS));
  if (ret != 0) {
    LOG_ERR("Failed to read Codec Controller command: %d", ret);
    enter_error();
    return;
  }

  if (msg.cmd != CODEC_CONTROLLER_CMD_INITIALIZE) {
    LOG_WRN("Command %d is not supported while OFF", msg.cmd);
    return;
  }

  ret = set_state(CODEC_CONTROLLER_STATE_INITIALIZING);
  if (ret != 0) {
    LOG_ERR("Failed to publish INITIALIZING state: %d", ret);
    enter_error();
    return;
  }

  ret = hw_codec_init();
  if (ret != 0) {
    LOG_ERR("Failed to initialize the hardware codec: %d", ret);
    enter_error();
    return;
  }

  ret = select_local_mode();
  if (ret != 0) {
    LOG_ERR("Failed to select local audio after initialization: %d", ret);
    enter_error();
  }
}

static void handle_state_initializing(const struct zbus_channel* channel)
{
  ARG_UNUSED(channel);

  LOG_WRN("Ignoring notification while codec initialization is in progress");
}

static void handle_state_local_only(const struct zbus_channel* channel)
{
  codec_controller_cmd_chan_msg msg;
  audio_streaming_state streaming_state;
  int ret;

  if (channel != &codec_controller_cmd_chan) {
    return;
  }

  ret = zbus_chan_read(channel, &msg, K_MSEC(CODEC_CONTROLLER_ZBUS_TIMEOUT_MS));
  if (ret != 0) {
    LOG_ERR("Failed to read Codec Controller command: %d", ret);
    enter_error();
    return;
  }

  if (msg.cmd != CODEC_CONTROLLER_CMD_SELECT_BROADCAST) {
    LOG_WRN("Command %d is not supported while LOCAL_ONLY", msg.cmd);
    return;
  }

  ret = read_audio_streaming_state(&streaming_state);
  if (ret != 0) {
    LOG_ERR("Failed to read Audio Streaming state: %d", ret);
    enter_error();
    return;
  }

  if (streaming_state != AUDIO_STREAMING_STATE_STREAMING) {
    LOG_INF("Broadcast audio is not available");
    return;
  }

  ret = select_broadcast_mode();
  if (ret != 0) {
    LOG_ERR("Failed to select broadcast audio: %d", ret);
    enter_error();
  }
}

static void handle_state_broadcast_only(const struct zbus_channel* channel)
{
  codec_controller_cmd_chan_msg msg;
  audio_streaming_state streaming_state;
  int ret;

  if (channel == &audio_streaming_state_chan) {
    ret = read_audio_streaming_state(&streaming_state);
    if (ret != 0) {
      LOG_ERR("Failed to read Audio Streaming state: %d", ret);
      enter_error();
      return;
    }

    if (streaming_state == AUDIO_STREAMING_STATE_STREAMING) {
      return;
    }

    LOG_INF("Audio streaming stopped; falling back to local audio");
    ret = select_local_mode();
    if (ret != 0) {
      LOG_ERR("Failed to fall back to local audio: %d", ret);
      enter_error();
    }
    return;
  }

  if (channel != &codec_controller_cmd_chan) {
    return;
  }

  ret = zbus_chan_read(channel, &msg, K_MSEC(CODEC_CONTROLLER_ZBUS_TIMEOUT_MS));
  if (ret != 0) {
    LOG_ERR("Failed to read Codec Controller command: %d", ret);
    enter_error();
    return;
  }

  if (msg.cmd != CODEC_CONTROLLER_CMD_SELECT_LOCAL) {
    LOG_WRN("Command %d is not supported while BROADCAST_ONLY", msg.cmd);
    return;
  }

  ret = select_local_mode();
  if (ret != 0) {
    LOG_ERR("Failed to select local audio: %d", ret);
    enter_error();
  }
}

static void handle_state_error(const struct zbus_channel* channel)
{
  ARG_UNUSED(channel);
}

/* === State Machine === */

static void codec_controller_state_machine(const struct zbus_channel* channel)
{
  switch (current_state) {
  case CODEC_CONTROLLER_STATE_OFF:
    handle_state_off(channel);
    break;
  case CODEC_CONTROLLER_STATE_INITIALIZING:
    handle_state_initializing(channel);
    break;
  case CODEC_CONTROLLER_STATE_LOCAL_ONLY:
    handle_state_local_only(channel);
    break;
  case CODEC_CONTROLLER_STATE_BROADCAST_ONLY:
    handle_state_broadcast_only(channel);
    break;
  case CODEC_CONTROLLER_STATE_ERROR:
    handle_state_error(channel);
    break;
  default:
    LOG_ERR("Unknown Codec Controller state: %d", current_state);
    enter_error();
    break;
  }
}

/* === Thread === */

static void codec_controller_thread(void* arg1, void* arg2, void* arg3)
{
  const struct zbus_channel* channel;

  ARG_UNUSED(arg1);
  ARG_UNUSED(arg2);
  ARG_UNUSED(arg3);

  while (1) {
    if (zbus_sub_wait(&codec_controller_sub, &channel, K_FOREVER) != 0) {
      continue;
    }

    codec_controller_state_machine(channel);
  }
}

K_THREAD_DEFINE(codec_controller_thread_id, CODEC_CONTROLLER_THREAD_STACK_SIZE, codec_controller_thread, NULL, NULL,
    NULL, CODEC_CONTROLLER_THREAD_PRIORITY, 0, 0);
