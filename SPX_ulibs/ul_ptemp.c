/*
 * ul_ptemp.c
 *
 *  Created on: 15 nov. 2019
 *      Author: pablo
 */


#include "spx.h"

//------------------------------------------------------------------------------------
void ptemp_init(void)
{
}
//------------------------------------------------------------------------------------
bool ptemp_read( float *tempC )
{

int8_t xBytes = 0;
char buffer[4] = { 0 };
uint8_t msbTemp = 0;
uint8_t lsbTemp = 0;
uint16_t temp = 0;

	xBytes = adt7410_raw_read( buffer );
	if ( xBytes == -1 ) {
		xprintf_P(PSTR("ERROR: I2C: ptemp_test_read\r\n\0"));
		*tempC = -100;
		return(false);
	}

	if ( xBytes > 0 ) {
		msbTemp = buffer[0];
		lsbTemp = buffer[1];
		temp = (msbTemp << 8) + lsbTemp;
		temp >>= 3;
		if(temp & 0x1000)   {    // Negative temperature
			*tempC = (float)( temp - 8192 ) / 16;
		} else {                 // Positive temperature
			*tempC = (float)temp / 16;
		}
		return(true);
	}

	return(true);

}
//------------------------------------------------------------------------------------
void ptemp_test_read (void)
{
	// Funcion de testing del sensor de temperatura
	// La direccion es fija 0x90 y solo se leen 4 bytes.

int8_t xBytes = 0;
char buffer[4] = { 0 };
uint8_t msbTemp = 0;
uint8_t lsbTemp = 0;
uint16_t temp = 0;
float tempC = 0;


	xBytes = adt7410_raw_read( buffer );
	if ( xBytes == -1 )
		xprintf_P(PSTR("ERROR: I2C: ptemp_test_read\r\n\0"));

	if ( xBytes > 0 ) {
		msbTemp = buffer[0];
		lsbTemp = buffer[1];
		temp = (msbTemp << 8) + lsbTemp;
		temp >>= 3;
		if(temp & 0x1000)   {    // Negative temperature
			tempC = (float)( temp - 8192 ) / 16;
		} else {                 // Positive temperature
			tempC = (float)temp / 16;
		}
		xprintf_P( PSTR( "I2C_RAW_READ=b0[0x%02x],b1[0x%02x],b2[0x%02x],b3[0x%02x]\r\n\0"),buffer[0],buffer[1],buffer[2],buffer[3]);
		xprintf_P( PSTR( "I2C_RAW_READ TEMP: %.02f, %d\r\n\0"), tempC, temp);
	}
}
//------------------------------------------------------------------------------------
void ptemp_print(file_descriptor_t fd, float temp )
{

//	if ( ! strcmp ( systemVars.psensor_conf.name, "X" ) )
//		return;

	xCom_printf_P(fd, PSTR("TEMP:%.02f;"), temp );
}
//------------------------------------------------------------------------------------
