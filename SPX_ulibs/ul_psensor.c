/*
 * l_psensor.c
 *
 *  Created on: 19 set. 2019
 *      Author: pablo
 */

#include "spx.h"

bool psensor_present = false;

//------------------------------------------------------------------------------------
void psensor_init(void)
{
	if ( I2C_scan_device( BUSADDR_BPS120 ) ) {
		psensor_present = true;
		xprintf_P(PSTR("PSENSOR detected.\r\n"));
	} else {
		psensor_present = false;
		xprintf_P(PSTR("PSENSOR not detected\r\n"));
	}
}
//------------------------------------------------------------------------------------
void psensor_config_defaults(void)
{

	snprintf_P( systemVars.psensor_conf.name, PARAMNAME_LENGTH, PSTR("PSEN\0"));
	systemVars.psensor_conf.count_min = 1480;
	systemVars.psensor_conf.count_max = 6200;
	systemVars.psensor_conf.pmin = 0;
	systemVars.psensor_conf.pmax = 28.5;
	systemVars.psensor_conf.offset = 0.0;
}
//------------------------------------------------------------------------------------
bool psensor_config ( char *s_pname, char *s_countMin, char *s_countMax, char *s_pmin, char *s_pmax, char *s_offset )
{

	// Esta opcion es solo valida para IO5
	if ( spx_io_board != SPX_IO5CH ) {
		psensor_config_defaults();
		return(false);
	}

	if ( s_pname != NULL ) {
		snprintf_P( systemVars.psensor_conf.name, PARAMNAME_LENGTH, PSTR("%s\0"), s_pname );
//		xprintf_P(PSTR("DEBUG NAME %s\r\n"), s_pname);
	}

	if ( s_countMin != NULL ) {
		systemVars.psensor_conf.count_min = atoi(s_countMin);
//		xprintf_P(PSTR("DEBUG CMIN [%s] %d\r\n"), s_countMin, systemVars.psensor_conf.count_min);
	}


	if ( s_countMax != NULL ) {
		systemVars.psensor_conf.count_max = atoi(s_countMax);
//		xprintf_P(PSTR("DEBUG CMAX [%s] %d\r\n"), s_countMax, systemVars.psensor_conf.count_max);
	}

	if ( s_pmin != NULL ) {
		systemVars.psensor_conf.pmin = atof(s_pmin);
//		xprintf_P(PSTR("DEBUG PMIN [%s] %.02f\r\n"), s_pmin, systemVars.psensor_conf.pmin);
	}

	if ( s_pmax != NULL ) {
		systemVars.psensor_conf.pmax = atof(s_pmax);
//		xprintf_P(PSTR("DEBUG PMAX [%s] %.02f\r\n"), s_pmax, systemVars.psensor_conf.pmax);
	}

	if ( s_offset != NULL ) {
		systemVars.psensor_conf.offset = atof(s_offset);
//		xprintf_P(PSTR("DEBUG OFFSET [%s] %.02f\r\n"), s_offset, systemVars.psensor_conf.offset);
	}

	return(true);

}
//------------------------------------------------------------------------------------
bool psensor_read( float *presion )
{

int8_t xBytes = 0;
char buffer[2] = { 0 };
uint8_t msbPres = 0;
uint8_t lsbPres = 0;
float pres = 0;
int32_t pcounts;

	if ( ! psensor_present ) {
		xprintf_P(PSTR("ERROR: psensor not present.\r\n\0"));
		*presion = -100;
		return(false);
	}

	xBytes = bps120_raw_read( buffer );
	if ( xBytes == -1 ) {
		xprintf_P(PSTR("ERROR: I2C: psensor_read\r\n\0"));
		*presion = -100;
		return(false);
	}

	if ( xBytes > 0 ) {
		msbPres = buffer[0]  & 0x3F;
		lsbPres = buffer[1];
		pcounts = (msbPres << 8) + lsbPres;

		pres = ( systemVars.psensor_conf.pmax - systemVars.psensor_conf.pmin ) / ( systemVars.psensor_conf.count_max - systemVars.psensor_conf.count_min );
		pres *= ( pcounts - systemVars.psensor_conf.count_min);
		pres += systemVars.psensor_conf.pmin;
		pres += systemVars.psensor_conf.offset;
		if ( pres < 0 )
			pres = 0.0;

		// pcounts = ( buffer[0]<<8 ) + buffer[1];

		// Aplico la funcion de transferencia.
		// Viene dada por los puntos countMin, countMax,pMin, pMax
		//pres = ( 1 * (pcounts - 1638)/13107 );
		//pres = (( 0.85 * (pcounts - 1638)/13107 ) + 0.15) * 70.31;

		*presion = pres;
		return(true);
	}

	return(true);

}
//------------------------------------------------------------------------------------
bool psensor_test_read( void )
{

int8_t xBytes = 0;
char buffer[2] = { 0 };
uint8_t msbPres = 0;
uint8_t lsbPres = 0;
float pres = 0;
int32_t pcounts;

	if ( ! psensor_present ) {
		xprintf_P(PSTR("ERROR: psensor not present.\r\n\0"));
		return(false);
	}

	xBytes = bps120_raw_read( buffer );
	if ( xBytes == -1 ) {
		xprintf_P(PSTR("ERROR: I2C: psensor_test_read\r\n\0"));
		return(false);
	}

	if ( xBytes > 0 ) {
		msbPres = buffer[0] & 0x3F;
		lsbPres = buffer[1];
		pcounts = (msbPres << 8) + lsbPres;
		// pcounts = ( buffer[0]<<8 ) + buffer[1];

		// Aplico la funcion de transferencia.
		//pres = (( 0.85 * (pcounts - 1638)/13107 ) + 0.15) * 70.31;

		xprintf_P( PSTR( "PRES TEST: raw=MSB[0x%02x], LSB[0x%02x]\r\n\0"),buffer[0],buffer[1]);

		pres = ( systemVars.psensor_conf.pmax - systemVars.psensor_conf.pmin ) / ( systemVars.psensor_conf.count_max - systemVars.psensor_conf.count_min );
		//xprintf_P( PSTR( "PRES TEST1:pres=%.03f\r\n\0"), pres );
		pres = pres * ( pcounts - systemVars.psensor_conf.count_min);
		//xprintf_P( PSTR( "PRES TEST2:pres=%.03f\r\n\0"), pres );
		pres = pres + systemVars.psensor_conf.pmin;
		pres += systemVars.psensor_conf.offset;
		//xprintf_P( PSTR( "PRES TEST3:pres=%.03f\r\n\0"), pres );
		xprintf_P( PSTR( "PRES TEST: pcounts=%d, "), pcounts );
		xprintf_P( PSTR( "pres=%.03f\r\n\0"), pres );
		//xprintf_P( PSTR( "PRES TEST4:pres=%.03f\r\n\0"), pres );

		return(true);
	}

	return(true);

}
//------------------------------------------------------------------------------------
void psensor_print(file_descriptor_t fd, float presion )
{

//	if ( ! strcmp ( systemVars.psensor_conf.name, "X" ) )
//		return;

	xCom_printf_P(fd, PSTR("%s:%.01f;\0"), systemVars.psensor_conf.name, presion );

}
//------------------------------------------------------------------------------------
bool psensor_config_autocalibrar( char *s_mag )
{
	// Calibra el parámetro offset.
	// El usuario pone un valor de referencia s_mag.
	// El sistema mide la presion y ajusta el offset para que el valor final sea el que
	// ingreso el usuario.

float pres_real;
int8_t xBytes = 0;
char buffer[2] = { 0 };
uint8_t msbPres = 0;
uint8_t lsbPres = 0;
float pres = 0;
int32_t pcounts;

	if ( ! psensor_present ) {
		xprintf_P(PSTR("ERROR: psensor not present.\r\n\0"));
		return(false);
	}

	pres_real = atof(s_mag);

	// Mido
	xBytes = bps120_raw_read( buffer );
	if ( xBytes == -1 ) {
		xprintf_P(PSTR("ERROR: I2C: psensor_test_read\r\n\0"));
		return(false);
	}

	if ( xBytes > 0 ) {
		msbPres = buffer[0] & 0x3F;
		lsbPres = buffer[1];
		pcounts = (msbPres << 8) + lsbPres;
		//xprintf_P( PSTR( "PRES TEST: raw=MSB[0x%02x], LSB[0x%02x]\r\n\0"),buffer[0],buffer[1]);

		pres = ( systemVars.psensor_conf.pmax - systemVars.psensor_conf.pmin ) / ( systemVars.psensor_conf.count_max - systemVars.psensor_conf.count_min );
		pres = pres * ( pcounts - systemVars.psensor_conf.count_min);
		pres = pres + systemVars.psensor_conf.pmin;
		//pres += systemVars.psensor_conf.offset;
		systemVars.psensor_conf.offset = ( pres_real - pres ) ;
		xprintf_P( PSTR( "PSENSOR_ACAL: raw=MSB[0x%02x], LSB[0x%02x]\r\n\0"),buffer[0],buffer[1]);
		xprintf_P( PSTR( "PSENSOR_ACAL: pres=%.01f\r\n\0"), pres );
		xprintf_P( PSTR( "PSENSOR_ACAL: offset=%.01f\r\n\0"), systemVars.psensor_conf.offset );
		return(true);
	}

	return(false);
}
//------------------------------------------------------------------------------------


