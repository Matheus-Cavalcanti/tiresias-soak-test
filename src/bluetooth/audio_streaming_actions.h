/*
 * Copyright (c) 2026 Tiresias Firmware Team
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef AUDIO_STREAMING_ACTIONS_H
#define AUDIO_STREAMING_ACTIONS_H

#include "zbus_common.h"

#include <stdint.h>

struct bt_le_per_adv_sync;

int audio_streaming_actions_start(void);
int audio_streaming_actions_start_scan(void);
int audio_streaming_actions_set_pa_sync(struct bt_le_per_adv_sync* pa_sync, uint32_t broadcast_id);
int audio_streaming_actions_configure_pipeline(void);
void audio_streaming_actions_start_pipeline(void);
void audio_streaming_actions_stop_pipeline(void);
int audio_streaming_actions_restart_scan(void);
int audio_streaming_actions_restart_after_sync_loss(struct bt_le_per_adv_sync* pa_sync);
int audio_streaming_actions_restart_after_stream_stop(void);
int audio_streaming_actions_stop(void);
int audio_streaming_actions_set_indicator(audio_streaming_state state);

#endif /* AUDIO_STREAMING_ACTIONS_H */
