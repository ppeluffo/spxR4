/*
 * ul_xbee.c
 *
 *  Created on: 20 set. 2019
 *      Author: pablo
 *
 *  XBEE_CLASE_A: Master
 *  XBEE_CLASE_B: Slave + gprs
 *  XBEE_CLASE_C: Slave sin gprs.
 *
 */

#include "spx.h"

//------------------------------------------------------------------------------------
void xbee_config_defaults(void)
{
	systemVars.xbee = XBEE_OFF;
}
//------------------------------------------------------------------------------------
bool xbee_config ( char *s_mode )
{

char l_data[10] = { '\0','\0','\0','\0','\0','\0','\0','\0','\0','\0' };

	memcpy(l_data, s_mode, sizeof(l_data));
	strupr(l_data);

	if ( !strcmp_P( l_data, PSTR("OFF\0"))) {
		systemVars.xbee = XBEE_OFF;
	} else if ( !strcmp_P( l_data, PSTR("MASTER\0"))) {
		systemVars.xbee = XBEE_MASTER;
	} else if ( !strcmp_P( l_data, PSTR("SLAVE\0"))) {
		systemVars.xbee = XBEE_SLAVE;
	} else {
		return(false);
	}

	return(true);
}
//------------------------------------------------------------------------------------

