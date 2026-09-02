#ifndef _TIRESIAS_DK_H_
#define _TIRESIAS_DK_H_

#include "led.h"

/**
 * @brief	Initialize the hardware related modules on the Tiresias DK.
 *
 * @return	0 if successful, error otherwise.
 */
int tiresias_dk_init(void);

/**
 * @brief	Compatibility wrapper for application code still using the
 *		nRF5340 Audio DK init name.
 *
 * @return	0 if successful, error otherwise.
 */
int nrf5340_audio_dk_init(void);

#endif /* _TIRESIAS_DK_H_ */
