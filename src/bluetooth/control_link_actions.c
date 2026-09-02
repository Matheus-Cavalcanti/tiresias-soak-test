/*
 * Copyright (c) 2026 Tiresias Firmware Team
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "control_link_actions.h"

#include "bt_mgmt.h"
#include "control_link.h"

#include <errno.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>
#include <zephyr/zbus/zbus.h>

#define CONTROL_LINK_ACTION_TIMEOUT_MS 100

BUILD_ASSERT(
    CONFIG_BT_EXT_ADV_MAX_ADV_SET > CONTROL_LINK_ADV_SET_INDEX, "Control Link requires one extended advertising set");
BUILD_ASSERT(IS_ENABLED(CONFIG_BT_EXT_ADV), "Control Link requires extended advertising");
BUILD_ASSERT(IS_ENABLED(CONFIG_BT_PERIPHERAL), "Control Link requires Bluetooth peripheral support");
BUILD_ASSERT(IS_ENABLED(CONFIG_BT_DIS), "Control Link requires the Device Information Service");

ZBUS_CHAN_DECLARE(led_chan);

static const struct bt_data control_link_advertising_data[] = {
  BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR),
  BT_DATA_BYTES(BT_DATA_UUID16_ALL, BT_UUID_16_ENCODE(BT_UUID_DIS_VAL)),
};

int control_link_actions_enable(void)
{
  int ret = bt_mgmt_init();

  if (ret != 0) {
    return ret;
  }

  return bt_mgmt_adv_start(CONTROL_LINK_ADV_SET_INDEX, control_link_advertising_data,
      ARRAY_SIZE(control_link_advertising_data), NULL, 0, true);
}

int control_link_actions_restart_advertising(void)
{
  return bt_mgmt_adv_start(CONTROL_LINK_ADV_SET_INDEX, NULL, 0, NULL, 0, true);
}

int control_link_actions_disable(void)
{
  int ret = bt_mgmt_ext_adv_stop(CONTROL_LINK_ADV_SET_INDEX);

  return ret == -EALREADY ? 0 : ret;
}

int control_link_actions_set_indicator(control_link_state state)
{
  led_cmd_t command;

  switch (state) {
  case CONTROL_LINK_STATE_ADVERTISING:
    command = BLINK;
    break;
  case CONTROL_LINK_STATE_CONNECTED:
    command = TURN_ON;
    break;
  case CONTROL_LINK_STATE_DISABLED:
  case CONTROL_LINK_STATE_ERROR:
    command = TURN_OFF;
    break;
  default:
    return -EINVAL;
  }

  const led_chan_msg_t msg = {
    .led = LED_1,
    .cmd = command,
  };

  return zbus_chan_pub(&led_chan, &msg, K_MSEC(CONTROL_LINK_ACTION_TIMEOUT_MS));
}
