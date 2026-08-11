/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "button_handler.h"

#include "macros_common.h"
#include "zbus_common.h"

#include <errno.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>
#include <zephyr/zbus/zbus.h>

LOG_MODULE_REGISTER(button_handler, CONFIG_MODULE_BUTTON_HANDLER_LOG_LEVEL);

struct button_config {
	const char *name;
	const struct gpio_dt_spec spec;
	btn_event_t event;
};

ZBUS_CHAN_DEFINE(button_chan, btn_chan_msg_t, NULL, NULL, ZBUS_OBSERVERS_EMPTY,
		 ZBUS_MSG_INIT(0));

K_MSGQ_DEFINE(button_queue, sizeof(btn_chan_msg_t), 1, 4);

static bool debounce_is_ongoing;

BUILD_ASSERT(DT_NODE_HAS_PROP(DT_ALIAS(sw0), gpios), "sw0 alias missing gpios property");

static const struct button_config buttons[] = {
	{
		.name = "sw0",
		.spec = GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios),
		.event = BUTTON_1_PRESSED,
	},
};

static struct gpio_callback button_callbacks[ARRAY_SIZE(buttons)];

static void on_button_debounce_timeout(struct k_timer *timer)
{
	ARG_UNUSED(timer);

	debounce_is_ongoing = false;
}

K_TIMER_DEFINE(button_debounce_timer, on_button_debounce_timeout, NULL);

static const struct button_config *button_config_get(const struct device *port, uint32_t pins)
{
	for (size_t i = 0; i < ARRAY_SIZE(buttons); i++) {
		if (port == buttons[i].spec.port && (pins & BIT(buttons[i].spec.pin)) != 0) {
			return &buttons[i];
		}
	}

	return NULL;
}

static void button_isr(const struct device *port, struct gpio_callback *cb, uint32_t pins)
{
	const struct button_config *button;
	btn_chan_msg_t msg;
	int ret;

	ARG_UNUSED(cb);

	if (debounce_is_ongoing) {
		LOG_DBG("Button debounce in action");
		return;
	}

	button = button_config_get(port, pins);
	if (button == NULL) {
		LOG_WRN("Button interrupt from unknown pin mask: 0x%x", pins);
		return;
	}

	msg.event = button->event;

	ret = k_msgq_put(&button_queue, &msg, K_NO_WAIT);
	if (ret == -EAGAIN) {
		LOG_WRN("Button message queue full");
		return;
	}
	ERR_CHK(ret);

	debounce_is_ongoing = true;
	k_timer_start(&button_debounce_timer, K_MSEC(CONFIG_BUTTON_DEBOUNCE_MS), K_NO_WAIT);
}

static void button_publish_thread(void *arg1, void *arg2, void *arg3)
{
	btn_chan_msg_t msg;
	int ret;

	ARG_UNUSED(arg1);
	ARG_UNUSED(arg2);
	ARG_UNUSED(arg3);

	while (1) {
		k_msgq_get(&button_queue, &msg, K_FOREVER);

		ret = zbus_chan_pub(&button_chan, &msg, K_NO_WAIT);
		if (ret) {
			LOG_ERR("Failed to publish button event: %d", ret);
		}
	}
}

K_THREAD_DEFINE(button_publish, CONFIG_BUTTON_PUBLISH_STACK_SIZE, button_publish_thread, NULL, NULL, NULL,
		K_PRIO_PREEMPT(CONFIG_BUTTON_PUBLISH_THREAD_PRIO), 0, 0);

int button_pressed(gpio_pin_t button_pin, bool *pressed)
{
	if (pressed == NULL) {
		return -EINVAL;
	}

	for (size_t i = 0; i < ARRAY_SIZE(buttons); i++) {
		if (button_pin != buttons[i].spec.pin) {
			continue;
		}

		if (!gpio_is_ready_dt(&buttons[i].spec)) {
			return -ENODEV;
		}

		int ret = gpio_pin_get_dt(&buttons[i].spec);
		if (ret < 0) {
			return ret;
		}

		*pressed = ret == 1;
		return 0;
	}

	*pressed = false;
	return 0;
}

int button_handler_init(void)
{
	int ret;

	if (ARRAY_SIZE(buttons) == 0) {
		LOG_WRN("No buttons assigned");
		return -EINVAL;
	}

	for (size_t i = 0; i < ARRAY_SIZE(buttons); i++) {
		if (!gpio_is_ready_dt(&buttons[i].spec)) {
			LOG_ERR("Button GPIO controller %s is not ready", buttons[i].name);
			return -ENODEV;
		}

		ret = gpio_pin_configure_dt(&buttons[i].spec, GPIO_INPUT);
		if (ret) {
			return ret;
		}

		gpio_init_callback(&button_callbacks[i], button_isr, BIT(buttons[i].spec.pin));

		ret = gpio_add_callback(buttons[i].spec.port, &button_callbacks[i]);
		if (ret) {
			return ret;
		}

		ret = gpio_pin_interrupt_configure_dt(&buttons[i].spec, GPIO_INT_EDGE_TO_INACTIVE);
		if (ret) {
			return ret;
		}
	}

	return 0;
}
