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
   * if command is not ENABLE_RECEIVER:
   *   publish COMMAND_REJECTED on audio_streaming_result_chan
   *   return
   *
   * initialize the LE Audio receiver and BIS data path
   *
   * if initialization succeeds:
   *   set_state(IDLE)
   * else:
   *   publish OPERATION_FAILED on audio_streaming_result_chan
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
   *   start scanning for a suitable broadcast source
   *   if scanning starts successfully:
   *     set_state(SCANNING)
   *   else:
   *     publish OPERATION_FAILED on audio_streaming_result_chan
   *   return
   *
   * if command is DISABLE_RECEIVER:
   *   disable the LE Audio receiver
   *   if the operation succeeds:
   *     set_state(DISABLED)
   *   else:
   *     publish OPERATION_FAILED on audio_streaming_result_chan
   *     set_state(ERROR)
   *   return
   *
   * publish COMMAND_REJECTED on audio_streaming_result_chan
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
   *   if event represents a fatal Bluetooth failure:
   *     set_state(ERROR)
   *   return
   *
   * if channel is not audio_streaming_cmd_chan:
   *   ignore the notification
   *   return
   *
   * command = read audio_streaming_cmd_chan
   * if command is STOP_SCAN:
   *   stop scanning
   *   if the operation succeeds:
   *     set_state(IDLE)
   *   else:
   *     publish OPERATION_FAILED on audio_streaming_result_chan
   *     set_state(ERROR)
   *   return
   *
   * publish COMMAND_REJECTED on audio_streaming_result_chan
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
   *     begin BIG/BIS synchronization
   *     if synchronization starts successfully:
   *       set_state(BIS_SYNCING)
   *     else:
   *       publish OPERATION_FAILED on audio_streaming_result_chan
   *       set_state(RECOVERING)
   *   return
   *
   * if channel is bt_mgmt_chan:
   *   event = read bt_mgmt_chan
   *   if event is PA_SYNC_LOST:
   *     set_state(RECOVERING)
   *   else if event represents a fatal Bluetooth failure:
   *     set_state(ERROR)
   *   return
   *
   * if channel is audio_streaming_cmd_chan:
   *   command = read audio_streaming_cmd_chan
   *   if command is STOP:
   *     stop synchronization and clean up the selected source
   *     set_state(IDLE)
   *   else:
   *     publish COMMAND_REJECTED on audio_streaming_result_chan
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
   *     set_state(RECOVERING)
   *   else if event represents a fatal Bluetooth failure:
   *     set_state(ERROR)
   *   return
   *
   * if channel is bt_mgmt_chan:
   *   event = read bt_mgmt_chan
   *   if event is PA_SYNC_LOST:
   *     set_state(RECOVERING)
   *   else if event represents a fatal Bluetooth failure:
   *     set_state(ERROR)
   *   return
   *
   * if channel is audio_streaming_cmd_chan:
   *   command = read audio_streaming_cmd_chan
   *   if command is STOP:
   *     cancel synchronization and clean up the selected source
   *     set_state(IDLE)
   *   else:
   *     publish COMMAND_REJECTED on audio_streaming_result_chan
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
   *     stop forwarding received ISO data
   *     set_state(RECOVERING)
   *   else if event represents a fatal Bluetooth failure:
   *     set_state(ERROR)
   *   return
   *
   * if channel is bt_mgmt_chan:
   *   event = read bt_mgmt_chan
   *   if event is PA_SYNC_LOST:
   *     stop forwarding received ISO data
   *     set_state(RECOVERING)
   *   else if event represents a fatal Bluetooth failure:
   *     set_state(ERROR)
   *   return
   *
   * if channel is audio_streaming_cmd_chan:
   *   command = read audio_streaming_cmd_chan
   *   if command is STOP:
   *     stop streaming and clean up synchronization
   *     set_state(IDLE)
   *   else:
   *     publish COMMAND_REJECTED on audio_streaming_result_chan
   */
  ARG_UNUSED(channel);
}

static void handle_state_recovering(const struct zbus_channel* channel)
{
  /*
   * Pseudocode:
   *
   * if channel is audio_streaming_cmd_chan:
   *   command = read audio_streaming_cmd_chan
   *   if command is STOP:
   *     cancel retry and finish synchronization cleanup
   *     set_state(IDLE)
   *   else:
   *     publish COMMAND_REJECTED on audio_streaming_result_chan
   *   return
   *
   * if channel reports that cleanup is complete and retry is permitted:
   *   restart broadcast scanning
   *   if scanning starts successfully:
   *     set_state(SCANNING)
   *   else:
   *     publish OPERATION_FAILED on audio_streaming_result_chan
   *     set_state(ERROR)
   *   return
   *
   * if channel reports a fatal Bluetooth failure:
   *   set_state(ERROR)
   */
  ARG_UNUSED(channel);
}

static void handle_state_error(const struct zbus_channel* channel)
{
  /*
   * Pseudocode:
   *
   * if channel is not audio_streaming_cmd_chan:
   *   retain Bluetooth and synchronization diagnostics
   *   ignore the notification
   *   return
   *
   * command = read audio_streaming_cmd_chan
   * if command is not RESET:
   *   publish COMMAND_REJECTED on audio_streaming_result_chan
   *   return
   *
   * clean up and reset the LE Audio receiver
   *
   * if recovery succeeds:
   *   set_state(IDLE)
   * else:
   *   publish OPERATION_FAILED on audio_streaming_result_chan
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
