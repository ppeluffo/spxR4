/*
 * l_psensor.h
 *
 *  Created on: 23 ago. 2019
 *      Author: pablo
 */

#ifndef SRC_SPX_LIBS_L_PSENSOR_H_
#define SRC_SPX_LIBS_L_PSENSOR_H_

#include "frtos-io.h"
#include "stdint.h"
#include "l_i2c.h"
#include "l_printf.h"

//--------------------------------------------------------------------------------
// API START

int8_t PSENS_raw_read( char *data );

// API END
//--------------------------------------------------------------------------------


#endif /* SRC_SPX_LIBS_L_PSENSOR_H_ */
