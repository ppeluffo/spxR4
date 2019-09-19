/*
 * l_psensor.c
 *
 *  Created on: 23 ago. 2019
 *      Author: pablo
 */



#include "l_i2c.h"
#include "l_psensor.h"

//------------------------------------------------------------------------------------
int8_t PSENS_test_read( void )
{
	// Funcion de testing del sensor de presion I2C
	// La direccion es fija 0x50 y solo se leen 2 bytes.

int8_t xBytes = 0;
char buffer[2] = { 0 };
int16_t pcounts = 0;

	xBytes = PSENS_read( buffer );
	if ( xBytes == -1 )
		xprintf_P(PSTR("ERROR: I2C: PSENS_test_read\r\n\0"));

	if ( xBytes > 0 )
		pcounts = ( buffer[0]<<8 ) + buffer[1];
		xprintf_P( PSTR( "I2C_PSENSOR=0x%04x,%d\r\n\0"),pcounts,pcounts);

	return (xBytes );

}
//------------------------------------------------------------------------------------
int8_t PSENS_read( char *data )
{

int8_t rcode = 0;
uint8_t times = 3;


	while ( times-- > 0 ) {

		// Leo 2 bytes del sensor de presion.
		// El resultado es de 14 bits.
		rcode =  I2C_read( BUSADDR_PSENS, 0, data, 0x02 );

		if ( rcode == -1 ) {
			// Hubo error: trato de reparar el bus y reintentar la operacion
			// Espero 1s que se termine la fuente de ruido.
			vTaskDelay( ( TickType_t)( 1000 / portTICK_RATE_MS ) );
			// Reconfiguro los dispositivos I2C del bus que pueden haberse afectado
			xprintf_P(PSTR("ERROR: PSENS_read recovering i2c bus (%d)\r\n\0"), times );
			I2C_reinit_devices();
		} else {
			// No hubo error: salgo normalmente
			break;
		}
	}
	return( rcode );
}
//------------------------------------------------------------------------------------
