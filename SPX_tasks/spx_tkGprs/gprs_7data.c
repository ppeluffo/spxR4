/*
 * sp5KV5_tkGprs_data.c
 *
 *  Created on: 27 de abr. de 2017
 *      Author: pablo
 */

#include "gprs.h"

static bool pv_hay_datos_para_trasmitir(void);
static bool pv_procesar_frame( void );
static bool pv_trasmitir_frame(void );
static void pv_try_to_open_socket(void);

static void pv_trasmitir_dataHeader( void );
static void pv_trasmitir_dataTail( void );
static void pv_trasmitir_dataframe( void );
static void pv_transmitir_df_analogicos( void );
static void pv_transmitir_df_digitales( void );
static void pv_transmitir_df_contadores( void );
static void pv_transmitir_df_range( void );
static void pv_transmitir_df_bateria( void );

static bool pv_procesar_respuesta_server(void);
static void pv_process_response_RESET(void);
static uint8_t pv_process_response_OK(void);
static void pv_process_response_DOUTS(void);
static void pv_process_response_POUT(void);
static bool pv_check_more_Rcds4Del ( void );

dataframe_s df;

// La tarea se repite para cada paquete de datos. Esto no puede demorar
// mas de 5 minutos
#define WDG_GPRS_TO_DATA	300
//------------------------------------------------------------------------------------
bool st_gprs_data(void)
{
	// Me quedo en un loop permanente revisando si hay datos y si hay los trasmito
	// Solo salgo en caso que hubiesen problemas para trasmitir
	// Esto es que no puedo abrir un socket mas de 3 veces o que no puedo trasmitir
	// el mismo frame mas de 3 veces.
	// En estos casos salgo con false de modo de hacer un ciclo completo apagando el modem.
	// Si por algun problema no puedo trasmitir, salgo asi me apago y reinicio.
	// Si pude trasmitir o simplemente no hay datos, en modo continuo retorno TRUE.

	// Si hay datos los trasmito todos
	// Solo salgo en caso que hubiesen problemas para trasmitir
	// Esto es que no puedo abrir un socket mas de 3 veces o que no puedo trasmitir
	// el mismo frame mas de 3 veces.
	// En estos casos salgo con false de modo de hacer un ciclo completo apagando el modem.

	// Mientras estoy trasmitiendo datos no atiendo las señales; solo durante la espera en modo
	// continuo.

uint8_t sleep_time;
bool exit_flag = bool_RESTART;

	GPRS_stateVars.state = G_DATA;

	xprintf_P( PSTR("GPRS: data.\r\n\0"));

	//
	while ( 1 ) {

		// Si por algun problema no puedo trasmitir, salgo asi me apago y reinicio.
		// Si pude trasmitir o simplemente no hay datos, en modo continuo retorno TRUE.

		ctl_watchdog_kick(WDG_GPRSTX, WDG_GPRS_TO_DATA );

		if ( pv_hay_datos_para_trasmitir() ) {	// Si hay datos, intento trasmitir

			if ( ! pv_procesar_frame() ) {		// Si por cualquier cosa no pude transmitir un frame
				exit_flag = bool_RESTART;		// salgo a apagarme.
				goto EXIT;
			}

		} else {
			// No hay datos para trasmitir

			// Modo discreto, Salgo a apagarme y esperar
			if ( MODO_DISCRETO ) {
				exit_flag = bool_CONTINUAR ;
				goto EXIT;

			} else {
				// MODO CONTINUO
				sleep_time = 90;
				while( sleep_time-- > 0 ) {
					vTaskDelay( (portTickType)( 1000 / portTICK_RATE_MS ) );

					// PROCESO LAS SEÑALES
					if ( GPRS_stateVars.signal_redial) {
						// Salgo a discar inmediatamente.
						GPRS_stateVars.signal_redial = false;
						exit_flag = bool_RESTART;
						goto EXIT;
					}

					if ( GPRS_stateVars.signal_frameReady) {
						GPRS_stateVars.signal_frameReady = false;
						break;	// Salgo del while de espera.
					}
				} // while
			} // else
		} // else
	} // while

	// Exit area:
EXIT:
	// No espero mas y salgo del estado prender.
	return(exit_flag);
}
//------------------------------------------------------------------------------------
static bool pv_hay_datos_para_trasmitir(void)
{

	/* Veo si hay datos en memoria para trasmitir
	 * Memoria vacia: rcds4wr = MAX, rcds4del = 0;
	 * Memoria llena: rcds4wr = 0, rcds4del = MAX;
	 * Memoria toda leida: rcds4rd = 0;
	 */

//	l_fat.wrPTR,l_fat.rdPTR, l_fat.delPTR,l_fat.rcds4wr,l_fat.rcds4rd,l_fat.rcds4del );

FAT_t l_fat;
bool retS = false;

	FAT_read(&l_fat);

	// Si hay registros para leer
	if ( l_fat.rcds4rd > 0) {
		retS = true;
	} else {
		retS = false;
		if ( systemVars.debug == DEBUG_GPRS ) {
			xprintf_P( PSTR("GPRS: bd EMPTY\r\n\0"));
		}
	}

	return(retS);
}
//------------------------------------------------------------------------------------
static bool pv_procesar_frame( void )
{
	// Intento MAX_TRYES de trasmitir un frame y recibir la respuesta correctamente

uint8_t intentos;

	for ( intentos = 0; intentos < MAX_INIT_TRYES; intentos++ ) {

		if ( pv_trasmitir_frame() && pv_procesar_respuesta_server() ) {
			// Intento madar el frame al servidor
			// Aqui es que anduvo todo bien y debo salir para pasar al modo DATA
				if ( systemVars.debug == DEBUG_GPRS ) {
					xprintf_P( PSTR("GPRS: data packet sent.\r\n\0" ));
				}
				return(true);

		} else {

			// Espero 3s antes de reintentar
			vTaskDelay( (portTickType)( 3000 / portTICK_RATE_MS ) );
			if ( systemVars.debug == DEBUG_GPRS ) {
				xprintf_P( PSTR("GPRS: data packey retry(%d)\r\n\0"),intentos);
			}
		}
	}
	// No pude transmitir despues de MAX_TRYES
	return(false);

}
//------------------------------------------------------------------------------------
static bool pv_trasmitir_frame(void )
{
	// Hay datos que intento trasmitir.
	// Leo los mismos hasta completar un TX_WINDOW
	// El socket lo chequeo antes de comenzar a trasmitir. Una vez que comienzo
	// trasmito todo y paso a esperar la respuesta donde chequeo el socket nuevamente.
	// Intento enviar el mismo paquete de datos hasta 3 veces.
	// Entro luego de haber chequeado que hay registros para trasmitir o sea que se que los hay !!

uint8_t registros_trasmitidos = 0;
uint8_t i;
t_socket_status socket_status;

	for ( i = 0; i < MAX_TRYES_OPEN_SOCKET; i++ ) {

		socket_status = u_gprs_check_socket_status();

		if (  socket_status == SOCK_OPEN ) {
			// Envio un window frame
			registros_trasmitidos = 0;
			FF_rewind();
			pv_trasmitir_dataHeader();
			while ( pv_hay_datos_para_trasmitir() && ( registros_trasmitidos < MAX_RCDS_WINDOW_SIZE ) ) {
				pv_trasmitir_dataframe();
				registros_trasmitidos++;
			}
			pv_trasmitir_dataTail();
			return(true);

		} else {

			pv_try_to_open_socket();
		}

	}

	return(false);
}
//------------------------------------------------------------------------------------
static void pv_try_to_open_socket(void)
{
	// Doy el comando de abrir el socket
uint8_t timeout, await_loops;
t_socket_status socket_status;

	u_gprs_open_socket();
	// Y espero que lo abra
	await_loops = ( 10 * 1000 / 3000 ) + 1;
	// Y espero hasta 30s que abra.
	for ( timeout = 0; timeout < await_loops; timeout++) {
		vTaskDelay( (portTickType)( 3000 / portTICK_RATE_MS ) );
		socket_status = u_gprs_check_socket_status();

		// Si el socket abrio, salgo para trasmitir el frame de init.
		if ( socket_status == SOCK_OPEN ) {
			break;
		}

		// Si el socket dio error, salgo para enviar de nuevo el comando.
		if ( socket_status == SOCK_ERROR ) {
			break;
		}
	}
}
//------------------------------------------------------------------------------------
static void pv_trasmitir_dataHeader( void )
{

	u_gprs_flush_RX_buffer();
	u_gprs_flush_TX_buffer();

	// Armo el header en el buffer

	xCom_printf_P( fdGPRS, PSTR("GET %s?DLGID=%s\0"), systemVars.gprs_conf.serverScript, systemVars.gprs_conf.dlgId );
	if ( systemVars.debug == DEBUG_GPRS ) {
		xprintf_P( PSTR("GPRS: sent>GET %s?DLGID=%s\r\n\0"), systemVars.gprs_conf.serverScript, systemVars.gprs_conf.dlgId );
	}

	// Para darle tiempo a vaciar el buffer y que no se corten los datos que se estan trasmitiendo
	// por sobreescribir el gprs_printBuff.
	vTaskDelay( (portTickType)( 250 / portTICK_RATE_MS ) );

}
//------------------------------------------------------------------------------------
static void pv_trasmitir_dataTail( void )
{
	// TAIL : No mando el close ya que espero la respuesta del server

	//pub_gprs_flush_RX_buffer();

	// TAIL ( No mando el close ya que espero la respuesta y no quiero que el socket se cierre )
	xCom_printf_P( fdGPRS, PSTR(" HTTP/1.1\r\nHost: www.spymovil.com\r\n\r\n\r\n\0") );
	vTaskDelay( (portTickType)( 250 / portTICK_RATE_MS ) );
	// DEBUG & LOG
	if ( systemVars.debug ==  DEBUG_GPRS ) {
		xprintf_P( PSTR(" HTTP/1.1\r\nHost: www.spymovil.com\r\n\r\n\r\n\0") );
	}

}
//------------------------------------------------------------------------------------
static void pv_trasmitir_dataframe( void )
{
	// Primero recupero los datos de la memoria haciendo el proceso inverso de
	// cuando los grabe en spx_tkData::pv_data_guardar_en_BD

st_dataRecord_t dr;

FAT_t l_fat;
size_t bRead;

	// Paso1: Leo un registro de memoria
	bRead = FF_readRcd( &dr, sizeof(st_dataRecord_t));
	if ( bRead == 0) {
		return;
	}

	FAT_read(&l_fat);

	// Inversamente a cuando en tkData guarde en memoria, convierto el datarecord a dataframe
	// Copio al dr solo los campos que correspondan
	switch ( spx_io_board ) {
	case SPX_IO5CH:
		memcpy( &df.ainputs, &dr.df.io5.ainputs, ( NRO_ANINPUTS * sizeof(float)));
		memcpy( &df.dinputsA, &dr.df.io5.dinputsA, ( NRO_DINPUTS * sizeof(uint16_t)));
		memcpy( &df.counters, &dr.df.io5.counters, ( NRO_COUNTERS * sizeof(float)));
		df.range = dr.df.io5.range;
		df.battery = dr.df.io5.battery;
		memcpy( &df.rtc, &dr.rtc, sizeof(RtcTimeType_t) );
		break;
	case SPX_IO8CH:
		memcpy( &df.ainputs, &dr.df.io8.ainputs, ( NRO_ANINPUTS * sizeof(float)));
		memcpy( &df.dinputsA, &dr.df.io8.dinputsA, ( NRO_DINPUTS * sizeof(uint8_t)));
		memcpy( &df.dinputsB, &dr.df.io8.dinputsB, ( NRO_DINPUTS * sizeof(uint16_t)));
		memcpy( &df.counters, &dr.df.io8.counters, ( NRO_COUNTERS * sizeof(float)));
		memcpy( &df.rtc, &dr.rtc, sizeof(RtcTimeType_t) );
		break;
	default:
		return;
	}

	// Paso2: Armo el frame
	// Siempre trasmito los datos aunque vengan papasfritas.
	//pub_gprs_flush_RX_buffer();
	xCom_printf_P( fdGPRS,PSTR("&CTL=%d&LINE=%04d%02d%02d,%02d%02d%02d\0"), l_fat.rdPTR, dr.rtc.year, dr.rtc.month, dr.rtc.day, dr.rtc.hour, dr.rtc.min, dr.rtc.sec );
	// DEBUG & LOG
	if ( systemVars.debug ==  DEBUG_GPRS ) {
		xprintf_P( PSTR("GPRS: sent> CTL=%d&LINE=%04d%02d%02d,%02d%02d%02d\0"), l_fat.rdPTR, dr.rtc.year, dr.rtc.month, dr.rtc.day, dr.rtc.hour, dr.rtc.min, dr.rtc.sec );
	}

	pv_transmitir_df_analogicos();
	pv_transmitir_df_digitales();
	pv_transmitir_df_contadores();
	pv_transmitir_df_range();
	pv_transmitir_df_bateria();

	// DEBUG & LOG
	if ( systemVars.debug ==  DEBUG_GPRS ) {
		xprintf_P(PSTR( "\r\n"));
	}

	vTaskDelay( (portTickType)( 250 / portTICK_RATE_MS ) );

}
//------------------------------------------------------------------------------------
static void pv_transmitir_df_analogicos( void )
{
uint8_t channel;

	// Canales analogicos: Solo muestro los que tengo configurados.
	for ( channel = 0; channel < NRO_ANINPUTS; channel++) {
		if ( ! strcmp ( systemVars.ainputs_conf.name[channel], "X" ) )
			continue;

		xCom_printf_P( fdGPRS, PSTR(",%s=%.02f"),systemVars.ainputs_conf.name[channel], df.ainputs[channel] );
		// DEBUG & LOG
		if ( systemVars.debug ==  DEBUG_GPRS ) {
			xprintf_P( PSTR(",%s=%.02f"),systemVars.ainputs_conf.name[channel], df.ainputs[channel] );
		}
	}
}
//------------------------------------------------------------------------------------
static void pv_transmitir_df_digitales( void )
{

uint8_t channel;

	// Canales digitales.
	if ( spx_io_board == SPX_IO5CH ) {
		for ( channel = 0; channel < NRO_DINPUTS; channel++) {
			if ( ! strcmp ( systemVars.dinputs_conf.name[channel], "X" ) )
				continue;

			xCom_printf_P( fdGPRS, PSTR(",%s=%d"), systemVars.dinputs_conf.name[channel], df.dinputsA[channel] );
			// DEBUG & LOG
			if ( systemVars.debug ==  DEBUG_GPRS ) {
				xprintf_P( PSTR(",%s=%d\0"), systemVars.dinputs_conf.name[channel], df.dinputsA[channel] );
			}
		}
		return;
	}

	// Aqui hay que ver si los dtimers estan o no habilitados.
	if ( spx_io_board == SPX_IO8CH ) {
		// Los primeros 4 son dinputs.
		for ( channel = 0; channel < 4; channel++) {
			if ( ! strcmp ( systemVars.dinputs_conf.name[channel], "X" ) )
				continue;

			xCom_printf_P( fdGPRS, PSTR(",%s=%d"), systemVars.dinputs_conf.name[channel], df.dinputsA[channel] );
			// DEBUG & LOG
			if ( systemVars.debug ==  DEBUG_GPRS ) {
				xprintf_P( PSTR(",%s=%d\0"), systemVars.dinputs_conf.name[channel], df.dinputsA[channel] );
			}
		}

		// Del 4 al 7 pueden ser dinputs o dtimers
		for ( channel = 4; channel < 8; channel++) {
			if ( ! strcmp ( systemVars.dinputs_conf.name[channel], "X" ) )
				continue;

			xCom_printf_P( fdGPRS, PSTR(",%s=%d"), systemVars.dinputs_conf.name[channel], df.dinputsB[channel - 4] );
			// DEBUG & LOG
			if ( systemVars.debug ==  DEBUG_GPRS ) {
				xprintf_P( PSTR(",%s=%d\0"), systemVars.dinputs_conf.name[channel], df.dinputsB[channel - 4] );
			}
		}
		return;
	}
}
//------------------------------------------------------------------------------------
static void pv_transmitir_df_contadores( void )
{
uint8_t channel;

	// Contadores
	for ( channel = 0; channel < NRO_COUNTERS; channel++) {
		// Si el canal no esta configurado no lo muestro.
		if ( ! strcmp ( systemVars.counters_conf.name[channel], "X" ) )
			continue;

		xCom_printf_P( fdGPRS, PSTR(",%s=%.02f"),systemVars.counters_conf.name[channel], df.counters[channel] );
		// DEBUG & LOG
		if ( systemVars.debug ==  DEBUG_GPRS ) {
			xprintf_P(PSTR(",%s=%.02f"),systemVars.counters_conf.name[channel], df.counters[channel] );
		}
	}
}
//------------------------------------------------------------------------------------
static void pv_transmitir_df_range( void )
{
	// Range
	if ( ( spx_io_board == SPX_IO5CH ) && ( systemVars.rangeMeter_enabled ) ) {
		xCom_printf_P( fdGPRS, PSTR(",DIST=%d"), df.range );
		// DEBUG & LOG
		if ( systemVars.debug ==  DEBUG_GPRS ) {
			xprintf_P(PSTR(",DIST=%d"), df.range );
		}
	}
}
//------------------------------------------------------------------------------------
static void pv_transmitir_df_bateria( void )
{
	// bateria
	if ( spx_io_board == SPX_IO5CH ) {
		xCom_printf_P( fdGPRS, PSTR(",bt=%.02f"),df.battery );
		// DEBUG & LOG
		if ( systemVars.debug ==  DEBUG_GPRS ) {
			xprintf_P(PSTR( ",bt=%.02f"), df.battery );
		}
	}
}
//------------------------------------------------------------------------------------
static bool pv_procesar_respuesta_server(void)
{
	// Me quedo hasta 10s esperando la respuesta del server al paquete de datos.
	// Salgo por timeout, socket cerrado, error del server o respuesta correcta
	// Si la respuesta es correcta, ejecuto la misma

uint8_t timeout;
bool exit_flag = false;

	for ( timeout = 0; timeout < 10; timeout++) {

		vTaskDelay( (portTickType)( 1000 / portTICK_RATE_MS ) );	// Espero 1s

		if ( u_gprs_check_socket_status() != SOCK_OPEN ) {		// El socket se cerro
			exit_flag = false;
			goto EXIT;
		}

		if ( u_gprs_check_response ("</h1>\0")) {	// Recibi una respuesta del server.
			// Log.

			if ( systemVars.debug == DEBUG_GPRS ) {
				u_gprs_print_RX_Buffer();
			} else {
				u_gprs_print_RX_response();
			}

			if ( u_gprs_check_response ("ERROR\0")) {
				// ERROR del server: salgo inmediatamente
				exit_flag = false;
				goto EXIT;
			}

			if ( u_gprs_check_response ("RESET\0")) {
				// El sever mando la orden de resetearse inmediatamente
				pv_process_response_RESET();
			}

			if ( ( spx_io_board == SPX_IO8CH ) && u_gprs_check_response ("OUTS\0")) {
				// El sever mando actualizacion de las salidas
				pv_process_response_DOUTS();
			}

			if ( u_gprs_check_response ("RX_OK\0")) {
				// Datos procesados por el server.
				pv_process_response_OK();
				exit_flag = true;
				goto EXIT;
			}
		}
	}

// Exit:
EXIT:

	return(exit_flag);

}
//------------------------------------------------------------------------------------
static void pv_process_response_RESET(void)
{
	// El server me pide que me resetee de modo de mandar un nuevo init y reconfigurarme

	xprintf_P( PSTR("GPRS: Config RESET...\r\n\0"));

	vTaskDelay( ( TickType_t)( 1000 / portTICK_RATE_MS ) );
	// RESET
	CCPWrite( &RST.CTRL, RST_SWRST_bm );   /* Issue a Software Reset to initilize the CPU */

}
//------------------------------------------------------------------------------------
static uint8_t pv_process_response_OK(void)
{
	// Retorno la cantidad de registros procesados ( y borrados )
	// Recibi un OK del server y el ultimo ID de registro a borrar.
	// Los borro de a uno.

FAT_t l_fat;
uint8_t recds_borrados = 0;

	// Borro los registros.
	while ( pv_check_more_Rcds4Del() ) {

		FF_deleteRcd();
		recds_borrados++;
		FAT_read(&l_fat);

		if ( systemVars.debug == DEBUG_GPRS ) {
			xprintf_P( PSTR("GPRS: mem wrPtr=%d,rdPtr=%d,delPtr=%d,r4wr=%d,r4rd=%d,r4del=%d \r\n\0"), l_fat.wrPTR,l_fat.rdPTR, l_fat.delPTR,l_fat.rcds4wr,l_fat.rcds4rd,l_fat.rcds4del );
		}

	}

	return(recds_borrados);
}
//------------------------------------------------------------------------------------
static void pv_process_response_DOUTS(void)
{
	// Recibi algo del estilo >RX_OK:469:DOUTS=245
	// Es la respuesta del server para activar las salidas en perforaciones o modo remoto.

	// Extraigo el valor de las salidas y las seteo.

char localStr[32];
char *stringp;
char *tk_douts;
char *delim = ",=:><";
char *p;

	p = strstr( (const char *)&pv_gprsRxCbuffer.buffer, "DOUTS");
	if ( p == NULL ) {
		return;
	}

	// Copio el mensaje enviado a un buffer local porque la funcion strsep lo modifica.
	memset(localStr,'\0',32);
	memcpy(localStr,p,sizeof(localStr));

	stringp = localStr;
	tk_douts = strsep(&stringp,delim);	//OUTS
	tk_douts = strsep(&stringp,delim);	// Str. con el valor de las salidas. 0..128

	// Actualizo el status a travez de una funcion propia del modulo de outputs
	doutput_set( atoi( tk_douts ), false );
	//systemVars.doutputs_conf.d_outputs = atoi(tk_douts);

	if ( systemVars.debug == DEBUG_GPRS ) {
		xprintf_P( PSTR("GPRS: processDOUTS\r\n\0"));
	}

}
//------------------------------------------------------------------------------------
static void pv_process_response_POUT(void)
{
	// Recibi algo del estilo >RX_OK:469:POUT=2.3
	// Es la respuesta del server para modificar la presion de referencia en modo PILOTO

char localStr[32];
char *stringp;
char *tk_pout;
char *delim = ",=:><";
char *p;

	p = strstr( (const char *)&pv_gprsRxCbuffer.buffer, "POUT");
	if ( p == NULL ) {
		return;
	}

	// Copio el mensaje enviado a un buffer local porque la funcion strsep lo modifica.
	memset(localStr,'\0',32);
	memcpy(localStr,p,sizeof(localStr));

	stringp = localStr;
	tk_pout = strsep(&stringp,delim);	// POUT
	tk_pout = strsep(&stringp,delim);	// valor de presion a fijar

	// Actualizo el status a travez de una funcion propia del modulo de outputs
	doutputs_config_piloto( tk_pout, NULL, NULL);

	if ( systemVars.debug == DEBUG_GPRS ) {
		xprintf_P( PSTR("GPRS: process POUT\r\n\0"));
	}

}
//------------------------------------------------------------------------------------
static bool pv_check_more_Rcds4Del ( void )
{
	// Devuelve si aun quedan registros para borrar del FS

FAT_t l_fat;

	FAT_read(&l_fat);

	if ( l_fat.rcds4del > 0 ) {
		return(true);
	} else {
		return(false);
	}

}
//------------------------------------------------------------------------------------
