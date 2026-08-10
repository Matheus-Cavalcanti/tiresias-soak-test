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
   * if command is not CODEC_CONTROLLER_CMD_INITIALIZE:
   *   log that only CODEC_CONTROLLER_CMD_INITIALIZE is supported while OFF by the PoC
   *   return
   *
   * set_state(INITIALIZING)
   * result = codec_controller_actions_start_local()
   *   this action resets, initializes, and starts the local audio path
   *
   * if result succeeds:
   *   set_state(LOCAL_ONLY)
   * else:
   *   set_state(ERROR)
   */
  ARG_UNUSED(channel);
}

static void handle_state_initializing(const struct zbus_channel* channel)
{
  /*
   * Pseudocode:
   *
   * initialization runs synchronously in codec_controller_actions_start_local()
   * log and ignore any unexpected notification while it is in progress
   */
  ARG_UNUSED(channel);
}

static void handle_state_local_only(const struct zbus_channel* channel)
{
  /*
   * Pseudocode:
   *
   * if channel is audio_streaming_state_chan:
   *   cache whether Audio Streaming is STREAMING
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
   *     log that broadcast audio is unavailable and remain LOCAL_ONLY
   *     return
   *
   *   result = codec_controller_actions_select_broadcast()
   *   if result succeeds:
   *     set_state(BROADCAST_ONLY)
   *   else:
   *     set_state(ERROR)
   *   return
   *
   * if command is SELECT_LOCAL:
   *   local audio is already selected; do nothing
   *   return
   *
   * log that every other command is deferred or invalid for the PoC
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
   *     result = codec_controller_actions_select_local()
   *     if result succeeds:
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
   *   result = codec_controller_actions_select_local()
   *   if result succeeds:
   *     set_state(LOCAL_ONLY)
   *   else:
   *     set_state(ERROR)
   *   return
   *
   * if command is SELECT_BROADCAST:
   *   broadcast audio is already selected; do nothing
   *   return
   *
   * log that every other command is deferred or invalid for the PoC
   */
  ARG_UNUSED(channel);
}

static void handle_state_error(const struct zbus_channel* channel)
{
  /*
   * Pseudocode:
   *
   * retain codec diagnostics
   * log that the PoC uses fail-stop behavior and requires a device reboot
   * ignore every notification; RESET and recovery are deferred
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
