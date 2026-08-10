/*
 * Copyright (c) 2026 Tiresias Firmware Team
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "audio_streaming.h"

#include "zbus_common.h"

#include <zephyr/kernel.h>
#include <zephyr/zbus/zbus.h>

#define AUDIO_STREAMING_THREAD_STACK_SIZE 1024
#define AUDIO_STREAMING_THREAD_PRIORITY 3
#define AUDIO_STREAMING_SUBSCRIBER_QUEUE_SIZE 8
#define AUDIO_STREAMING_OBSERVER_PRIORITY 0
#define AUDIO_STREAMING_ZBUS_TIMEOUT_MS 100

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

static int __maybe_unused set_state(audio_streaming_state state)
{
  audio_streaming_state_chan_msg msg = {
    .state = state,
  };

  if (current_state == state) {
    return 0;
  }

  current_state = state;

  return zbus_chan_pub(&audio_streaming_state_chan, &msg, K_MSEC(AUDIO_STREAMING_ZBUS_TIMEOUT_MS));
}

static void handle_state_disabled(const struct zbus_channel* channel)
{
  /*
   * Pseudocode:
   *
   * if channel is not audio_streaming_cmd_chan:
   *   ignore the notification
   *   return
   *
   * command = read audio_streaming_cmd_chan
   * if command is not START_SCAN:
   *   log that only START_SCAN is supported while DISABLED by the PoC
   *   return
   *
   * result = audio_streaming_actions_start()
   *   this action initializes the receiver and BIS data path, then starts scanning
   *
   * if result succeeds:
   *   set_state(SCANNING)
   * else:
   *   set_state(ERROR)
   */
  ARG_UNUSED(channel);
}

static void handle_state_idle(const struct zbus_channel* channel)
{
  /*
   * Pseudocode:
   *
   * if channel is not audio_streaming_cmd_chan:
   *   ignore the notification
   *   return
   *
   * command = read audio_streaming_cmd_chan
   *
   * if command is START_SCAN:
   *   result = audio_streaming_actions_start_scan()
   *   if result succeeds:
   *     set_state(SCANNING)
   *   else:
   *     set_state(ERROR)
   *   return
   *
   * if command is STOP:
   *   reception is already stopped; do nothing
   *   return
   *
   * log that every other command is deferred or invalid for the PoC
   */
  ARG_UNUSED(channel);
}

static void handle_state_scanning(const struct zbus_channel* channel)
{
  /*
   * Pseudocode:
   *
   * if channel is bt_mgmt_chan:
   *   event = read bt_mgmt_chan
   *
   *   if event is PA_SYNC_ESTABLISHED:
   *     retain the periodic advertising synchronization
   *     set_state(PA_SYNCED)
   *     return
   *
   *   if event represents a Bluetooth failure:
   *     set_state(ERROR)
   *   return
   *
   * if channel is not audio_streaming_cmd_chan:
   *   ignore the notification
   *   return
   *
   * command = read audio_streaming_cmd_chan
   * if command is STOP:
   *   result = audio_streaming_actions_stop()
   *   if result succeeds:
   *     set_state(IDLE)
   *   else:
   *     set_state(ERROR)
   *   return
   *
   * log that STOP_SCAN, DISABLE_RECEIVER, and RESET are deferred
   */
  ARG_UNUSED(channel);
}

static void handle_state_pa_synced(const struct zbus_channel* channel)
{
  /*
   * Pseudocode:
   *
   * if channel is le_audio_chan:
   *   event = read le_audio_chan
   *   if event contains a valid selected BASE configuration:
   *     result = audio_streaming_actions_start_bis_sync()
   *     if result succeeds:
   *       set_state(BIS_SYNCING)
   *     else:
   *       result = audio_streaming_actions_restart_scan()
   *       if result succeeds:
   *         set_state(SCANNING)
   *       else:
   *         set_state(ERROR)
   *   return
   *
   * if channel is bt_mgmt_chan:
   *   event = read bt_mgmt_chan
   *   if event is PA_SYNC_LOST:
   *     result = audio_streaming_actions_restart_scan()
   *     if result succeeds:
   *       set_state(SCANNING)
   *     else:
   *       set_state(ERROR)
   *   else if event represents a Bluetooth failure:
   *     set_state(ERROR)
   *   return
   *
   * if channel is audio_streaming_cmd_chan:
   *   command = read audio_streaming_cmd_chan
   *   if command is STOP:
   *     result = audio_streaming_actions_stop()
   *     if result succeeds:
   *       set_state(IDLE)
   *     else:
   *       set_state(ERROR)
   *   else:
   *     log that every other command is deferred or invalid for the PoC
   */
  ARG_UNUSED(channel);
}

static void handle_state_bis_syncing(const struct zbus_channel* channel)
{
  /*
   * Pseudocode:
   *
   * if channel is le_audio_chan:
   *   event = read le_audio_chan
   *
   *   if event is BIS_STARTED:
   *     set_state(STREAMING)
   *     return
   *
   *   if event is BIS_SYNC_FAILED or reports no valid configuration:
   *     result = audio_streaming_actions_restart_scan()
   *     if result succeeds:
   *       set_state(SCANNING)
   *     else:
   *       set_state(ERROR)
   *   else if event represents a Bluetooth failure:
   *     set_state(ERROR)
   *   return
   *
   * if channel is bt_mgmt_chan:
   *   event = read bt_mgmt_chan
   *   if event is PA_SYNC_LOST:
   *     result = audio_streaming_actions_restart_scan()
   *     if result succeeds:
   *       set_state(SCANNING)
   *     else:
   *       set_state(ERROR)
   *   else if event represents a Bluetooth failure:
   *     set_state(ERROR)
   *   return
   *
   * if channel is audio_streaming_cmd_chan:
   *   command = read audio_streaming_cmd_chan
   *   if command is STOP:
   *     result = audio_streaming_actions_stop()
   *     if result succeeds:
   *       set_state(IDLE)
   *     else:
   *       set_state(ERROR)
   *   else:
   *     log that every other command is deferred or invalid for the PoC
   */
  ARG_UNUSED(channel);
}

static void handle_state_streaming(const struct zbus_channel* channel)
{
  /*
   * Pseudocode:
   *
   * if channel is le_audio_chan:
   *   event = read le_audio_chan
   *   if event is BIS_STOPPED or synchronization is lost:
   *     result = audio_streaming_actions_restart_scan()
   *       this action stops ISO forwarding, cleans up synchronization, and restarts scanning
   *     if result succeeds:
   *       set_state(SCANNING)
   *       the Codec Controller observes this state and falls back to local audio
   *     else:
   *       set_state(ERROR)
   *   else if event represents a Bluetooth failure:
   *     set_state(ERROR)
   *   return
   *
   * if channel is bt_mgmt_chan:
   *   event = read bt_mgmt_chan
   *   if event is PA_SYNC_LOST:
   *     result = audio_streaming_actions_restart_scan()
   *     if result succeeds:
   *       set_state(SCANNING)
   *       the Codec Controller observes this state and falls back to local audio
   *     else:
   *       set_state(ERROR)
   *   else if event represents a Bluetooth failure:
   *     set_state(ERROR)
   *   return
   *
   * if channel is audio_streaming_cmd_chan:
   *   command = read audio_streaming_cmd_chan
   *   if command is STOP:
   *     result = audio_streaming_actions_stop()
   *     if result succeeds:
   *       set_state(IDLE)
   *     else:
   *       set_state(ERROR)
   *   else:
   *     log that every other command is deferred or invalid for the PoC
   */
  ARG_UNUSED(channel);
}

static void handle_state_recovering(const struct zbus_channel* channel)
{
  /*
   * Pseudocode:
   *
   * RECOVERING is not entered by the initial PoC
   * recoverable synchronization loss performs one immediate cleanup and scan restart
   * log the unexpected notification and leave the state unchanged
   */
  ARG_UNUSED(channel);
}

static void handle_state_error(const struct zbus_channel* channel)
{
  /*
   * Pseudocode:
   *
   * retain Bluetooth and synchronization diagnostics
   * log that the PoC uses fail-stop behavior and requires a device reboot
   * ignore every notification; RESET and recovery are deferred
   */
  ARG_UNUSED(channel);
}

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
