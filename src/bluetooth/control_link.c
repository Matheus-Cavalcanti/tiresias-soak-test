/*
 * Copyright (c) 2026 Tiresias Firmware Team
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "control_link.h"

#include "zbus_common.h"

#include <zephyr/kernel.h>
#include <zephyr/zbus/zbus.h>

#define CONTROL_LINK_THREAD_STACK_SIZE 1024
#define CONTROL_LINK_THREAD_PRIORITY 3
#define CONTROL_LINK_SUBSCRIBER_QUEUE_SIZE 8
#define CONTROL_LINK_OBSERVER_PRIORITY 0
#define CONTROL_LINK_ZBUS_TIMEOUT_MS 100

ZBUS_SUBSCRIBER_DEFINE(control_link_sub, CONTROL_LINK_SUBSCRIBER_QUEUE_SIZE);

ZBUS_CHAN_DECLARE(bt_mgmt_chan);

ZBUS_CHAN_DEFINE(control_link_cmd_chan, control_link_cmd_chan_msg, NULL, NULL, ZBUS_OBSERVERS(control_link_sub),
    ZBUS_MSG_INIT(.cmd = CONTROL_LINK_CMD_ENABLE_CONTROL));

ZBUS_CHAN_DEFINE(control_link_state_chan, control_link_state_chan_msg, NULL, NULL, ZBUS_OBSERVERS_EMPTY,
    ZBUS_MSG_INIT(.state = CONTROL_LINK_STATE_DISABLED));

ZBUS_CHAN_DEFINE(control_link_event_chan, control_link_event_chan_msg, NULL, NULL, ZBUS_OBSERVERS_EMPTY,
    ZBUS_MSG_INIT(.cmd = CONTROL_LINK_CMD_ENABLE_CONTROL, .result = CONTROL_LINK_RESULT_COMMAND_REJECTED, .error = 0));

ZBUS_CHAN_ADD_OBS(bt_mgmt_chan, control_link_sub, CONTROL_LINK_OBSERVER_PRIORITY);

static control_link_state current_state = CONTROL_LINK_STATE_DISABLED;

static int __maybe_unused set_state(control_link_state state)
{
  control_link_state_chan_msg msg = {
    .state = state,
  };

  if (current_state == state) {
    return 0;
  }

  current_state = state;

  return zbus_chan_pub(&control_link_state_chan, &msg, K_MSEC(CONTROL_LINK_ZBUS_TIMEOUT_MS));
}

static void handle_state_disabled(const struct zbus_channel* channel)
{
  /*
   * Pseudocode:
   *
   * if channel is not control_link_cmd_chan:
   *   ignore the notification
   *   return
   *
   * command = read control_link_cmd_chan
   * if command is not ENABLE_CONTROL:
   *   publish COMMAND_REJECTED on control_link_event_chan
   *   return
   *
   * initialize the BLE control services
   * start connectable advertising
   *
   * if both operations succeed:
   *   set_state(ADVERTISING)
   * else:
   *   publish OPERATION_FAILED on control_link_event_chan
   *   set_state(ERROR)
   */
  ARG_UNUSED(channel);
}

static void handle_state_advertising(const struct zbus_channel* channel)
{
  /*
   * Pseudocode:
   *
   * if channel is bt_mgmt_chan:
   *   event = read bt_mgmt_chan
   *
   *   if event is BLE_CONNECTED:
   *     retain the connection used by the control interface
   *     set_state(CONNECTED)
   *     return
   *
   *   if event represents a fatal BLE failure:
   *     set_state(ERROR)
   *   return
   *
   * if channel is not control_link_cmd_chan:
   *   ignore the notification
   *   return
   *
   * command = read control_link_cmd_chan
   * if command is DISABLE_CONTROL:
   *   stop connectable advertising
   *   disable the BLE control services
   *
   *   if both operations succeed:
   *     set_state(DISABLED)
   *   else:
   *     publish OPERATION_FAILED on control_link_event_chan
   *     set_state(ERROR)
   *   return
   *
   * publish COMMAND_REJECTED on control_link_event_chan
   */
  ARG_UNUSED(channel);
}

static void handle_state_connected(const struct zbus_channel* channel)
{
  /*
   * Pseudocode:
   *
   * if channel is bt_mgmt_chan:
   *   event = read bt_mgmt_chan
   *
   *   if event is BLE_DISCONNECTED:
   *     release the control connection
   *     restart connectable advertising
   *
   *     if advertising starts successfully:
   *       set_state(ADVERTISING)
   *     else:
   *       set_state(ERROR)
   *     return
   *
   *   if event represents a fatal BLE failure:
   *     set_state(ERROR)
   *   return
   *
   * if channel is not control_link_cmd_chan:
   *   ignore the notification
   *   return
   *
   * command = read control_link_cmd_chan
   * if command is DISABLE_CONTROL:
   *   disconnect the control connection
   *   disable connectable advertising and the BLE control services
   *
   *   if every operation succeeds:
   *     set_state(DISABLED)
   *   else:
   *     publish OPERATION_FAILED on control_link_event_chan
   *     set_state(ERROR)
   *   return
   *
   * publish COMMAND_REJECTED on control_link_event_chan
   *
   * normalized GATT control requests are published to the Device Controller
   * command channel without changing Control Link state
   */
  ARG_UNUSED(channel);
}

static void handle_state_error(const struct zbus_channel* channel)
{
  /*
   * Pseudocode:
   *
   * if channel is not control_link_cmd_chan:
   *   retain BLE diagnostics
   *   ignore the notification
   *   return
   *
   * command = read control_link_cmd_chan
   * if command is not RESET:
   *   publish COMMAND_REJECTED on control_link_event_chan
   *   return
   *
   * clean up the connection, advertising, and control services
   * reset the Control Link resources
   *
   * if recovery succeeds:
   *   set_state(DISABLED)
   * else:
   *   publish OPERATION_FAILED on control_link_event_chan
   */
  ARG_UNUSED(channel);
}

static void control_link_state_machine(const struct zbus_channel* channel)
{
  switch (current_state) {
  case CONTROL_LINK_STATE_DISABLED:
    handle_state_disabled(channel);
    break;
  case CONTROL_LINK_STATE_ADVERTISING:
    handle_state_advertising(channel);
    break;
  case CONTROL_LINK_STATE_CONNECTED:
    handle_state_connected(channel);
    break;
  case CONTROL_LINK_STATE_ERROR:
    handle_state_error(channel);
    break;
  }
}

static void control_link_thread(void)
{
  const struct zbus_channel* channel;

  while (1) {
    if (zbus_sub_wait(&control_link_sub, &channel, K_FOREVER) != 0) {
      continue;
    }

    control_link_state_machine(channel);
  }
}

K_THREAD_DEFINE(control_link_thread_id, CONTROL_LINK_THREAD_STACK_SIZE, control_link_thread, NULL, NULL, NULL,
    CONTROL_LINK_THREAD_PRIORITY, 0, 0);
