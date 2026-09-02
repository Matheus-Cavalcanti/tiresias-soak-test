#include <nrfx_clock.h>

#include "button_handler.h"
#include "channel_assignment.h"
#include "led.h"
#include "tiresias_dk.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(tiresias_dk, CONFIG_MODULE_NRF5340_AUDIO_DK_LOG_LEVEL);

static int channel_assign_check(void)
{
	return 0;
}

int tiresias_dk_init(void)
{
	int ret;

	ret = led_init();
	if (ret) {
		LOG_ERR("Failed to initialize LED module");
		return ret;
	}

	ret = button_handler_init();
	if (ret) {
		LOG_ERR("Failed to initialize button handler");
		return ret;
	}

	ret = channel_assign_check();
	if (ret) {
		LOG_ERR("Failed get channel assignment");
		return ret;
	}

	/* Use this to turn on 128 MHz clock for cpu_app */
	ret = nrfx_clock_divider_set(NRF_CLOCK_DOMAIN_HFCLK, NRF_CLOCK_HFCLK_DIV_1);
	ret -= NRFX_ERROR_BASE_NUM;
	if (ret) {
		return ret;
	}

	return 0;
}

int nrf5340_audio_dk_init(void)
{
	return tiresias_dk_init();
}
