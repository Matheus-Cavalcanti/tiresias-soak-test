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

/*
 * TODO: After the initial control flow is validated, add per-destination
 * outstanding-command tracking, result correlation, stale-result detection,
 * completion deadlines, bounded retries, and escalation policies.
 */

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
   * When device is off, ignore the channel and start the
   * initialization sequence.
   *
   * set_state(INITIALIZING)
   * publish CODEC_CONTROLLER_CMD_INITIALIZE to codec_controller_cmd_chan
   * publish AUDIO_STREAMING_CMD_ENABLE_RECEIVER to audio_streaming_cmd_chan
   */
  ARG_UNUSED(channel);
}

static void handle_state_initializing(const struct zbus_channel* channel)
{
  /*
   * Pseudocode:
   *
   * when any startup state notification arrives:
   * codec_state = read codec_controller_state_chan
   * streaming_state = read audio_streaming_state_chan
   *
   * if either is ERROR:
   *   set_state(FAULT)
   * else if codec is LOCAL_ONLY and streaming discovery is active:
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
   *   log that LOW_POWER, WAKE, POWER_OFF, and RECOVER are deferred
   *   return
   *
   * if channel is button_chan:
   *   button_event = read button_chan
   *   if codec_state is LOCAL_ONLY:
   *     if streaming_state is STREAMING:
   *       publish SELECT_BROADCAST to codec_controller_cmd_chan
   *     else:
   *       log that broadcast audio is not available yet
   *     return
   *   if codec_state is BROADCAST_ONLY:
   *     publish SELECT_LOCAL to codec_controller_cmd_chan
   *     return
   *
   * if channel is codec_controller_state_chan:
   *   codec_state = read codec_controller_state_chan
   *   cache codec_state
   *   if codec_state is ERROR:
   *     set_state(FAULT)
   *     return
   *   if codec_state is LOCAL_ONLY or BROADCAST_ONLY:
   *     publish the corresponding LED Indicator command
   *   return
   *
   * if channel is audio_streaming_state_chan:
   *   streaming_state = read audio_streaming_state_chan
   *   cache streaming_state
   *   if streaming_state is ERROR:
   *     set_state(FAULT)
   *   return
   *
   * ignore Control Link and result/event notifications because they are not used
   * by the PoC
   */
  ARG_UNUSED(channel);
}

static void handle_state_low_power(const struct zbus_channel* channel)
{
  /*
   * Pseudocode:
   *
   * LOW_POWER is not entered by the initial PoC
   * log the unexpected notification and leave the state unchanged
   */
  ARG_UNUSED(channel);
}

static void handle_state_fault(const struct zbus_channel* channel)
{
  /*
   * Pseudocode:
   *
   * retain the latest subsystem diagnostics
   * log that the PoC uses fail-stop behavior and requires a device reboot
   * ignore every notification; RECOVER and POWER_OFF are deferred
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
