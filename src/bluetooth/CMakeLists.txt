#
# Copyright (c) 2022 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

target_sources(app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/audio_streaming.c
    ${CMAKE_CURRENT_SOURCE_DIR}/audio_streaming_actions.c
    ${CMAKE_CURRENT_SOURCE_DIR}/control_link.c
    ${CMAKE_CURRENT_SOURCE_DIR}/control_link_actions.c
)

add_subdirectory(bt_management)
add_subdirectory(bt_rendering_and_capture)
add_subdirectory(bt_content_control)
add_subdirectory(bt_stream)

zephyr_library_include_directories(
    bt_management
    bt_rendering_and_capture
    bt_content_control
    bt_stream
)
