/*
 * Copyright (c) 2026 Tiresias Firmware Team
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "device_controller.h"

#include "zbus_common.h"

#include <zephyr/kernel.h>
#include <zephyr/zbus/zbus.h>

#define DEVICE_CONTROLLER_THREAD_STACK_SIZE 1024
#define DEVICE_CONTROLLER_THREAD_PRIORITY 3
#define DEVICE_CONTROLLER_SUBSCRIBER_QUEUE_SIZE 8
#define DEVICE_CONTROLLER_OBSERVER_PRIORITY 0
#define DEVICE_CONTROLLER_ZBUS_TIMEOUT_MS 100

ZBUS_SUBSCRIBER_DEFINE(device_controller_sub, DEVICE_CONTROLLER_SUBSCRIBER_QUEUE_SIZE);

ZBUS_CHAN_DECLARE(button_chan);
ZBUS_CHAN_DECLARE(codec_controller_state_chan);
ZBUS_CHAN_DECLARE(codec_controller_result_chan);
ZBUS_CHAN_DECLARE(control_link_state_chan);
ZBUS_CHAN_DECLARE(control_link_event_chan);
ZBUS_CHAN_DECLARE(audio_streaming_state_chan);
ZBUS_CHAN_DECLARE(audio_streaming_result_chan);

ZBUS_CHAN_DEFINE(device_controller_cmd_chan, device_controller_cmd_chan_msg, NULL, NULL,
    ZBUS_OBSERVERS(device_controller_sub), ZBUS_MSG_INIT(.cmd = DEVICE_CONTROLLER_CMD_START));

ZBUS_CHAN_DEFINE(device_controller_state_chan, device_controller_state_chan_msg, NULL, NULL, ZBUS_OBSERVERS_EMPTY,
    ZBUS_MSG_INIT(.state = DEVICE_CONTROLLER_STATE_OFF));

ZBUS_CHAN_ADD_OBS(button_chan, device_controller_sub, DEVICE_CONTROLLER_OBSERVER_PRIORITY);
ZBUS_CHAN_ADD_OBS(codec_controller_state_chan, device_controller_sub, DEVICE_CONTROLLER_OBSERVER_PRIORITY);
ZBUS_CHAN_ADD_OBS(codec_controller_result_chan, device_controller_sub, DEVICE_CONTROLLER_OBSERVER_PRIORITY);
ZBUS_CHAN_ADD_OBS(control_link_state_chan, device_controller_sub, DEVICE_CONTROLLER_OBSERVER_PRIORITY);
ZBUS_CHAN_ADD_OBS(control_link_event_chan, device_controller_sub, DEVICE_CONTROLLER_OBSERVER_PRIORITY);
ZBUS_CHAN_ADD_OBS(audio_streaming_state_chan, device_controller_sub, DEVICE_CONTROLLER_OBSERVER_PRIORITY);
ZBUS_CHAN_ADD_OBS(audio_streaming_result_chan, device_controller_sub, DEVICE_CONTROLLER_OBSERVER_PRIORITY);

static device_controller_state current_state = DEVICE_CONTROLLER_STATE_OFF;

static int __maybe_unused set_state(device_controller_state state)
{
  device_controller_state_chan_msg msg = {
    .state = state,
  };

  if (current_state == state) {
    return 0;
  }

  current_state = state;

  return zbus_chan_pub(&device_controller_state_chan, &msg, K_MSEC(DEVICE_CONTROLLER_ZBUS_TIMEOUT_MS));
}

static void handle_state_off(const struct zbus_channel* channel)
{
  /*
   * Pseudocode:
   *
   * if channel is not device_controller_cmd_chan:
   *   ignore the notification
   *   return
   *
   * command = read device_controller_cmd_chan
   * if command is not START:
   *   log that the command is invalid while OFF
   *   return
   *
   * set_state(INITIALIZING)
   *
   * request the required subsystems to start in dependency order:
   *   publish INITIALIZE to codec_controller_cmd_chan
   *   publish ENABLE_CONTROL to control_link_cmd_chan
   *   publish ENABLE_RECEIVER to audio_streaming_cmd_chan
   *
   * mark one initialization command as outstanding for each subsystem
   * wait for subsystem state notifications to report completion
   */
  ARG_UNUSED(channel);
}

static void handle_state_initializing(const struct zbus_channel* channel)
{
  /*
   * Pseudocode:
   *
   * if channel is codec_controller_state_chan:
   *   codec_state = read codec_controller_state_chan
   *   update the cached codec readiness snapshot
   *   if codec_state is ERROR:
   *     set_state(FAULT)
   *     return
   *
   * if channel is control_link_state_chan:
   *   control_link_state = read control_link_state_chan
   *   update the cached Control Link readiness snapshot
   *   if control_link_state is ERROR:
   *     set_state(FAULT)
   *     return
   *
   * if channel is audio_streaming_state_chan:
   *   streaming_state = read audio_streaming_state_chan
   *   update the cached Audio Streaming readiness snapshot
   *   if streaming_state is ERROR:
   *     set_state(FAULT)
   *     return
   *
   * if channel is a subsystem result/event channel:
   *   result = read the triggering channel
   *   if an initialization command was rejected or failed:
   *     set_state(FAULT)
   *     return
   *
   * if channel is device_controller_cmd_chan:
   *   read the command
   *   reject it because no lifecycle command is valid while INITIALIZING
   *   return
   *
   * if every required subsystem reports its ready condition:
   *   publish START_AUDIO to codec_controller_cmd_chan
   *   publish START_SCAN to audio_streaming_cmd_chan
   *   set_state(OPERATIONAL)
   */
  ARG_UNUSED(channel);
}

static void handle_state_operational(const struct zbus_channel* channel)
{
  /*
   * Pseudocode:
   *
   * if channel is device_controller_cmd_chan:
   *   command = read device_controller_cmd_chan
   *
   *   if command is LOW_POWER:
   *     publish STOP_CODEC to codec_controller_cmd_chan
   *     publish STOP to audio_streaming_cmd_chan
   *     apply the configured Control Link low-power policy
   *     wait for the required completion state reports
   *     set_state(LOW_POWER)
   *     return
   *
   *   if command is POWER_OFF:
   *     publish STOP_CODEC and POWER_DOWN to codec_controller_cmd_chan in order
   *     publish STOP and DISABLE_RECEIVER to audio_streaming_cmd_chan in order
   *     publish DISABLE_CONTROL to control_link_cmd_chan
   *     wait for OFF and DISABLED state reports
   *     set_state(OFF)
   *     return
   *
   *   reject every other lifecycle command
   *   return
   *
   * if channel is button_chan:
   *   button_event = read button_chan
   *   translate the semantic button event into device policy
   *   publish an explicit SELECT_LOCAL or SELECT_BROADCAST command to
   *     codec_controller_cmd_chan
   *   wait for the Codec Controller state or result notification
   *   publish the corresponding LED Indicator command
   *   return
   *
   * if channel is any subsystem state channel:
   *   state = read the triggering state channel
   *   update the corresponding cached snapshot
   *   if a required subsystem reports ERROR:
   *     set_state(FAULT)
   *   return
   *
   * if channel is any subsystem result/event channel:
   *   result = read the triggering result/event channel
   *   clear the matching outstanding command
   *   retry, reject the originating policy request, or transition to FAULT
   *     according to whether the failure is recoverable
   */
  ARG_UNUSED(channel);
}

static void handle_state_low_power(const struct zbus_channel* channel)
{
  /*
   * Pseudocode:
   *
   * if channel is device_controller_cmd_chan:
   *   command = read device_controller_cmd_chan
   *
   *   if command is WAKE:
   *     restore the configured Control Link policy
   *     publish ENABLE_RECEIVER to audio_streaming_cmd_chan if it was disabled
   *     publish START_AUDIO to codec_controller_cmd_chan
   *     wait until every required subsystem reports its operational condition
   *     set_state(OPERATIONAL)
   *     return
   *
   *   if command is POWER_OFF:
   *     publish POWER_DOWN to codec_controller_cmd_chan
   *     publish DISABLE_RECEIVER to audio_streaming_cmd_chan
   *     publish DISABLE_CONTROL to control_link_cmd_chan
   *     wait for OFF and DISABLED state reports
   *     set_state(OFF)
   *     return
   *
   *   reject every other lifecycle command
   *   return
   *
   * if channel is any subsystem state channel:
   *   state = read the triggering state channel
   *   update the corresponding cached snapshot
   *   if a required subsystem reports an unrecoverable ERROR:
   *     set_state(FAULT)
   *   return
   *
   * if channel is any subsystem result/event channel:
   *   process the outcome of the outstanding suspend, wake, or shutdown command
   */
  ARG_UNUSED(channel);
}

static void handle_state_fault(const struct zbus_channel* channel)
{
  /*
   * Pseudocode:
   *
   * if channel is not device_controller_cmd_chan:
   *   retain subsystem diagnostics for recovery
   *   ignore the notification
   *   return
   *
   * command = read device_controller_cmd_chan
   *
   * if command is RECOVER:
   *   publish RESET to codec_controller_cmd_chan
   *   publish RESET to control_link_cmd_chan
   *   publish RESET to audio_streaming_cmd_chan
   *   clear cached readiness and outstanding-command tracking
   *   set_state(INITIALIZING)
   *   wait for subsystem state notifications to report recovery completion
   *   return
   *
   * if command is POWER_OFF:
   *   request best-effort shutdown of all subsystems
   *   set_state(OFF)
   *   return
   *
   * reject every other lifecycle command
   */
  ARG_UNUSED(channel);
}

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
  }
}

static void device_controller_thread(void)
{
  const struct zbus_channel* channel;

  while (1) {
    if (zbus_sub_wait(&device_controller_sub, &channel, K_FOREVER) != 0) {
      continue;
    }

    device_controller_state_machine(channel);
  }
}

K_THREAD_DEFINE(device_controller_thread_id, DEVICE_CONTROLLER_THREAD_STACK_SIZE, device_controller_thread, NULL, NULL,
    NULL, DEVICE_CONTROLLER_THREAD_PRIORITY, 0, 0);
