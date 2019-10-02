/*
 * spx_dinputs.c
 *
 *  Created on: 4 abr. 2019
 *      Author: pablo
 */

#include "spx.h"

//------------------------------------------------------------------------------------
void dinputs_init( void )
{
	// En el caso del SPX_8CH se deberia inicializar el port de salidas del MCP
	// pero esto se hace en la funcion MCP_init(). Esta tambien inicializa el port
	// de entradas digitales.

	switch (spx_io_board ) {
	case SPX_IO5CH:
		IO_config_PA0();	// D0
		IO_config_PB7();	// D1
		break;
	case SPX_IO8CH:
		MCP_init();
		break;
	}

}
//------------------------------------------------------------------------------------
bool dinputs_config_channel( uint8_t channel,char *s_aname ,char *s_tpoll )
{

	// Configura los canales digitales. Es usada tanto desde el modo comando como desde el modo online por gprs.

bool retS = false;

	if ( u_control_string(s_aname) == 0 ) {
		xprintf_P( PSTR("DEBUG DIGITAL ERROR: D%d\r\n\0"), channel );
		return( false );
	}

	if ( s_aname == NULL ) {
		return(retS);
	}

	if ( ( channel >=  0) && ( channel < NRO_DINPUTS ) ) {
		snprintf_P( systemVars.dinputs_conf.name[channel], PARAMNAME_LENGTH, PSTR("%s\0"), s_aname );
		if ( s_tpoll != NULL ) {
			systemVars.dinputs_conf.tpoll[channel] = atoi(s_tpoll);
		}
		retS = true;
	}


	return(retS);
}
//------------------------------------------------------------------------------------
void dinputs_config_defaults(void)
{
	// Realiza la configuracion por defecto de los canales digitales.

uint8_t i = 0;

	for ( i = 0; i < MAX_DINPUTS_CHANNELS; i++ ) {
		snprintf_P( systemVars.dinputs_conf.name[i], PARAMNAME_LENGTH, PSTR("D%d\0"), i );
		systemVars.dinputs_conf.tpoll[i] = 0;
	}

}
//------------------------------------------------------------------------------------
int8_t dinputs_read_channel ( uint8_t din )
{

	// Solo devuleve el nivel logico de la entrada. ( OJO: No el dtimer !!! )

int8_t val = -1 ;
uint8_t port = 0;
int8_t rdBytes = 0;

	switch (spx_io_board ) {

	case SPX_IO5CH:	// SPX_IO5
		if ( din == 0 ) {
			val = IO_read_PA0();
		} else if ( din == 1 ) {
			val = IO_read_PB7();
		}
		break;

	case SPX_IO8CH:
		rdBytes = MCP_read( MCP_GPIOA, (char *)&port, 1 );
		if ( rdBytes == -1 ) {
			xprintf_P(PSTR("ERROR: IO_DIN_read_pin\r\n\0"));
			return(-1);
		}
		val = ( port & ( 1 << din )) >> din;
		break;
	}

	return(val);

}
//------------------------------------------------------------------------------------
void dinputs_df_print( dataframe_s *df, bool print_xbee )
{
	// Canales digitales.

uint8_t channel = 0;

	for ( channel = 0; channel < NRO_DINPUTS; channel++) {
		if ( ! strcmp ( systemVars.dinputs_conf.name[channel], "X" ) )
			continue;

		xprintf_P(PSTR(",%s=%d"), systemVars.dinputs_conf.name[channel], df->dinputsA[channel] );
		if (print_xbee) {
			xCom_printf_P( fdXBEE, PSTR(",%s=%d"), systemVars.dinputs_conf.name[channel], df->dinputsA[channel] );
		}
	}
}
//------------------------------------------------------------------------------------

