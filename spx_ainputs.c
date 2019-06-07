/*
 * spx_ainputs.c
 *
 *  Created on: 8 mar. 2019
 *      Author: pablo
 */

#include "spx.h"

//------------------------------------------------------------------------------------
void ainputs_prender_12V_sensors(void)
{
	IO_set_SENS_12V_CTL();
}
//------------------------------------------------------------------------------------
void ainputs_apagar_12Vsensors(void)
{
	IO_clr_SENS_12V_CTL();
}
//------------------------------------------------------------------------------------
bool ainputs_config_channel( uint8_t channel,char *s_aname,char *s_imin,char *s_imax,char *s_mmin,char *s_mmax )
{

	// Configura los canales analogicos. Es usada tanto desde el modo comando como desde el modo online por gprs.

bool retS = false;

	if ( u_control_string(s_aname) == 0 ) {
		xprintf_P( PSTR("DEBUG ANALOG ERROR: A%d\r\n\0"), channel );
		return( false );
	}

	if ( s_aname == NULL ) {
		return(retS);
	}

	if ( ( channel >=  0) && ( channel < NRO_ANINPUTS ) ) {
		snprintf_P( systemVars.ainputs_conf.name[channel], PARAMNAME_LENGTH, PSTR("%s\0"), s_aname );
		if ( s_imin != NULL ) { systemVars.ainputs_conf.imin[channel] = atoi(s_imin); }
		if ( s_imax != NULL ) { systemVars.ainputs_conf.imax[channel] = atoi(s_imax); }
		if ( s_mmin != NULL ) { systemVars.ainputs_conf.mmin[channel] = atoi(s_mmin); }
		if ( s_mmax != NULL ) { systemVars.ainputs_conf.mmax[channel] = atof(s_mmax); }
		retS = true;
	}

	return(retS);
}
//------------------------------------------------------------------------------------
bool ainputs_config_offset( char *s_channel, char *s_offset )
{
	// Configuro el parametro offset de un canal analogico.

uint8_t channel;
float offset;

	channel = atoi(s_channel);
	if ( ( channel >=  0) && ( channel < NRO_ANINPUTS ) ) {
		offset = atof(s_offset);
		systemVars.ainputs_conf.offset[channel] = offset;
		return(true);
	}

	return(false);
}
//------------------------------------------------------------------------------------
void ainputs_config_timepwrsensor ( char *s_sensortime )
{
	// Configura el tiempo de espera entre que prendo  la fuente de los sensores y comienzo el poleo.
	// Se utiliza solo desde el modo comando.
	// El tiempo de espera debe estar entre 1s y 15s

	systemVars.ainputs_conf.pwr_settle_time = atoi(s_sensortime);

	if ( systemVars.ainputs_conf.pwr_settle_time < 1 )
		systemVars.ainputs_conf.pwr_settle_time = 1;

	if ( systemVars.ainputs_conf.pwr_settle_time > 15 )
		systemVars.ainputs_conf.pwr_settle_time = 15;

	return;
}
//------------------------------------------------------------------------------------
void ainputs_config_span ( char *s_channel, char *s_span )
{
	// Configura el factor de correccion del span de canales delos INA.
	// Esto es debido a que las resistencias presentan una tolerancia entonces con
	// esto ajustamos que con 20mA den la excursión correcta.
	// Solo de configura desde modo comando.

uint8_t channel;
uint16_t span;

	channel = atoi(s_channel);
	if ( ( channel >=  0) && ( channel < NRO_ANINPUTS ) ) {
		span = atoi(s_span);
		systemVars.ainputs_conf.inaspan[channel] = span;
	}
	return;

}
//------------------------------------------------------------------------------------
bool ainputs_config_autocalibrar( char *s_channel, char *s_mag_val )
{
	// Para un canal, toma como entrada el valor de la magnitud y ajusta
	// mag_offset para que la medida tomada coincida con la dada.


uint16_t an_raw_val;
float an_mag_val;
float I,M,P;
uint16_t D;
uint8_t channel;

float an_mag_val_real;
float offset;

	channel = atoi(s_channel);

	if ( channel >= NRO_ANINPUTS ) {
		return(false);
	}

	// Leo el canal del ina.
	an_raw_val = ainputs_read_channel_raw( channel );
//	xprintf_P( PSTR("ANRAW=%d\r\n\0"), an_raw_val );

	// Convierto el raw_value a la magnitud
	I = (float)( an_raw_val) * 20 / ( systemVars.ainputs_conf.inaspan[channel] + 1);
	P = 0;
	D = systemVars.ainputs_conf.imax[channel] - systemVars.ainputs_conf.imin[channel];

	an_mag_val = 0.0;
	if ( D != 0 ) {
		// Pendiente
		P = (float) ( systemVars.ainputs_conf.mmax[channel]  -  systemVars.ainputs_conf.mmin[channel] ) / D;
		// Magnitud
		M = (float) ( systemVars.ainputs_conf.mmin[channel] + ( I - systemVars.ainputs_conf.imin[channel] ) * P);

		// En este caso el offset que uso es 0 !!!.
		an_mag_val = M;

	} else {
		return(false);
	}

//	xprintf_P( PSTR("ANMAG=%.02f\r\n\0"), an_mag_val );

	an_mag_val_real = atof(s_mag_val);
//	xprintf_P( PSTR("ANMAG_T=%.02f\r\n\0"), an_mag_val_real );

	offset = an_mag_val_real - an_mag_val;
//	xprintf_P( PSTR("AUTOCAL offset=%.02f\r\n\0"), offset );

	systemVars.ainputs_conf.offset[channel] = offset;

	xprintf_P( PSTR("OFFSET=%.02f\r\n\0"), systemVars.ainputs_conf.offset[channel] );

	return(true);

}
//------------------------------------------------------------------------------------
void ainputs_config_defaults(void)
{
	// Realiza la configuracion por defecto de los canales digitales.

uint8_t channel;

	systemVars.ainputs_conf.pwr_settle_time = 1;

	for ( channel = 0; channel < NRO_ANINPUTS; channel++) {
		systemVars.ainputs_conf.imin[channel] = 0;
		systemVars.ainputs_conf.imax[channel] = 20;
		systemVars.ainputs_conf.mmin[channel] = 0;
		systemVars.ainputs_conf.mmax[channel] = 6.0;
		systemVars.ainputs_conf.offset[channel] = 0.0;
		systemVars.ainputs_conf.inaspan[channel] = 3646;
		snprintf_P( systemVars.ainputs_conf.name[channel], PARAMNAME_LENGTH, PSTR("A%d\0"),channel );
	}

}
//------------------------------------------------------------------------------------
uint16_t ainputs_read_channel_raw(uint8_t channel_id )
{
	// Como tenemos 2 arquitecturas de dataloggers, SPX_5CH y SPX_8CH,
	// los canales estan mapeados en INA con diferentes id.

	// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	// Aqui convierto de io_channel a (ina_id, ina_channel )
	// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

	// ina_id es el parametro que se pasa a la funcion INA_id2busaddr para
	// que me devuelva la direccion en el bus I2C del dispositivo.


uint8_t ina_reg = 0;
uint8_t ina_id = 0;
uint16_t an_raw_val;
uint8_t MSB, LSB;
char res[3];
int8_t xBytes;
//float vshunt;

	switch(spx_io_board) {

	case SPX_IO5CH:	// Datalogger SPX_5CH
		switch ( channel_id ) {
		case 0:
			ina_id = INA_A; ina_reg = INA3221_CH1_SHV;
			break;
		case 1:
			ina_id = INA_A; ina_reg = INA3221_CH2_SHV;
			break;
		case 2:
			ina_id = INA_A; ina_reg = INA3221_CH3_SHV;
			break;
		case 3:
			ina_id = INA_B; ina_reg = INA3221_CH2_SHV;
			break;
		case 4:
			ina_id = INA_B; ina_reg = INA3221_CH3_SHV;
			break;
		case 99:
			ina_id = INA_B; ina_reg = INA3221_CH1_BUSV;
			break;	// Battery
		default:
			return(-1);
			break;
		}
		break;

	case SPX_IO8CH:	// Datalogger SPX_8CH
		switch ( channel_id ) {
		case 0:
			ina_id = INA_B; ina_reg = INA3221_CH1_SHV;
			break;
		case 1:
			ina_id = INA_B; ina_reg = INA3221_CH2_SHV;
			break;
		case 2:
			ina_id = INA_B; ina_reg = INA3221_CH3_SHV;
			break;
		case 3:
			ina_id = INA_A; ina_reg = INA3221_CH1_SHV;
			break;
		case 4:
			ina_id = INA_A; ina_reg = INA3221_CH2_SHV;
			break;
		case 5:
			ina_id = INA_A; ina_reg = INA3221_CH3_SHV;
			break;
		case 6:
			ina_id = INA_C; ina_reg = INA3221_CH1_SHV;
			break;
		case 7:
			ina_id = INA_C; ina_reg = INA3221_CH2_SHV;
			break;
		case 8:
			ina_id = INA_C; ina_reg = INA3221_CH3_SHV;
			break;
		default:
			return(-1);
			break;
		}
		break;

	default:
		return(-1);
		break;
	}

	// Leo el valor del INA.
//	xprintf_P(PSTR("DEBUG: INAID = %d\r\n\0"), ina_id );

	xBytes = INA_read( ina_id, ina_reg, res ,2 );
	if ( xBytes == -1 )
		xprintf_P(PSTR("ERROR I2C: ainputs_read_channel_raw.\r\n\0"));

	an_raw_val = 0;
	MSB = res[0];
	LSB = res[1];
	an_raw_val = ( MSB << 8 ) + LSB;
	an_raw_val = an_raw_val >> 3;

//	vshunt = (float) an_raw_val * 40 / 1000;
//	xprintf_P( PSTR("ACH: ch=%d, ina=%d, reg=%d, MSB=0x%x, LSB=0x%x, ANV=(0x%x)%d, VSHUNT = %.02f(mV)\r\n\0") ,channel_id, ina_id, ina_reg, MSB, LSB, an_raw_val, an_raw_val, vshunt );

	return( an_raw_val );
}
//------------------------------------------------------------------------------------
float ainputs_read_channel ( uint8_t io_channel )
{
	// Lee un canal analogico y devuelve el valor convertido a la magnitud configurada.
	// Es publico porque se utiliza tanto desde el modo comando como desde el modulo de poleo de las entradas.
	// Hay que corregir la correspondencia entre el canal leido del INA y el canal fisico del datalogger
	// io_channel. Esto lo hacemos en AINPUTS_read_ina.

uint16_t an_raw_val;
float an_mag_val;
float I,M,P;
uint16_t D;

	// Leo el valor del INA.
	an_raw_val = ainputs_read_channel_raw( io_channel );

	// Convierto el raw_value a la magnitud
	// Calculo la corriente medida en el canal
	I = (float)( an_raw_val) * 20 / ( systemVars.ainputs_conf.inaspan[io_channel] + 1);

	// Calculo la magnitud
	P = 0;
	D = systemVars.ainputs_conf.imax[io_channel] - systemVars.ainputs_conf.imin[io_channel];

	an_mag_val = 0.0;
	if ( D != 0 ) {
		// Pendiente
		P = (float) ( systemVars.ainputs_conf.mmax[io_channel]  -  systemVars.ainputs_conf.mmin[io_channel] ) / D;
		// Magnitud
		M = (float) (systemVars.ainputs_conf.mmin[io_channel] + ( I - systemVars.ainputs_conf.imin[io_channel] ) * P);

		// Al calcular la magnitud, al final le sumo el offset.
		an_mag_val = M + systemVars.ainputs_conf.offset[io_channel];
	} else {
		// Error: denominador = 0.
		an_mag_val = -999.0;
	}

	return(an_mag_val);

}
//------------------------------------------------------------------------------------
uint16_t ainputs_read_battery_raw(void)
{
	if ( spx_io_board != SPX_IO5CH ) {
		return(-1);
	}

	return( ainputs_read_channel_raw(99));
}
//------------------------------------------------------------------------------------
float ainputs_read_battery(void)
{
float battery_level;

	if ( spx_io_board != SPX_IO5CH ) {
		return(-1);
	}

	// Convierto el raw_value a la magnitud ( 8mV por count del A/D)
	battery_level =  0.008 * ainputs_read_battery_raw();

	return(battery_level);
}
//------------------------------------------------------------------------------------
void ainputs_awake(void)
{
	switch (spx_io_board) {
	case SPX_IO5CH:
		INA_config_avg128(INA_A );
		INA_config_avg128(INA_B );
		break;
	case SPX_IO8CH:
		INA_config_avg128(INA_A );
		INA_config_avg128(INA_B );
		INA_config_avg128(INA_C );
		break;
	}
}
//------------------------------------------------------------------------------------
void ainputs_sleep(void)
{
	switch (spx_io_board) {
	case SPX_IO5CH:
		INA_config_sleep(INA_A );
		INA_config_sleep(INA_B );
		break;
	case SPX_IO8CH:
		INA_config_sleep(INA_A );
		INA_config_sleep(INA_B );
		INA_config_sleep(INA_C );
		break;
	}
}
//------------------------------------------------------------------------------------
void ainputs_df_print( dataframe_s *df )
{

uint8_t channel;

	// Canales analogicos: Solo muestro los que tengo configurados.
	for ( channel = 0; channel < NRO_ANINPUTS; channel++) {
		if ( ! strcmp ( systemVars.ainputs_conf.name[channel], "X" ) )
			continue;

		xprintf_P(PSTR(",%s=%.02f"),systemVars.ainputs_conf.name[channel], df->ainputs[channel] );
	}
}
//------------------------------------------------------------------------------------
void ainputs_df_print_battery( dataframe_s *df )
{
	// bateria
	if ( spx_io_board == SPX_IO5CH ) {
		xprintf_P(PSTR(",bt=%.02f"), df->battery );
	}
}
//------------------------------------------------------------------------------------

