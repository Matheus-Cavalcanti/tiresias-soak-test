#include "audio_control.h"

#include "adau_1787_IC_1_SIGMA_PARAM.h"
#include "audio_datapath.h"
#include "audio_system.h"
#include "broadcast_sink.h"
#include "adau1787.h"
#include "macros_common.h"
#include "streamctrl.h"
#include "zbus_common.h"

#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/zbus/zbus.h>

LOG_MODULE_REGISTER(audio_control_module, LOG_LEVEL_INF);

#define AUDIO_CONTROL_THREAD_STACK_SIZE 8192
#define AUDIO_CONTROL_THREAD_PRIORITY   3
#define AUDIO_CONTROL_ZBUS_TIMEOUT_MS   100

#define SWITCH_ADDRESS MOD_TROCA_STEREOSWSLEW_ADDR

ZBUS_SUBSCRIBER_DEFINE(audio_control_sub, 8);

ZBUS_CHAN_DECLARE(le_audio_chan, led_chan);

ZBUS_CHAN_DEFINE(audio_state_chan, audio_state_chan_msg, NULL, NULL, ZBUS_OBSERVERS_EMPTY,
		 ZBUS_MSG_INIT(.state = AUDIO_STATE_OFF));

ZBUS_CHAN_DEFINE(audio_cmd_chan, audio_cmd_chan_msg, NULL, NULL,
		 ZBUS_OBSERVERS(audio_control_sub), ZBUS_MSG_INIT(.cmd = AUDIO_CMD_INIT));

static audio_state current_state = AUDIO_STATE_OFF;
static enum stream_state stream_state = STATE_PAUSED;
static bool le_audio_observer_registered;

static uint32_t param_word_to_u32(const param_word_t param_word)
{
	return ((uint32_t)param_word[0] << 24) | ((uint32_t)param_word[1] << 16) |
	       ((uint32_t)param_word[2] << 8) | param_word[3];
}

static void u32_to_param_word(uint32_t value, param_word_t param_word)
{
	param_word[0] = (value >> 24) & 0xFF;
	param_word[1] = (value >> 16) & 0xFF;
	param_word[2] = (value >> 8) & 0xFF;
	param_word[3] = value & 0xFF;
}

static const char *stereo_switch_value_label(uint32_t value)
{
	switch (value) {
	case 0:
		return "From BLE";
	case 1:
		return "Passtrough";
	default:
		return "Unknown";
	}
}

static void set_audio_state(audio_state state)
{
	audio_state_chan_msg msg = {
		.state = state,
	};
	int ret;

	if (current_state == state) {
		return;
	}

	current_state = state;
	stream_state = (state == AUDIO_STATE_BROADCAST_STREAMING) ? STATE_STREAMING : STATE_PAUSED;

	ret = zbus_chan_pub(&audio_state_chan, &msg, K_MSEC(AUDIO_CONTROL_ZBUS_TIMEOUT_MS));
	if (ret) {
		LOG_ERR("Failed to publish audio state: %d", ret);
	}
}

static int le_audio_observer_register(void)
{
	int ret;

	if (le_audio_observer_registered) {
		return 0;
	}

	ret = zbus_chan_add_obs(&le_audio_chan, &audio_control_sub,
				K_MSEC(AUDIO_CONTROL_ZBUS_TIMEOUT_MS));
	if (ret) {
		return ret;
	}

	le_audio_observer_registered = true;
	return 0;
}

static int codec_switch_toggle(void)
{
	param_word_t codec_param;
	uint32_t codec_param_value;
	uint32_t toggled_codec_param_value;
	int ret;

	ret = adau1787_read(SWITCH_ADDRESS, codec_param, sizeof(codec_param));
	if (ret) {
		LOG_ERR("Failed to read codec parameter at 0x%04X: %d",
			SWITCH_ADDRESS, ret);
		return ret;
	}

	codec_param_value = param_word_to_u32(codec_param);
	toggled_codec_param_value = (codec_param_value == 0U) ? 1U : 0U;
	u32_to_param_word(toggled_codec_param_value, codec_param);

	ret = adau1787_write(SWITCH_ADDRESS, codec_param, sizeof(codec_param));
	if (ret) {
		LOG_ERR("Failed to write codec parameter at 0x%04X: %d",
			SWITCH_ADDRESS, ret);
		return ret;
	}

	ret = adau1787_read(SWITCH_ADDRESS, codec_param, sizeof(codec_param));
	if (ret) {
		LOG_ERR("Failed to read toggled codec parameter at 0x%04X: %d",
			SWITCH_ADDRESS, ret);
		return ret;
	}

	codec_param_value = param_word_to_u32(codec_param);
	LOG_INF("Stereo Switch Nx2 at 0x%04X toggled to %u (%s), raw=0x%08X",
		SWITCH_ADDRESS, codec_param_value,
		stereo_switch_value_label(codec_param_value), codec_param_value);

  struct led_chan_msg_t led_msg;
  led_msg.led = LED_2;
  if (codec_param_value == 0U) {
    led_msg.cmd = TURN_ON;
  } else {
    led_msg.cmd = BLINK;
  }
  ret = zbus_chan_pub(&led_chan, &led_msg, K_MSEC(100));
  if (ret) {
    LOG_ERR("Failed to send LED command: %d", ret);
  }
    
	return 0;
}

static void handle_audio_init(void)
{
	int ret;

	if (current_state != AUDIO_STATE_OFF) {
		return;
	}

	set_audio_state(AUDIO_STATE_INITIALIZING);

	ret = le_audio_observer_register();
	if (ret) {
		LOG_ERR("Failed to observe LE Audio events: %d", ret);
		set_audio_state(AUDIO_STATE_ERROR);
		return;
	}

	ret = audio_system_init();
	if (ret) {
		LOG_ERR("Failed to initialize audio system: %d", ret);
		set_audio_state(AUDIO_STATE_ERROR);
		return;
	}

	set_audio_state(AUDIO_STATE_STANDARD);
}

static void handle_audio_cmd(audio_cmd cmd)
{
	int ret;

	switch (cmd) {
	case AUDIO_CMD_INIT:
		handle_audio_init();
		break;
	case AUDIO_CMD_CODEC_SWITCH:
		ret = codec_switch_toggle();
		if (ret) {
			set_audio_state(AUDIO_STATE_ERROR);
		}
		break;
	default:
		LOG_WRN("Unhandled audio command: %d", cmd);
		break;
	}
}

static void handle_le_audio_config_received(void)
{
	uint32_t pres_delay_us;
	uint32_t bitrate_bps;
	uint32_t sampling_rate_hz;
	int ret;

	ret = broadcast_sink_config_get(&bitrate_bps, &sampling_rate_hz, &pres_delay_us);
	if (ret) {
		LOG_WRN("Failed to get broadcast sink config: %d", ret);
		return;
	}

	LOG_DBG("Broadcast sink sampling rate: %d Hz", sampling_rate_hz);
	LOG_DBG("Broadcast sink bitrate: %d bps", bitrate_bps);

	ret = audio_system_config_set(VALUE_NOT_SET, VALUE_NOT_SET, sampling_rate_hz);
	if (ret) {
		LOG_ERR("Failed to set audio system config: %d", ret);
		set_audio_state(AUDIO_STATE_ERROR);
		return;
	}

	ret = audio_datapath_pres_delay_us_set(pres_delay_us);
	if (ret) {
		LOG_ERR("Failed to set presentation delay: %d", ret);
		set_audio_state(AUDIO_STATE_ERROR);
		return;
	}

	LOG_INF("Presentation delay %d us is set", pres_delay_us);
}

static void handle_le_audio_event(const struct le_audio_msg *msg)
{
	switch (msg->event) {
	case LE_AUDIO_EVT_CONFIG_RECEIVED:
		handle_le_audio_config_received();
		break;
	case LE_AUDIO_EVT_STREAMING:
		if (current_state != AUDIO_STATE_BROADCAST_STREAMING) {
			audio_system_start();
			set_audio_state(AUDIO_STATE_BROADCAST_STREAMING);
		}
		break;
	case LE_AUDIO_EVT_NOT_STREAMING:
	case LE_AUDIO_EVT_SYNC_LOST:
		if (current_state == AUDIO_STATE_BROADCAST_STREAMING) {
			audio_system_stop();
		}
		set_audio_state(AUDIO_STATE_STANDARD);
		break;
	case LE_AUDIO_EVT_STREAM_SENT:
		break;
	case LE_AUDIO_EVT_NO_VALID_CFG:
		LOG_WRN("No valid broadcast sink configuration found");
		set_audio_state(AUDIO_STATE_STANDARD);
		break;
	default:
		LOG_WRN("Unexpected LE Audio event: %d", msg->event);
		break;
	}
}

static void audio_control_thread(void)
{
	struct zbus_channel *chan;
	int ret;

	LOG_INF("Audio control thread started");

	while (1) {
		ret = zbus_sub_wait(&audio_control_sub, &chan, K_FOREVER);
		if (ret) {
			LOG_ERR("Failed to wait for audio event: %d", ret);
			continue;
		}

		if (chan == &audio_cmd_chan) {
			audio_cmd_chan_msg msg;

			ret = zbus_chan_read(chan, &msg, K_MSEC(AUDIO_CONTROL_ZBUS_TIMEOUT_MS));
			if (ret) {
				LOG_ERR("Failed to read audio command: %d", ret);
				continue;
			}

			handle_audio_cmd(msg.cmd);
		} else if (chan == &le_audio_chan) {
			struct le_audio_msg msg;

			ret = zbus_chan_read(chan, &msg, K_MSEC(AUDIO_CONTROL_ZBUS_TIMEOUT_MS));
			if (ret) {
				LOG_ERR("Failed to read LE Audio event: %d", ret);
				continue;
			}

			handle_le_audio_event(&msg);
		}
	}
}

uint8_t stream_state_get(void)
{
	return stream_state;
}

void streamctrl_send(void const *const data, size_t size, uint8_t num_ch)
{
	ARG_UNUSED(data);
	ARG_UNUSED(size);
	ARG_UNUSED(num_ch);

	LOG_WRN("Sending is not possible for broadcast sink");
}

K_THREAD_DEFINE(audio_control_thread_id, AUDIO_CONTROL_THREAD_STACK_SIZE, audio_control_thread, NULL,
		NULL, NULL, AUDIO_CONTROL_THREAD_PRIORITY, 0, 0);
