/*
 * l_psensor.c
 *
 *  Created on: 19 set. 2019
 *      Author: pablo
 */

#include "spx.h"

//------------------------------------------------------------------------------------
void psensor_init(void)
{
}
//------------------------------------------------------------------------------------
void psensor_config_defaults(void)
{

	snprintf_P( systemVars.psensor_conf.name, PARAMNAME_LENGTH, PSTR("X\0"));
	systemVars.psensor_conf.pmax = 1.0;
	systemVars.psensor_conf.pmin = 0.0;
	systemVars.psensor_conf.offset = 0.0;

}
//------------------------------------------------------------------------------------
bool psensor_config ( char *s_pname, char *s_pmin, char *s_pmax,  char *s_offset   )
{

	if ( s_pname != NULL ) {
		snprintf_P( systemVars.psensor_conf.name, PARAMNAME_LENGTH, PSTR("%s\0"), s_pname );
	}

	if ( s_pmin != NULL ) {
		systemVars.psensor_conf.pmin = atof(s_pmin);
	}

	if ( s_pmax != NULL ) {
		systemVars.psensor_conf.pmax = atof(s_pmax);
	}

	if ( s_offset != NULL ) {
		systemVars.psensor_conf.offset = atof(s_offset);
	}

	//xprintf_P(PSTR("DEBUG PSENSOR [%s,%d],[%s,%d]\r\n\0"), s_pmin, systemVars.psensor_conf.pmin, s_pmax, systemVars.psensor_conf.pmax);
	return(true);

}
//------------------------------------------------------------------------------------
bool psensor_read( float *psens )
{

char buffer[2] = { 0 };
int8_t xBytes = 0;
int16_t pcounts;
bool retS = false;

	xBytes = PSENS_raw_read( buffer );
	if ( xBytes == -1 ) {
		xprintf_P(PSTR("ERROR: PSENSOR\r\n\0"));
		return(false);
	}

	if ( xBytes > 0 ) {
		/*
		pcounts = ( buffer[0]<<8 ) + buffer[1];
		psensor = pcounts;
		psensor *= systemVars.psensor_conf.pmax;
		psensor /= (0.9 * 16384);
		*psens = psensor;
		return(true);
		*/

		pcounts = ( buffer[0]<<8 ) + buffer[1];
		*psens = systemVars.psensor_conf.pmax * (pcounts - 1638)/13107 + systemVars.psensor_conf.offset;

	}

	return(retS);
}
//------------------------------------------------------------------------------------
void psensor_test_read (void)
{
	// Funcion de testing del sensor de presion I2C
	// La direccion es fija 0x50 y solo se leen 2 bytes.

int8_t xBytes = 0;
char buffer[2] = { 0 };
int16_t pcounts = 0;
float hcl;


	xBytes = PSENS_raw_read( buffer );
	if ( xBytes == -1 )
		xprintf_P(PSTR("ERROR: I2C: PSENS_test_read\r\n\0"));

	if ( xBytes > 0 )
		pcounts = ( buffer[0]<<8 ) + buffer[1];
		hcl = systemVars.psensor_conf.pmax * (pcounts - 1638)/13107;

		xprintf_P( PSTR( "I2C_PSENSOR=0x%04x,pcount=%d,Hmt=%0.3f\r\n\0"),pcounts,pcounts,hcl);

}
//------------------------------------------------------------------------------------

