/*
 * File:           C:\Users\colombari\Desktop\tiresias-soak\tiresias-soak-ha\tiresias-soak-ha-export\tiresias-soak-ha_IC_1_FAST.h
 *
 * Created:        Wednesday, September 2, 2026 2:26:02 PM
 * Description:    tiresias-soak-ha:IC 1-Fast program data.
 *
 * This software is distributed in the hope that it will be useful,
 * but is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 * CONDITIONS OF ANY KIND, without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * This software may only be used to program products purchased from
 * Analog Devices for incorporation by you into audio products that
 * are intended for resale to audio product end users. This software
 * may not be distributed whole or in any part to third parties.
 *
 * Copyright ©2026 Analog Devices, Inc. All rights reserved.
 */
#ifndef __TIRESIAS_SOAK_HA_IC_1_FAST_H__
#define __TIRESIAS_SOAK_HA_IC_1_FAST_H__

#include "SigmaStudioFW.h"
#include "tiresias-soak-ha_IC_1_FAST_REG.h"

#define DEVICE_ARCHITECTURE_IC_1_FAST             "ADAU1787F"
#define DEVICE_ADDR_IC_1_FAST                     0x50


/* Register Default - IC 1-Fast.FDSP_RUN */
ADI_REG_TYPE R0_FDSP_RUN_IC_1_Fast_Default[REG_FDSP_RUN_IC_1_Fast_BYTE] = {
0x00
};

/* Register Default - IC 1-Fast.FDSP_RUN */
ADI_REG_TYPE R1_FDSP_RUN_IC_1_Fast_Default[REG_FDSP_RUN_IC_1_Fast_BYTE] = {
0x01
};


/*
 * Default Download
 */
#define DEFAULT_DOWNLOAD_SIZE_IC_1_Fast 2

void default_download_IC_1_Fast() {
	SIGMA_WRITE_REGISTER_BLOCK( DEVICE_ADDR_IC_1_FAST, REG_FDSP_RUN_IC_1_Fast_ADDR, REG_FDSP_RUN_IC_1_Fast_BYTE, R0_FDSP_RUN_IC_1_Fast_Default );
	SIGMA_WRITE_REGISTER_BLOCK( DEVICE_ADDR_IC_1_FAST, REG_FDSP_RUN_IC_1_Fast_ADDR, REG_FDSP_RUN_IC_1_Fast_BYTE, R1_FDSP_RUN_IC_1_Fast_Default );
}

#endif
