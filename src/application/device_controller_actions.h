/*
 * Copyright (c) 2026 Tiresias Firmware Team
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef DEVICE_CONTROLLER_ACTIONS_H
#define DEVICE_CONTROLLER_ACTIONS_H

#include "zbus_common.h"

int publish_codec_controller_command(codec_controller_cmd command);
int publish_audio_streaming_command(audio_streaming_cmd command);

#endif /* DEVICE_CONTROLLER_ACTIONS_H */
