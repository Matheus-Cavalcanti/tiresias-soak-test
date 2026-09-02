/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef _BUTTON_HANDLER_H_
#define _BUTTON_HANDLER_H_

#include <stdbool.h>
#include <zephyr/drivers/gpio.h>

/** @brief Initialize the button handler.
 *
 * Configures the board button GPIOs and publishes button events on button_chan.
 *
 * @return 0 if successful.
 * @return -ENODEV	gpio driver not found
 */
int button_handler_init(void);

/** @brief Check button state.
 *
 * @param[in] button_pin GPIO pin for the configured button.
 * @param[out] button_pressed Button state. True if currently pressed, false otherwise.
 *
 * @return 0 if success, an error code otherwise.
 */
int button_pressed(gpio_pin_t button_pin, bool *button_pressed);

#endif /* _BUTTON_HANDLER_H_ */
