/*
 * l_adt7410.h
 *
 *  Created on: 15 nov. 2019
 *      Author: pablo
 */

#ifndef SRC_SPX_LIBS_L_ADT7410_H_
#define SRC_SPX_LIBS_L_ADT7410_H_

#include "frtos-io.h"
#include "stdint.h"
#include "l_i2c.h"
#include "l_printf.h"

//--------------------------------------------------------------------------------
// API START

int8_t adt7410_raw_read( char *data );

// API END
//--------------------------------------------------------------------------------


#endif /* SRC_SPX_LIBS_L_ADT7410_H_ */
