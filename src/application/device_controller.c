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

static void device_controller_state_off_handle(const struct zbus_channel* channel)
{
  ARG_UNUSED(channel);
}

static void device_controller_state_initializing_handle(const struct zbus_channel* channel)
{
  ARG_UNUSED(channel);
}

static void device_controller_state_operational_handle(const struct zbus_channel* channel)
{
  ARG_UNUSED(channel);
}

static void device_controller_state_low_power_handle(const struct zbus_channel* channel)
{
  ARG_UNUSED(channel);
}

static void device_controller_state_fault_handle(const struct zbus_channel* channel)
{
  ARG_UNUSED(channel);
}

static void device_controller_state_machine(const struct zbus_channel* channel)
{
  switch (current_state) {
  case DEVICE_CONTROLLER_STATE_OFF:
    device_controller_state_off_handle(channel);
    break;
  case DEVICE_CONTROLLER_STATE_INITIALIZING:
    device_controller_state_initializing_handle(channel);
    break;
  case DEVICE_CONTROLLER_STATE_OPERATIONAL:
    device_controller_state_operational_handle(channel);
    break;
  case DEVICE_CONTROLLER_STATE_LOW_POWER:
    device_controller_state_low_power_handle(channel);
    break;
  case DEVICE_CONTROLLER_STATE_FAULT:
    device_controller_state_fault_handle(channel);
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
