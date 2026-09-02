/*
 * Copyright (c) 2026 Tiresias Firmware Team
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "control_link.h"

#include "control_link_actions.h"
#include "zbus_common.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/zbus/zbus.h>

#define CONTROL_LINK_THREAD_STACK_SIZE 4096
#define CONTROL_LINK_THREAD_PRIORITY 3
#define CONTROL_LINK_OBSERVER_PRIORITY 0
#define CONTROL_LINK_ZBUS_TIMEOUT_MS 100

LOG_MODULE_REGISTER(control_link, CONFIG_LOG_DEFAULT_LEVEL);

ZBUS_MSG_SUBSCRIBER_DEFINE(control_link_sub);

ZBUS_CHAN_DECLARE(bt_mgmt_chan);

ZBUS_CHAN_DEFINE(control_link_cmd_chan, control_link_cmd_chan_msg, NULL, NULL, ZBUS_OBSERVERS(control_link_sub),
    ZBUS_MSG_INIT(.cmd = CONTROL_LINK_CMD_ENABLE_CONTROL));

ZBUS_CHAN_DEFINE(control_link_state_chan, control_link_state_chan_msg, NULL, NULL, ZBUS_OBSERVERS_EMPTY,
    ZBUS_MSG_INIT(.state = CONTROL_LINK_STATE_DISABLED));

ZBUS_CHAN_DEFINE(control_link_event_chan, control_link_event_chan_msg, NULL, NULL, ZBUS_OBSERVERS_EMPTY,
    ZBUS_MSG_INIT(.cmd = CONTROL_LINK_CMD_ENABLE_CONTROL, .result = CONTROL_LINK_RESULT_COMMAND_REJECTED, .error = 0));

ZBUS_CHAN_ADD_OBS(bt_mgmt_chan, control_link_sub, CONTROL_LINK_OBSERVER_PRIORITY);

static control_link_state current_state = CONTROL_LINK_STATE_DISABLED;
static uint8_t control_connection_index = UINT8_MAX;
static bool advertising_start_pending;

union control_link_notification {
  control_link_cmd_chan_msg command;
  struct bt_mgmt_msg bt_mgmt;
};

static union control_link_notification current_notification;

static int set_state(control_link_state state)
{
  control_link_state_chan_msg msg = {
    .state = state,
  };
  int indicator_ret;
  int ret;

  if (current_state == state) {
    return 0;
  }

  LOG_INF("State transition: %d -> %d", current_state, state);
  current_state = state;

  ret = zbus_chan_pub(&control_link_state_chan, &msg, K_MSEC(CONTROL_LINK_ZBUS_TIMEOUT_MS));

  indicator_ret = control_link_actions_set_indicator(state);
  if (indicator_ret != 0) {
    LOG_ERR("Failed to update Control Link indicator: %d", indicator_ret);
  }

  return ret;
}

static void publish_result(control_link_cmd command, control_link_result result, int error)
{
  const control_link_event_chan_msg msg = {
    .cmd = command,
    .result = result,
    .error = error,
  };
  int ret = zbus_chan_pub(&control_link_event_chan, &msg, K_MSEC(CONTROL_LINK_ZBUS_TIMEOUT_MS));

  if (ret != 0) {
    LOG_ERR("Failed to publish Control Link result: %d", ret);
  }
}

static void enter_error(control_link_cmd command, int error)
{
  int ret;

  advertising_start_pending = false;
  control_connection_index = UINT8_MAX;
  publish_result(command, CONTROL_LINK_RESULT_OPERATION_FAILED, error);

  ret = set_state(CONTROL_LINK_STATE_ERROR);
  if (ret != 0) {
    LOG_ERR("Failed to publish ERROR state: %d", ret);
  }
}

static control_link_cmd read_command(void)
{
  return current_notification.command.cmd;
}

static void reject_command(control_link_cmd command)
{
  LOG_WRN("Command %d is not supported while in state %d", command, current_state);
  publish_result(command, CONTROL_LINK_RESULT_COMMAND_REJECTED, 0);
}

static void handle_bt_mgmt_event(void)
{
  const struct bt_mgmt_msg* msg = &current_notification.bt_mgmt;
  int ret;

  if ((msg->event == BT_MGMT_EXT_ADV_STARTED || msg->event == BT_MGMT_EXT_ADV_FAILED)
      && msg->index != CONTROL_LINK_ADV_SET_INDEX) {
    return;
  }

  switch (msg->event) {
  case BT_MGMT_EXT_ADV_STARTED:
    if (!advertising_start_pending) {
      return;
    }

    advertising_start_pending = false;
    ret = set_state(CONTROL_LINK_STATE_ADVERTISING);
    if (ret != 0) {
      LOG_ERR("Failed to publish ADVERTISING state: %d", ret);
      enter_error(CONTROL_LINK_CMD_ENABLE_CONTROL, ret);
    }
    break;

  case BT_MGMT_EXT_ADV_FAILED:
    if (advertising_start_pending) {
      LOG_ERR("Connectable advertising failed: %d", msg->error);
      enter_error(CONTROL_LINK_CMD_ENABLE_CONTROL, msg->error);
    }
    break;

  case BT_MGMT_CONNECTED:
    if (!msg->peripheral || (current_state != CONTROL_LINK_STATE_ADVERTISING && !advertising_start_pending)) {
      return;
    }

    advertising_start_pending = false;
    control_connection_index = msg->index;
    ret = set_state(CONTROL_LINK_STATE_CONNECTED);
    if (ret != 0) {
      LOG_ERR("Failed to publish CONNECTED state: %d", ret);
      enter_error(CONTROL_LINK_CMD_ENABLE_CONTROL, ret);
    }
    break;

  case BT_MGMT_DISCONNECTED:
    if (msg->index != control_connection_index) {
      return;
    }

    control_connection_index = UINT8_MAX;
    ret = control_link_actions_restart_advertising();
    if (ret != 0) {
      LOG_ERR("Failed to restart connectable advertising: %d", ret);
      enter_error(CONTROL_LINK_CMD_ENABLE_CONTROL, ret);
      return;
    }

    advertising_start_pending = true;
    break;

  default:
    break;
  }
}

static void handle_state_disabled(const struct zbus_channel* channel)
{
  control_link_cmd command;
  int ret;

  if (channel != &control_link_cmd_chan) {
    return;
  }

  command = read_command();
  if (command != CONTROL_LINK_CMD_ENABLE_CONTROL || advertising_start_pending) {
    reject_command(command);
    return;
  }

  ret = control_link_actions_enable();
  if (ret != 0) {
    LOG_ERR("Failed to enable Control Link: %d", ret);
    enter_error(command, ret);
    return;
  }

  advertising_start_pending = true;
}

static void handle_state_advertising(const struct zbus_channel* channel)
{
  control_link_cmd command;
  int ret;

  if (channel != &control_link_cmd_chan) {
    return;
  }

  command = read_command();
  if (command != CONTROL_LINK_CMD_DISABLE_CONTROL) {
    reject_command(command);
    return;
  }

  ret = control_link_actions_disable();
  if (ret != 0) {
    LOG_ERR("Failed to disable Control Link: %d", ret);
    enter_error(command, ret);
    return;
  }

  ret = set_state(CONTROL_LINK_STATE_DISABLED);
  if (ret != 0) {
    LOG_ERR("Failed to publish DISABLED state: %d", ret);
    enter_error(command, ret);
  }
}

static void handle_state_connected(const struct zbus_channel* channel)
{
  if (channel == &control_link_cmd_chan) {
    reject_command(read_command());
  }
}

static void handle_state_error(const struct zbus_channel* channel)
{
  if (channel == &control_link_cmd_chan) {
    reject_command(read_command());
  }
}

static void control_link_state_machine(const struct zbus_channel* channel)
{
  if (channel == &bt_mgmt_chan) {
    handle_bt_mgmt_event();
    return;
  }

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

static void control_link_thread(void* arg1, void* arg2, void* arg3)
{
  const struct zbus_channel* channel;

  ARG_UNUSED(arg1);
  ARG_UNUSED(arg2);
  ARG_UNUSED(arg3);

  while (1) {
    if (zbus_sub_wait_msg(&control_link_sub, &channel, &current_notification, K_FOREVER) != 0) {
      continue;
    }

    control_link_state_machine(channel);
  }
}

K_THREAD_DEFINE(control_link_thread_id, CONTROL_LINK_THREAD_STACK_SIZE, control_link_thread, NULL, NULL, NULL,
    CONTROL_LINK_THREAD_PRIORITY, 0, 0);
