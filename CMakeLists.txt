#
# Copyright (c) 2022 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

cmake_minimum_required(VERSION 3.20.0)

# Flag which defines whether application is compiled as gateway/dongle or headset
add_compile_definitions(HEADSET=1)
add_compile_definitions(GATEWAY=2)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(new-tiresias)

# The HA soak profile only programs the ADAU1787 and then lets the application
# core fall through to Zephyr's idle thread. Keep the normal application source
# graph untouched for the BLE profiles.
if(CONFIG_TIRESIAS_SOAK_PROFILE_HA)
  target_sources(app PRIVATE src/modes/main_ha.c)

  zephyr_library_include_directories(
      src/drivers
      src/utils/macros
      src/SigmaStudioFiles
  )

  add_subdirectory(src/drivers)
  add_subdirectory(src/SigmaStudioFiles)
elseif(CONFIG_TIRESIAS_SOAK_PROFILE_ADAU_PD)
  target_sources(app PRIVATE src/modes/main_adau_pd.c)
else()
  string(TIMESTAMP NRF5340_AUDIO_CORE_APP_COMP_DATE "%a %b %d %H:%M:%S %Y")

# Generate fw_info_app.c
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/src/utils/fw_info_app.c.in"
               "${CMAKE_BINARY_DIR}/fw_info_app.c"
               @ONLY)

# Target sources below are specific to the nRF5340 Audio DK HW
target_sources(app PRIVATE
    ${CMAKE_BINARY_DIR}/fw_info_app.c
    )

target_sources(app PRIVATE src/main.c)

# Include application events and configuration headers
zephyr_library_include_directories(
    include
    src/audio
    src/application
    src/bluetooth
    src/drivers
    src/modules
    src/utils
    src/utils/macros
    src/SigmaStudioFiles
)

zephyr_library_include_directories(app PRIVATE
    ${ZEPHYR_NRF_MODULE_DIR}/boards/arm/nrf5340_audio_dk_nrf5340)

# Application sources
add_subdirectory(src/audio)
add_subdirectory(src/application)
add_subdirectory(src/bluetooth)
add_subdirectory(src/drivers)
add_subdirectory(src/modules)
add_subdirectory(src/utils)
add_subdirectory(src/SigmaStudioFiles)
endif()
