/*
 * Copyright (c) 2026 Tiresias Firmware Team
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef CONTROL_LINK_ACTIONS_H
#define CONTROL_LINK_ACTIONS_H

#include "zbus_common.h"

int control_link_actions_enable(void);
int control_link_actions_restart_advertising(void);
int control_link_actions_disable(void);
int control_link_actions_set_indicator(control_link_state state);

#endif /* CONTROL_LINK_ACTIONS_H */
