/*
 * Copyright (c) 2026 Tiresias Firmware Team
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "codec_controller.h"

#include "zbus_common.h"

#include <zephyr/kernel.h>
#include <zephyr/zbus/zbus.h>

#define CODEC_CONTROLLER_THREAD_STACK_SIZE 1024
#define CODEC_CONTROLLER_THREAD_PRIORITY 3
#define CODEC_CONTROLLER_SUBSCRIBER_QUEUE_SIZE 8
#define CODEC_CONTROLLER_OBSERVER_PRIORITY 0
#define CODEC_CONTROLLER_ZBUS_TIMEOUT_MS 100

ZBUS_SUBSCRIBER_DEFINE(codec_controller_sub, CODEC_CONTROLLER_SUBSCRIBER_QUEUE_SIZE);

ZBUS_CHAN_DECLARE(audio_streaming_state_chan);

ZBUS_CHAN_DEFINE(codec_controller_cmd_chan, codec_controller_cmd_chan_msg, NULL, NULL,
    ZBUS_OBSERVERS(codec_controller_sub), ZBUS_MSG_INIT(.cmd = CODEC_CONTROLLER_CMD_INITIALIZE));

ZBUS_CHAN_DEFINE(codec_controller_state_chan, codec_controller_state_chan_msg, NULL, NULL, ZBUS_OBSERVERS_EMPTY,
    ZBUS_MSG_INIT(.state = CODEC_CONTROLLER_STATE_OFF));

ZBUS_CHAN_DEFINE(codec_controller_result_chan, codec_controller_result_chan_msg, NULL, NULL, ZBUS_OBSERVERS_EMPTY,
    ZBUS_MSG_INIT(.cmd = CODEC_CONTROLLER_CMD_INITIALIZE, .result = CODEC_CONTROLLER_RESULT_COMMAND_REJECTED,
        .error = 0));

ZBUS_CHAN_ADD_OBS(audio_streaming_state_chan, codec_controller_sub, CODEC_CONTROLLER_OBSERVER_PRIORITY);

static codec_controller_state current_state = CODEC_CONTROLLER_STATE_OFF;

static int __maybe_unused set_state(codec_controller_state state)
{
  codec_controller_state_chan_msg msg = {
    .state = state,
  };

  if (current_state == state) {
    return 0;
  }

  current_state = state;

  return zbus_chan_pub(&codec_controller_state_chan, &msg, K_MSEC(CODEC_CONTROLLER_ZBUS_TIMEOUT_MS));
}

static void handle_state_off(const struct zbus_channel* channel)
{
  /*
   * Pseudocode:
   *
   * if channel is not codec_controller_cmd_chan:
   *   ignore the notification
   *   return
   *
   * command = read codec_controller_cmd_chan
   * if command is not INITIALIZE:
   *   publish COMMAND_REJECTED on codec_controller_result_chan
   *   return
   *
   * set_state(INITIALIZING)
   * reset and boot the codec
   * program the initial codec configuration
   *
   * if initialization succeeds:
   *   set_state(READY)
   * else:
   *   publish OPERATION_FAILED on codec_controller_result_chan
   *   set_state(ERROR)
   */
  ARG_UNUSED(channel);
}

static void handle_state_initializing(const struct zbus_channel* channel)
{
  /*
   * Pseudocode:
   *
   * if channel is codec_controller_cmd_chan:
   *   command = read codec_controller_cmd_chan
   *   publish COMMAND_REJECTED on codec_controller_result_chan
   *   return
   *
   * ignore every other notification while initialization is in progress
   */
  ARG_UNUSED(channel);
}

static void handle_state_ready(const struct zbus_channel* channel)
{
  /*
   * Pseudocode:
   *
   * if channel is not codec_controller_cmd_chan:
   *   ignore the notification
   *   return
   *
   * command = read codec_controller_cmd_chan
   *
   * if command is START_AUDIO:
   *   configure and start the local microphone and DSP path
   *   if the operation succeeds:
   *     set_state(LOCAL_ONLY)
   *   else:
   *     publish OPERATION_FAILED on codec_controller_result_chan
   *     set_state(ERROR)
   *   return
   *
   * if command is POWER_DOWN:
   *   power down the codec
   *   if the operation succeeds:
   *     set_state(OFF)
   *   else:
   *     publish OPERATION_FAILED on codec_controller_result_chan
   *     set_state(ERROR)
   *   return
   *
   * publish COMMAND_REJECTED on codec_controller_result_chan
   */
  ARG_UNUSED(channel);
}

static void handle_state_local_only(const struct zbus_channel* channel)
{
  /*
   * Pseudocode:
   *
   * if channel is audio_streaming_state_chan:
   *   ignore the notification because the local path is already selected
   *   return
   *
   * if channel is not codec_controller_cmd_chan:
   *   ignore the notification
   *   return
   *
   * command = read codec_controller_cmd_chan
   *
   * if command is SELECT_BROADCAST:
   *   read the latest audio_streaming_state_chan value
   *   if Audio Streaming is not STREAMING:
   *     publish COMMAND_REJECTED on codec_controller_result_chan
   *     return
   *
   *   configure the codec to present only the broadcast input
   *   if the operation succeeds:
   *     set_state(BROADCAST_ONLY)
   *   else:
   *     publish OPERATION_FAILED on codec_controller_result_chan
   *     set_state(ERROR)
   *   return
   *
   * if command is STOP_CODEC:
   *   stop audio presentation
   *   if the operation succeeds:
   *     set_state(READY)
   *   else:
   *     publish OPERATION_FAILED on codec_controller_result_chan
   *     set_state(ERROR)
   *   return
   *
   * publish COMMAND_REJECTED on codec_controller_result_chan
   */
  ARG_UNUSED(channel);
}

static void handle_state_broadcast_only(const struct zbus_channel* channel)
{
  /*
   * Pseudocode:
   *
   * if channel is audio_streaming_state_chan:
   *   streaming_state = read audio_streaming_state_chan
   *   if streaming_state is not STREAMING:
   *     configure the codec to present only the local microphone and DSP path
   *     if the operation succeeds:
   *       set_state(LOCAL_ONLY)
   *     else:
   *       set_state(ERROR)
   *   return
   *
   * if channel is not codec_controller_cmd_chan:
   *   ignore the notification
   *   return
   *
   * command = read codec_controller_cmd_chan
   *
   * if command is SELECT_LOCAL:
   *   configure the codec to present only the local microphone and DSP path
   *   if the operation succeeds:
   *     set_state(LOCAL_ONLY)
   *   else:
   *     publish OPERATION_FAILED on codec_controller_result_chan
   *     set_state(ERROR)
   *   return
   *
   * if command is STOP_CODEC:
   *   stop audio presentation
   *   if the operation succeeds:
   *     set_state(READY)
   *   else:
   *     publish OPERATION_FAILED on codec_controller_result_chan
   *     set_state(ERROR)
   *   return
   *
   * publish COMMAND_REJECTED on codec_controller_result_chan
   */
  ARG_UNUSED(channel);
}

static void handle_state_error(const struct zbus_channel* channel)
{
  /*
   * Pseudocode:
   *
   * if channel is not codec_controller_cmd_chan:
   *   retain codec diagnostics
   *   ignore the notification
   *   return
   *
   * command = read codec_controller_cmd_chan
   * if command is not RESET:
   *   publish COMMAND_REJECTED on codec_controller_result_chan
   *   return
   *
   * set_state(INITIALIZING)
   * reset and reinitialize the codec
   *
   * if recovery succeeds:
   *   set_state(READY)
   * else:
   *   publish OPERATION_FAILED on codec_controller_result_chan
   *   set_state(ERROR)
   */
  ARG_UNUSED(channel);
}

static void codec_controller_state_machine(const struct zbus_channel* channel)
{
  switch (current_state) {
  case CODEC_CONTROLLER_STATE_OFF:
    handle_state_off(channel);
    break;
  case CODEC_CONTROLLER_STATE_INITIALIZING:
    handle_state_initializing(channel);
    break;
  case CODEC_CONTROLLER_STATE_READY:
    handle_state_ready(channel);
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
  }
}

static void codec_controller_thread(void)
{
  const struct zbus_channel* channel;

  while (1) {
    if (zbus_sub_wait(&codec_controller_sub, &channel, K_FOREVER) != 0) {
      continue;
    }

    codec_controller_state_machine(channel);
  }
}

K_THREAD_DEFINE(codec_controller_thread_id, CODEC_CONTROLLER_THREAD_STACK_SIZE, codec_controller_thread, NULL, NULL,
    NULL, CODEC_CONTROLLER_THREAD_PRIORITY, 0, 0);
