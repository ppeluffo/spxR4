/*
 * l_bps120.h
 *
 *  Created on: 28 nov. 2019
 *      Author: pablo
 */

#ifndef SRC_SPX_LIBS_L_BPS120_H_
#define SRC_SPX_LIBS_L_BPS120_H_


#include "frtos-io.h"
#include "stdint.h"
#include "l_i2c.h"
#include "l_printf.h"

//--------------------------------------------------------------------------------
// API START

//int8_t bps120_raw_read( float *presion );
int8_t bps120_raw_read( char *data );

// API END
//--------------------------------------------------------------------------------


#endif /* SRC_SPX_LIBS_L_BPS120_H_ */
