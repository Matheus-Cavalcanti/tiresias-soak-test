/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef _ZBUS_COMMON_H_
#define _ZBUS_COMMON_H_

#include <zephyr/bluetooth/audio/audio.h>

#include "le_audio.h"

#define ZBUS_READ_TIMEOUT_MS	K_MSEC(100)
#define ZBUS_ADD_OBS_TIMEOUT_MS K_MSEC(200)

/***** Messages for zbus ******/

/* === Button === */
typedef enum btn_event_t {
	BUTTON_1_PRESSED,
} btn_event_t;
typedef struct btn_chan_msg_t {
	enum btn_event_t event;
} btn_chan_msg_t;

/* === Device Controller subsystem === */
typedef enum device_controller_cmd {
  DEVICE_CONTROLLER_CMD_START,
  DEVICE_CONTROLLER_CMD_LOW_POWER,
  DEVICE_CONTROLLER_CMD_WAKE,
  DEVICE_CONTROLLER_CMD_POWER_OFF,
  DEVICE_CONTROLLER_CMD_RECOVER,
} device_controller_cmd;

typedef struct device_controller_cmd_chan_msg {
  enum device_controller_cmd cmd;
} device_controller_cmd_chan_msg;

typedef enum device_controller_state {
  DEVICE_CONTROLLER_STATE_OFF,
  DEVICE_CONTROLLER_STATE_INITIALIZING,
  DEVICE_CONTROLLER_STATE_OPERATIONAL,
  DEVICE_CONTROLLER_STATE_LOW_POWER,
  DEVICE_CONTROLLER_STATE_FAULT,
} device_controller_state;

typedef struct device_controller_state_chan_msg {
  enum device_controller_state state;
} device_controller_state_chan_msg;

/* === Codec Controller subsystem === */
typedef enum codec_controller_cmd {
  CODEC_CONTROLLER_CMD_INITIALIZE,
  CODEC_CONTROLLER_CMD_SELECT_LOCAL,
  CODEC_CONTROLLER_CMD_SELECT_BROADCAST,
  CODEC_CONTROLLER_CMD_POWER_DOWN,
  CODEC_CONTROLLER_CMD_RESET,
} codec_controller_cmd;

typedef struct codec_controller_cmd_chan_msg {
  enum codec_controller_cmd cmd;
} codec_controller_cmd_chan_msg;

typedef enum codec_controller_state {
  CODEC_CONTROLLER_STATE_OFF,
  CODEC_CONTROLLER_STATE_INITIALIZING,
  CODEC_CONTROLLER_STATE_LOCAL_ONLY,
  CODEC_CONTROLLER_STATE_BROADCAST_ONLY,
  CODEC_CONTROLLER_STATE_ERROR,
} codec_controller_state;

typedef struct codec_controller_state_chan_msg {
  enum codec_controller_state state;
} codec_controller_state_chan_msg;

typedef enum codec_controller_result {
  CODEC_CONTROLLER_RESULT_COMMAND_REJECTED,
  CODEC_CONTROLLER_RESULT_OPERATION_FAILED,
} codec_controller_result;

typedef struct codec_controller_result_chan_msg {
  enum codec_controller_cmd cmd;
  enum codec_controller_result result;
  int error;
} codec_controller_result_chan_msg;

/* === Control Link subsystem === */
typedef enum control_link_cmd {
  CONTROL_LINK_CMD_ENABLE_CONTROL,
  CONTROL_LINK_CMD_DISABLE_CONTROL,
  CONTROL_LINK_CMD_RESET,
} control_link_cmd;

typedef struct control_link_cmd_chan_msg {
  enum control_link_cmd cmd;
} control_link_cmd_chan_msg;

typedef enum control_link_state {
  CONTROL_LINK_STATE_DISABLED,
  CONTROL_LINK_STATE_ADVERTISING,
  CONTROL_LINK_STATE_CONNECTED,
  CONTROL_LINK_STATE_ERROR,
} control_link_state;

typedef struct control_link_state_chan_msg {
  enum control_link_state state;
} control_link_state_chan_msg;

typedef enum control_link_result {
  CONTROL_LINK_RESULT_COMMAND_REJECTED,
  CONTROL_LINK_RESULT_OPERATION_FAILED,
} control_link_result;

typedef struct control_link_event_chan_msg {
  enum control_link_cmd cmd;
  enum control_link_result result;
  int error;
} control_link_event_chan_msg;

/* === Audio Streaming subsystem === */
typedef enum audio_streaming_cmd {
  AUDIO_STREAMING_CMD_ENABLE_RECEIVER,
  AUDIO_STREAMING_CMD_START_SCAN,
  AUDIO_STREAMING_CMD_STOP_SCAN,
  AUDIO_STREAMING_CMD_STOP,
  AUDIO_STREAMING_CMD_DISABLE_RECEIVER,
  AUDIO_STREAMING_CMD_RESET,
} audio_streaming_cmd;

typedef struct audio_streaming_cmd_chan_msg {
  enum audio_streaming_cmd cmd;
} audio_streaming_cmd_chan_msg;

typedef enum audio_streaming_state {
  AUDIO_STREAMING_STATE_DISABLED,
  AUDIO_STREAMING_STATE_IDLE,
  AUDIO_STREAMING_STATE_SCANNING,
  AUDIO_STREAMING_STATE_PA_SYNCED,
  AUDIO_STREAMING_STATE_BIS_SYNCING,
  AUDIO_STREAMING_STATE_STREAMING,
  AUDIO_STREAMING_STATE_RECOVERING,
  AUDIO_STREAMING_STATE_ERROR,
} audio_streaming_state;

typedef struct audio_streaming_state_chan_msg {
  enum audio_streaming_state state;
} audio_streaming_state_chan_msg;

typedef enum audio_streaming_result {
  AUDIO_STREAMING_RESULT_COMMAND_REJECTED,
  AUDIO_STREAMING_RESULT_OPERATION_FAILED,
} audio_streaming_result;

typedef struct audio_streaming_result_chan_msg {
  enum audio_streaming_cmd cmd;
  enum audio_streaming_result result;
  int error;
} audio_streaming_result_chan_msg;

/* === LED === */
typedef enum board_led_t {
  LED_1,
  LED_2,
  LED_3,
} board_led_t;
typedef enum led_cmd_t {
  TURN_ON,
  TURN_OFF,
  TOGGLE,
  BLINK,
} led_cmd_t;
typedef struct led_chan_msg_t {
  enum board_led_t led;
  enum led_cmd_t cmd;
} led_chan_msg_t;

enum le_audio_evt_type {
	LE_AUDIO_EVT_CONFIG_RECEIVED = 1,
	LE_AUDIO_EVT_PRES_DELAY_SET,
	LE_AUDIO_EVT_STREAMING,
	LE_AUDIO_EVT_NOT_STREAMING,
	LE_AUDIO_EVT_STREAM_SENT,
	LE_AUDIO_EVT_SYNC_LOST,
	LE_AUDIO_EVT_NO_VALID_CFG,
	LE_AUDIO_EVT_COORD_SET_DISCOVERED,
};

struct le_audio_msg {
	enum le_audio_evt_type event;
	struct bt_conn *conn;
	struct bt_le_per_adv_sync *pa_sync;
	enum bt_audio_dir dir;
	uint8_t set_size;
	uint8_t const *sirk;
	struct stream_index idx;
};

/**
 * tx_sync_ts_us	The timestamp from get_tx_sync.
 * curr_ts_us		The current time. This must be in the controller frame of reference.
 */
struct sdu_ref_msg {
	uint32_t tx_sync_ts_us;
	uint32_t curr_ts_us;
	bool adjust;
};

enum bt_mgmt_evt_type {
	BT_MGMT_EXT_ADV_WITH_PA_READY = 1,
	BT_MGMT_CONNECTED,
	BT_MGMT_SECURITY_CHANGED,
	BT_MGMT_PA_SYNCED,
	BT_MGMT_PA_SYNC_LOST,
	BT_MGMT_DISCONNECTED,
	BT_MGMT_BROADCAST_SINK_DISABLE,
	BT_MGMT_BROADCAST_CODE_RECEIVED,
};

struct bt_mgmt_msg {
	enum bt_mgmt_evt_type event;
	struct bt_conn *conn;
	uint8_t index;
	struct bt_le_ext_adv *ext_adv;
	struct bt_le_per_adv_sync *pa_sync;
	uint32_t broadcast_id;
	uint8_t pa_sync_term_reason;
};

enum volume_evt_type {
	VOLUME_UP = 1,
	VOLUME_DOWN,
	VOLUME_SET,
	VOLUME_MUTE,
	VOLUME_UNMUTE,
};

struct volume_msg {
	enum volume_evt_type event;
	uint8_t volume;
};

enum content_control_evt_type {
	MEDIA_START = 1,
	MEDIA_STOP,
};

struct content_control_msg {
	enum content_control_evt_type event;
};

#endif /* _ZBUS_COMMON_H_ */
