/*
 * spx_tkGprs_utils.c
 *
 *  Created on: 25 de oct. de 2017
 *      Author: pablo
 */

#include "gprs.h"

//------------------------------------------------------------------------------------
void u_gprs_open_socket(void)
{
	// Envio el comando AT para abrir el socket.

	if ( systemVars.debug == DEBUG_GPRS ) {
		xprintf_P( PSTR("GPRS: try to open socket\r\n\0"));
	}

	u_gprs_flush_RX_buffer();
	xCom_printf_P( fdGPRS, PSTR("AT+CIPOPEN=0,\"TCP\",\"%s\",%s\r\n\0"), GPRS_stateVars.server_ip_address, systemVars.gprs_conf.server_tcp_port);
	vTaskDelay( (portTickType)( 1500 / portTICK_RATE_MS ) );

	if ( systemVars.debug == DEBUG_GPRS ) {
		u_gprs_print_RX_Buffer();
	}
}
//------------------------------------------------------------------------------------
void u_gprs_close_socket(void)
{
	// Envio el comando AT para cerrar el socket.

	if ( systemVars.debug == DEBUG_GPRS ) {
		xprintf_P( PSTR("GPRS: try to close socket\r\n\0"));
	}

	u_gprs_flush_RX_buffer();
	// Envio el comando para ver si el socket esta cerrado
	xCom_printf_P( fdGPRS, PSTR("AT+CIPCLOSE=?\r\n\0"));
	vTaskDelay( (portTickType)( 500 / portTICK_RATE_MS ) );

	if ( u_gprs_check_response("+CIPCLOSE: 0,") ) {
		// Socket cerrado.
		return;
	} else {
		u_gprs_flush_RX_buffer();
		// Envio el comando para ver si el socket esta cerrado
		xCom_printf_P( fdGPRS, PSTR("AT+CIPCLOSE=0\r\n\0"));
		vTaskDelay( (portTickType)( 2000 / portTICK_RATE_MS ) );
		if ( systemVars.debug == DEBUG_GPRS ) {
			u_gprs_print_RX_Buffer();
		}
	}

}
//------------------------------------------------------------------------------------
t_socket_status u_gprs_check_socket_status(void)
{
	// El socket esta abierto si el modem esta prendido y
	// el DCD esta en 0.
	// Cuando el modem esta apagado pin_dcd = 0
	// Cuando el modem esta prendido y el socket cerrado pin_dcd = 1
	// Cuando el modem esta prendido y el socket abierto pin_dcd = 0.

uint8_t pin_dcd = 0;
t_socket_status socket_status = SOCK_CLOSED;

	pin_dcd = IO_read_DCD();

	if ( ( GPRS_stateVars.modem_prendido == true ) && ( pin_dcd == 0 ) ){
		socket_status = SOCK_OPEN;
		if ( systemVars.debug == DEBUG_GPRS ) {
			xprintf_P( PSTR("GPRS: sckt is open (dcd=%d)\r\n\0"),pin_dcd);
		}

	} else if ( u_gprs_check_response("Operation not supported")) {
		socket_status = SOCK_ERROR;
		if ( systemVars.debug == DEBUG_GPRS ) {
			xprintf_P( PSTR("GPRS: sckt ERROR\r\n\0"));
		}

	} else if ( u_gprs_check_response("ERROR")) {
		socket_status = SOCK_ERROR;
		if ( systemVars.debug == DEBUG_GPRS ) {
			xprintf_P( PSTR("GPRS: sckt ERROR\r\n\0"));
		}

	} else if ( u_gprs_check_response("+CIPEVENT:")) {
		// El socket no se va a recuperar. Hay que cerrar el net y volver a abrirlo.
		socket_status = SOCK_FAIL;
		if ( systemVars.debug == DEBUG_GPRS ) {
			xprintf_P( PSTR("GPRS: sckt FAIL +CIPEVENT:\r\n\0"));
		}

	} else {
		socket_status = SOCK_CLOSED;
		if ( systemVars.debug == DEBUG_GPRS ) {
			xprintf_P( PSTR("GPRS: sckt is close (dcd=%d)\r\n\0"),pin_dcd);
		}

	}

	return(socket_status);

}
//------------------------------------------------------------------------------------
void u_gprs_load_defaults( char *opt )
{

char l_data[10] = { 0 };

	memcpy(l_data, opt, sizeof(l_data));
	strupr(l_data);

	if ( spx_io_board == SPX_IO8CH ) {
		systemVars.gprs_conf.timerDial = 0;
	} else if ( spx_io_board == SPX_IO5CH ) {
		systemVars.gprs_conf.timerDial = 900;
	}

	if (!strcmp_P( l_data, PSTR("SPY\0"))) {
		snprintf_P( systemVars.gprs_conf.apn, APN_LENGTH, PSTR("SPYMOVIL.VPNANTEL\0") );
		strncpy_P(systemVars.gprs_conf.server_ip_address, PSTR("192.168.0.9\0"),16);

	} else if (!strcmp_P( l_data, PSTR("UTE\0"))) {
		snprintf_P( systemVars.gprs_conf.apn, APN_LENGTH, PSTR("SPYMOVIL.VPNANTEL\0") );
		strncpy_P(systemVars.gprs_conf.server_ip_address, PSTR("192.168.8.13\0"),16);

	} else if (!strcmp_P( l_data, PSTR("OSE\0"))) {
		snprintf_P( systemVars.gprs_conf.apn, APN_LENGTH, PSTR("STG1.VPNANTEL\0") );
		strncpy_P(systemVars.gprs_conf.server_ip_address, PSTR("172.27.0.26\0"),16);

	} else {
		snprintf_P( systemVars.gprs_conf.apn, APN_LENGTH, PSTR("DEFAULT\0") );
		strncpy_P(systemVars.gprs_conf.server_ip_address, PSTR("DEFAULT\0"),16);
	}

	snprintf_P( systemVars.gprs_conf.dlgId, DLGID_LENGTH, PSTR("DEFAULT\0") );
	strncpy_P(systemVars.gprs_conf.serverScript, PSTR("/cgi-bin/PY/spy.py\0"),SCRIPT_LENGTH);
	strncpy_P(systemVars.gprs_conf.server_tcp_port, PSTR("80\0"),PORT_LENGTH	);
    strncpy_P(systemVars.gprs_conf.simpwd, PSTR("DEFAULT\0"),PASSWD_LENGTH);

	// PWRSAVE
	if ( spx_io_board == SPX_IO5CH ) {
		systemVars.gprs_conf.pwrSave.pwrs_enabled = true;
	} else if ( spx_io_board == SPX_IO8CH ) {
		systemVars.gprs_conf.pwrSave.pwrs_enabled = false;
	}

	systemVars.gprs_conf.pwrSave.hora_start.hour = 23;
	systemVars.gprs_conf.pwrSave.hora_start.min = 30;
	systemVars.gprs_conf.pwrSave.hora_fin.hour = 5;
	systemVars.gprs_conf.pwrSave.hora_fin.min = 30;

}
//------------------------------------------------------------------------------------
void u_gprs_rxbuffer_flush(void)
{
	memset( pv_gprsRxCbuffer.buffer, '\0', UART_GPRS_RXBUFFER_LEN);
	pv_gprsRxCbuffer.ptr = 0;
}
//------------------------------------------------------------------------------------
bool u_gprs_check_response( const char *rsp )
{
bool retS = false;

	if ( strstr( pv_gprsRxCbuffer.buffer, rsp) != NULL ) {
		retS = true;
	}
	return(retS);
}
//------------------------------------------------------------------------------------
void u_gprs_print_RX_response(void)
{
	// Imprime la respuesta del server.
	// Utiliza el buffer de RX.
	// Solo muestra el payload, es decir lo que esta entre <h1> y </h1>
	// Todas las respuestas el server las encierra entre ambos tags excepto los errores del apache.

char *start_tag = NULL;
char *end_tag = NULL;

	start_tag = strstr(pv_gprsRxCbuffer.buffer,"<h1>");
	end_tag = strstr(pv_gprsRxCbuffer.buffer, "</h1>");

	if ( ( start_tag != NULL ) && ( end_tag != NULL) ) {
		*end_tag = '\0';	// Para que frene el xprintf_P
		start_tag += 4;
		//xprintf_P ( PSTR("GPRS: rsp>%s\r\n\0"), start_tag );
		xprintf_P( PSTR ("GPRS: rxbuff>\r\n\0"));
		xnprint( start_tag, sizeof(pv_gprsRxCbuffer.buffer) );
		xprintf_P( PSTR ("\r\n[%d]\r\n\0"), pv_gprsRxCbuffer.ptr );
	}
}
//------------------------------------------------------------------------------------
void u_gprs_print_RX_Buffer(void)
{

	// Imprimo todo el buffer local de RX. Sale por \0.
	xprintf_P( PSTR ("GPRS: rxbuff>\r\n\0"));

	// Uso esta funcion para imprimir un buffer largo, mayor al que utiliza xprintf_P. !!!
	xnprint( pv_gprsRxCbuffer.buffer, sizeof(pv_gprsRxCbuffer.buffer) );
	xprintf_P( PSTR ("\r\n[%d]\r\n\0"), pv_gprsRxCbuffer.ptr );
}
//------------------------------------------------------------------------------------
void u_gprs_flush_RX_buffer(void)
{

	frtos_ioctl( fdGPRS,ioctl_UART_CLEAR_RX_BUFFER, NULL);
	u_gprs_rxbuffer_flush();

}
//------------------------------------------------------------------------------------
void u_gprs_flush_TX_buffer(void)
{
	frtos_ioctl( fdGPRS,ioctl_UART_CLEAR_TX_BUFFER, NULL);

}
//------------------------------------------------------------------------------------
void u_gprs_ask_sqe(void)
{
	// Veo la calidad de senal que estoy recibiendo

char csqBuffer[32] = { 0 };
char *ts = NULL;

	// AT+CSQ
	u_gprs_flush_RX_buffer();
	xCom_printf_P( fdGPRS, PSTR("AT+CSQ\r\0"));
	vTaskDelay( (portTickType)( 500 / portTICK_RATE_MS ) );

	if ( systemVars.debug == DEBUG_GPRS ) {
		u_gprs_print_RX_Buffer();
	}

	memcpy(csqBuffer, &pv_gprsRxCbuffer.buffer[0], sizeof(csqBuffer) );
	if ( (ts = strchr(csqBuffer, ':')) ) {
		ts++;
		GPRS_stateVars.csq = atoi(ts);
		GPRS_stateVars.dbm = 113 - 2 * GPRS_stateVars.csq;
	}

	// LOG & DEBUG
	xprintf_P ( PSTR("GPRS: signalQ CSQ=%d,DBM=%d\r\n\0"), GPRS_stateVars.csq, GPRS_stateVars.dbm );

}
//------------------------------------------------------------------------------------
bool u_gprs_modem_prendido(void)
{
	return(GPRS_stateVars.modem_prendido);
}
//------------------------------------------------------------------------------------
void u_gprs_redial(void)
{
	GPRS_stateVars.signal_redial = true;

}
//----------------------------------------------------------------------------------------
void u_gprs_config_timerdial ( char *s_timerdial )
{
	// El timer dial puede ser 0 si vamos a trabajar en modo continuo o mayor a
	// 15 minutos.
	// Es una variable de 32 bits para almacenar los segundos de 24hs.

	while ( xSemaphoreTake( sem_SYSVars, ( TickType_t ) 5 ) != pdTRUE )
		taskYIELD();

	systemVars.gprs_conf.timerDial = atoi(s_timerdial);

	// Controlo que este en los rangos permitidos
	if ( (systemVars.gprs_conf.timerDial > 0) && (systemVars.gprs_conf.timerDial < TDIAL_MIN_DISCRETO ) ) {
		//systemVars.gprs_conf.timerDial = 0;
		//xprintf_P( PSTR("TDIAL warn !! Default to 0. ( continuo TDIAL=0, discreto TDIAL > 900)\r\n\0"));
		xprintf_P( PSTR("TDIAL warn: continuo TDIAL < 900, discreto TDIAL >= 900)\r\n\0"));
	}

	xSemaphoreGive( sem_SYSVars );
	return;
}
//------------------------------------------------------------------------------------
void u_gprs_modem_pwr_off(void)
{
	// Apago la fuente del modem

	IO_clr_GPRS_SW();	// Es un FET que lo dejo cortado
	vTaskDelay( (portTickType)( 100 / portTICK_RATE_MS ) );
	IO_clr_GPRS_PWR();	// Apago la fuente.
	vTaskDelay( (portTickType)( 1000 / portTICK_RATE_MS ) );

}
//------------------------------------------------------------------------------------
void u_gprs_modem_pwr_on(uint8_t pwr_time )
{
	// Prendo la fuente del modem

	IO_clr_GPRS_SW();	// GPRS=0V, PWR_ON pullup 1.8V )
	IO_set_GPRS_PWR();											// Prendo la fuente ( alimento al modem ) HW
	vTaskDelay( (portTickType)( ( 2000 + 2000 * pwr_time) / portTICK_RATE_MS ) );	// Espero 2s que se estabilize la fuente.


}
//------------------------------------------------------------------------------------
void u_gprs_modem_pwr_sw(void)
{
	// Genera un pulso en la linea PWR_SW. Como tiene un FET la senal se invierte.
	// En reposo debe la linea estar en 0 para que el fet flote y por un pull-up del modem
	// la entrada PWR_SW esta en 1.
	// El PWR_ON se pulsa a 0 saturando el fet.
	IO_set_GPRS_SW();	// GPRS_SW = 3V, PWR_ON = 0V.
	vTaskDelay( (portTickType)( 1000 / portTICK_RATE_MS ) );
	IO_clr_GPRS_SW();	// GPRS_SW = 0V, PWR_ON = pullup, 1.8V

}
//------------------------------------------------------------------------------------
void u_gprs_configPwrSave( char *s_modo, char *s_startTime, char *s_endTime)
{
	// Recibe como parametros el modo ( 0,1) y punteros a string con las horas de inicio y fin del pwrsave
	// expresadas en minutos.

	while ( xSemaphoreTake( sem_SYSVars, ( TickType_t ) 5 ) != pdTRUE )
		taskYIELD();

	if (!strcmp_P( strupr(s_modo), PSTR( "OFF"))) {
		systemVars.gprs_conf.pwrSave.pwrs_enabled = false;
		goto quit;
	}

	if (!strcmp_P( strupr(s_modo), PSTR( "ON"))) {
		systemVars.gprs_conf.pwrSave.pwrs_enabled = true;
		if ( s_startTime != NULL ) { u_convert_str_to_time_t( s_startTime, &systemVars.gprs_conf.pwrSave.hora_start); }
		if ( s_endTime != NULL ) { u_convert_str_to_time_t( s_endTime, &systemVars.gprs_conf.pwrSave.hora_fin); }
		goto quit;
	}

quit:

	xSemaphoreGive( sem_SYSVars );

}
//------------------------------------------------------------------------------------
void u_gprs_init_pines(void)
{
	// GPRS
	IO_config_GPRS_SW();
	IO_config_GPRS_PWR();
	IO_config_GPRS_RTS();
	IO_config_GPRS_CTS();
	IO_config_GPRS_DCD();
	IO_config_GPRS_RI();
	IO_config_GPRS_RX();
	IO_config_GPRS_TX();

}
//------------------------------------------------------------------------------------
bool u_gprs_send_frame( t_frames frame_type )
{
	// Intento enviar 1 SOLO frame de init.
	// El socket puede estar cerrado por lo que reintento abrirlo hasta 3 veces.
	// Una vez que envie el INIT, salgo.
	// Al entrar, veo que el socket este cerrado.

uint8_t intentos = 0;
bool exit_flag = false;
uint8_t timeout = 0;
uint8_t await_loops = 0;
t_socket_status socket_status = 0;

	for ( intentos = 0; intentos < MAX_TRYES_OPEN_SOCKET; intentos++ ) {

		socket_status = u_gprs_check_socket_status();

		if (  socket_status == SOCK_OPEN ) {

			switch ( frame_type ) {
			case INIT_FRAME_A:
				u_gprs_send_INIT_A_frame();		// Escribo en el socket el frame de INIT
				return(true);
				break;
			case INIT_FRAME_B:
				u_gprs_send_INIT_B_frame();
				return(true);
				break;
			case SCAN_FRAME:
				u_gprs_send_SCAN_frame();		// Escribo en el socket el frame de SCAN
				return(true);
				break;
			}
		}

		// Doy el comando para abrirlo y espero
		u_gprs_open_socket();

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

			// Si el socket dio falla, debo reiniciar la conexion.
			if ( socket_status == SOCK_FAIL ) {
				return(exit_flag);
				break;
			}
		}
	}

	return(exit_flag);
}
//------------------------------------------------------------------------------------
void u_gprs_send_INIT_A_frame(void)
{

	// Send Init Frame
	// GET /cgi-bin/spx/SPY.pl?DLGID=TEST02&SIMPWD=DEFAULT&IMEI=860585004367917&VER=0.0.6.R1&UID=304632333433180f000500&SIMID=895980161423091055&INIT&TPOLL=300&TDIAL=900&PWRS=O
	// N,2330,0530&CSQ=25&WRST=0x33&A0=A0,0,20,0.00,6.00&A1=A1,0,20,0.00,6.00&A2=A2,0,20,0.00,6.00&A3=A3,0,20,0.00,6.00&A4=A4,0,20,0.00,6.00&D0=D0&D1=D1&C0=C0,0.10&C1=C1,0.10&CONS=OFF HTTP/1.1
	// Host: www.spymovil.com

	// GET /cgi-bin/spx/SPY.pl?DLGID=SPY001&SIMPWD=spymovil123&&INIT&PWRM=CONT&TPOLL=23&TDIAL=234&PWRS=1,1230,2045&A0=pZ,1,20,3,10&D0=qE,3.24&CONS=1,1234,927,1,3 HTTP/1.1
	// Host: www.spymovil.com
	// Connection: close\r\r ( no mando el close )

uint8_t i = 0;


	if ( systemVars.debug == DEBUG_GPRS  ) {
		xprintf_P( PSTR("GPRS: initframe A: Sent\r\n\0"));
	}

	// Trasmision: 1r.Parte.
	// HEADER:---------------------------------------------------------------------
	// Envio parcial ( no CR )
	u_gprs_flush_RX_buffer();
	u_gprs_flush_TX_buffer();

	// dlgid, simpwd,imei,uid
	xCom_printf_P( fdGPRS,PSTR("GET %s?DLGID=%s&SIMPWD=%s&IMEI=%s&VER=%s&UID=%s&SIMID=%s\0" ), systemVars.gprs_conf.serverScript, systemVars.gprs_conf.dlgId, systemVars.gprs_conf.simpwd, &buff_gprs_imei, SPX_FW_REV, NVMEE_readID(), &buff_gprs_ccid );
	// DEBUG & LOG
	if ( systemVars.debug ==  DEBUG_GPRS ) {
		xprintf_P( PSTR("GET %s?DLGID=%s&SIMPWD=%s&IMEI=%s&VER=%s&UID=%s&SIMID=%s\0" ), systemVars.gprs_conf.serverScript, systemVars.gprs_conf.dlgId, systemVars.gprs_conf.simpwd, &buff_gprs_imei, SPX_FW_REV, NVMEE_readID(), &buff_gprs_ccid );
	}

	// timerpoll,timerdial
	xCom_printf_P( fdGPRS, PSTR("&INIT&TPOLL=%d&TDIAL=%d\0"), systemVars.timerPoll,systemVars.gprs_conf.timerDial);
	// DEBUG & LOG
	if ( systemVars.debug == DEBUG_GPRS ) {
		xprintf_P( PSTR("&INIT&TPOLL=%d&TDIAL=%d\0"), systemVars.timerPoll,systemVars.gprs_conf.timerDial );
	}
	// pwrSave
	if ( systemVars.gprs_conf.pwrSave.pwrs_enabled == true ) {
		xCom_printf_P( fdGPRS, PSTR("&PWRS=ON,%02d%02d,%02d%02d\0"), systemVars.gprs_conf.pwrSave.hora_start.hour, systemVars.gprs_conf.pwrSave.hora_start.min, systemVars.gprs_conf.pwrSave.hora_fin.hour, systemVars.gprs_conf.pwrSave.hora_fin.min );
		// DEBUG & LOG
		if ( systemVars.debug == DEBUG_GPRS ) {
			xprintf_P( PSTR("&PWRS=ON,%02d%02d,%02d%02d\0"), systemVars.gprs_conf.pwrSave.hora_start.hour, systemVars.gprs_conf.pwrSave.hora_start.min, systemVars.gprs_conf.pwrSave.hora_fin.hour, systemVars.gprs_conf.pwrSave.hora_fin.min );
		}
	} else {
		xCom_printf_P( fdGPRS, PSTR("&PWRS=OFF,%02d%02d,%02d%02d\0"), systemVars.gprs_conf.pwrSave.hora_start.hour, systemVars.gprs_conf.pwrSave.hora_start.min, systemVars.gprs_conf.pwrSave.hora_fin.hour, systemVars.gprs_conf.pwrSave.hora_fin.min );
		// DEBUG & LOG
		if ( systemVars.debug == DEBUG_GPRS ) {
			xprintf_P( PSTR("&PWRS=OFF,%02d%02d,%02d%02d\0"), systemVars.gprs_conf.pwrSave.hora_start.hour, systemVars.gprs_conf.pwrSave.hora_start.min, systemVars.gprs_conf.pwrSave.hora_fin.hour, systemVars.gprs_conf.pwrSave.hora_fin.min );
		}
	}

	// csq, wrst
	xCom_printf_P( fdGPRS, PSTR("&CSQ=%d&WRST=0x%02X\0"), GPRS_stateVars.dbm, wdg_resetCause );
	// DEBUG & LOG
	if ( systemVars.debug == DEBUG_GPRS ) {
		xprintf_P( PSTR("&CSQ=%d&WRST=0x%02X\0"), GPRS_stateVars.csq, wdg_resetCause );
	}

	// PARAMETROS-------------------------------------------------------------------
	// BODY ( 2a parte) : Configuracion de canales
	// Configuracion de canales analogicos
	for ( i = 0; i < NRO_ANINPUTS; i++) {
		// No trasmito los canales que estan con X ( apagados )
		if (!strcmp_P( systemVars.ainputs_conf.name[i], PSTR("X"))) {
			continue;
		}
		xCom_printf_P( fdGPRS,PSTR("&A%d=%s,%d,%d,%.02f,%.02f\0"), i, systemVars.ainputs_conf.name[i], systemVars.ainputs_conf.imin[i], systemVars.ainputs_conf.imax[i], systemVars.ainputs_conf.mmin[i], systemVars.ainputs_conf.mmax[i] );
		// DEBUG & LOG
		if ( systemVars.debug ==  DEBUG_GPRS ) {
			xprintf_P( PSTR("&A%d=%s,%d,%d,%.02f,%.02f\0"), i, systemVars.ainputs_conf.name[i], systemVars.ainputs_conf.imin[i], systemVars.ainputs_conf.imax[i], systemVars.ainputs_conf.mmin[i], systemVars.ainputs_conf.mmax[i] );
		}
	}

	// Configuracion de canales digitales
	for (i = 0; i < NRO_DINPUTS; i++) {
		// No trasmito los canales que estan con X ( apagados )
		if (!strcmp_P( systemVars.dinputs_conf.name[i], PSTR("X"))) {
			continue;
		}
		xCom_printf_P( fdGPRS,PSTR("&D%d=%s,%d\0"),i, systemVars.dinputs_conf.name[i],systemVars.dinputs_conf.tpoll[i] );
		// DEBUG & LOG
		if ( systemVars.debug ==  DEBUG_GPRS ) {
			xprintf_P( PSTR("&D%d=%s,%d\0"),i, systemVars.dinputs_conf.name[i],systemVars.dinputs_conf.tpoll[i] );
		}
	}

	// Configuracion de canales contadores
	for (i = 0; i < NRO_COUNTERS; i++) {
		// No trasmito los canales que estan con X ( apagados )
		if (!strcmp_P( systemVars.counters_conf.name[i], PSTR("X"))) {
			continue;
		}
		if ( systemVars.counters_conf.speed[i] == CNT_LOW_SPEED ) {
			xCom_printf_P( fdGPRS,PSTR("&C%d=%s,%.03f,%d,%d,LS\0"),i, systemVars.counters_conf.name[i],systemVars.counters_conf.magpp[i],systemVars.counters_conf.pwidth[i], systemVars.counters_conf.period[i] );
			// DEBUG & LOG
			if ( systemVars.debug ==  DEBUG_GPRS ) {
				xprintf_P( PSTR("&C%d=%s,%.03f,%d,%d,LS\0"),i, systemVars.counters_conf.name[i], systemVars.counters_conf.magpp[i],systemVars.counters_conf.pwidth[i], systemVars.counters_conf.period[i] );
			}
		} else {
			xCom_printf_P( fdGPRS,PSTR("&C%d=%s,%.03f,%d,%d,HS\0"),i, systemVars.counters_conf.name[i],systemVars.counters_conf.magpp[i],systemVars.counters_conf.pwidth[i], systemVars.counters_conf.period[i] );
			// DEBUG & LOG
			if ( systemVars.debug ==  DEBUG_GPRS ) {
				xprintf_P( PSTR("&C%d=%s,%.03f,%d,%d,HS\0"),i, systemVars.counters_conf.name[i], systemVars.counters_conf.magpp[i],systemVars.counters_conf.pwidth[i], systemVars.counters_conf.period[i] );
			}
		}
	}

	if ( spx_io_board == SPX_IO5CH ) {

		// Configuracion del rangeMeter
		if ( systemVars.rangeMeter_enabled ) {
			xCom_printf_P( fdGPRS,PSTR("&DIST=ON\0"));
			if ( systemVars.debug ==  DEBUG_GPRS ) {
				xprintf_P( PSTR("&DIST=ON\0"));
			}

		} else {
			xCom_printf_P( fdGPRS,PSTR("&DIST=OFF\0"));
			if ( systemVars.debug ==  DEBUG_GPRS ) {
				xprintf_P( PSTR("&DIST=OFF\0"));
			}
		}

		// Configuracion del psensor
		if ( strcmp ( systemVars.psensor_conf.name, "X" ) != 0 ) {
			xCom_printf_P( fdGPRS,PSTR("&PSENSOR=%s,%d,%d,%.01f,%.01f,%.01f\0"),systemVars.psensor_conf.name, systemVars.psensor_conf.count_min, systemVars.psensor_conf.count_max,systemVars.psensor_conf.pmin, systemVars.psensor_conf.pmax, systemVars.psensor_conf.offset );

			if ( systemVars.debug ==  DEBUG_GPRS ) {
				xprintf_P( PSTR("&PSENSOR=%s,%d,%d,%.01f,%.01f,%.01f\0"),systemVars.psensor_conf.name, systemVars.psensor_conf.count_min, systemVars.psensor_conf.count_max,systemVars.psensor_conf.pmin, systemVars.psensor_conf.pmax, systemVars.psensor_conf.offset );
			}
		}

	}

	// doutputs
	switch( systemVars.doutputs_conf.modo) {
	case OFF:
		xCom_printf_P( fdGPRS, PSTR("&DOUT=OFF\0"));
		// DEBUG & LOG
		if ( systemVars.debug == DEBUG_GPRS ) {
			xprintf_P( PSTR("&DOUT=OFF\0"));
		}
		break;
	case CONSIGNA:
		// Consignas
		xCom_printf_P( fdGPRS, PSTR("&DOUT=CONS\0"));
		// DEBUG & LOG
		if ( systemVars.debug == DEBUG_GPRS ) {
			xprintf_P( PSTR("&DOUT=CONS\0"));
		}
		//
		// CONSIGNA
		xCom_printf_P( fdGPRS, PSTR("&CSGNA=%02d%02d,%02d%02d\0"), systemVars.doutputs_conf.consigna.hhmm_c_diurna.hour,systemVars.doutputs_conf.consigna.hhmm_c_diurna.min,systemVars.doutputs_conf.consigna.hhmm_c_nocturna.hour,systemVars.doutputs_conf.consigna.hhmm_c_nocturna.min);
		// DEBUG & LOG
		if ( systemVars.debug == DEBUG_GPRS ) {
			xprintf_P( PSTR("&CSGNA=%02d%02d,%02d%02d\0"), systemVars.doutputs_conf.consigna.hhmm_c_diurna.hour,systemVars.doutputs_conf.consigna.hhmm_c_diurna.min,systemVars.doutputs_conf.consigna.hhmm_c_nocturna.hour,systemVars.doutputs_conf.consigna.hhmm_c_nocturna.min);
		}
		break;
	case PERFORACIONES:
		xCom_printf_P( fdGPRS, PSTR("&DOUT=PERF\0"));
		// DEBUG & LOG
		if ( systemVars.debug == DEBUG_GPRS ) {
			xprintf_P( PSTR("&DOUT=PERF\0"));
		}
		break;
	case PILOTOS:
		xCom_printf_P( fdGPRS, PSTR("&DOUT=PLT\0"));
		if ( systemVars.debug == DEBUG_GPRS ) {
			xprintf_P( PSTR("&DOUT=PLT\0"));
		}
		break;
	}

	// TAIL ------------------------------------------------------------------------
	// ( No mando el close ya que espero la respuesta y no quiero que el socket se cierre )
	xCom_printf_P( fdGPRS, PSTR(" HTTP/1.1\r\nHost: www.spymovil.com\r\n\r\n\r\n\0") );

	// DEBUG & LOG
	if ( systemVars.debug ==  DEBUG_GPRS ) {
		xprintf_P( PSTR(" HTTP/1.1\r\nHost: www.spymovil.com\r\n\r\n\r\n\0") );
	}

	vTaskDelay( (portTickType)( 250 / portTICK_RATE_MS ) );


}
//------------------------------------------------------------------------------------
void u_gprs_send_INIT_B_frame(void)
{

	// Send Init Frame
	// GET /cgi-bin/spx/SPY.pl?DLGID=TEST02&IAUX0&PBAND=0.2&PSTEPS=6&S0=hhmm0,p0&S1=hhmm1,p1&S2=hhmm2,p2&S3=hhmm3,p3&S4=hhmm4,p4 HTTP/1.1
	// Host: www.spymovil.com

uint8_t i = 0;

	if ( systemVars.debug == DEBUG_GPRS  ) {
		xprintf_P( PSTR("GPRS: initframe B: Sent\r\n\0"));
	}

	// Trasmision: 1r.Parte.
	// HEADER:---------------------------------------------------------------------
	// Envio parcial ( no CR )
	u_gprs_flush_RX_buffer();
	u_gprs_flush_TX_buffer();

	// dlgid, simpwd,imei,uid
	xCom_printf_P( fdGPRS,PSTR("GET %s?IAUX0&DLGID=%s&PBAND=%.02f&PSTEPS=%d\0" ), systemVars.gprs_conf.serverScript, systemVars.gprs_conf.dlgId, systemVars.doutputs_conf.piloto.band, systemVars.doutputs_conf.piloto.max_steps );

	if ( systemVars.debug ==  DEBUG_GPRS ) {
		xprintf_P( PSTR("GET %s?IAUX0&DLGID=%s&PBAND=%.02f&PSTEPS=%d\0" ), systemVars.gprs_conf.serverScript, systemVars.gprs_conf.dlgId, systemVars.doutputs_conf.piloto.band, systemVars.doutputs_conf.piloto.max_steps );
	}

	// Slots
	for ( i = 0; i < MAX_PILOTO_PSLOTS; i++ ) {

		xCom_printf_P( fdGPRS, PSTR("&S%d=%02d%02d,%.02f\0"), i, systemVars.doutputs_conf.piloto.pSlots[i].hhmm.hour, systemVars.doutputs_conf.piloto.pSlots[i].hhmm.min, systemVars.doutputs_conf.piloto.pSlots[i].pout );
		// DEBUG & LOG
		if ( systemVars.debug == DEBUG_GPRS ) {
			xprintf_P( PSTR("&S%d=%d%02d,%.02f\0"), i, systemVars.doutputs_conf.piloto.pSlots[i].hhmm.hour, systemVars.doutputs_conf.piloto.pSlots[i].hhmm.min, systemVars.doutputs_conf.piloto.pSlots[i].pout );
		}
	}

	// TAIL ------------------------------------------------------------------------
	// ( No mando el close ya que espero la respuesta y no quiero que el socket se cierre )
	xCom_printf_P( fdGPRS, PSTR(" HTTP/1.1\r\nHost: www.spymovil.com\r\n\r\n\r\n\0") );

	// DEBUG & LOG
	if ( systemVars.debug ==  DEBUG_GPRS ) {
		xprintf_P( PSTR(" HTTP/1.1\r\nHost: www.spymovil.com\r\n\r\n\r\n\0") );
	}

	vTaskDelay( (portTickType)( 250 / portTICK_RATE_MS ) );


}
//------------------------------------------------------------------------------------
void u_gprs_send_SCAN_frame(void)
{
	// GET /cgi-bin/spx/SPY.pl?SCAN&UID=abcd45367 HTTP/1.1
	// Host: www.spymovil.com
	// Connection: close\r\r ( no mando el close )

	xprintf_P( PSTR("GPRS_SCAN:: scanframe: Sent\r\n\0"));

	u_gprs_flush_RX_buffer();
	u_gprs_flush_TX_buffer();
	//
	xCom_printf_P( fdGPRS, PSTR("GET %s?SCAN&DLGID=%s&UID=%s HTTP/1.1\r\nHost: www.spymovil.com\r\n\r\n\r\n\0" ), systemVars.gprs_conf.serverScript, systemVars.gprs_conf.dlgId, NVMEE_readID() );
	// DEBUG & LOG
	if ( systemVars.debug ==  DEBUG_GPRS ) {
		xprintf_P(  PSTR("GET %s?SCAN&DLGID=%s&UID=%s HTTP/1.1\r\nHost: www.spymovil.com\r\n\r\n\r\n\0" ), systemVars.gprs_conf.serverScript,systemVars.gprs_conf.dlgId, NVMEE_readID() );
	}

	vTaskDelay( (portTickType)( 250 / portTICK_RATE_MS ) );

}
//------------------------------------------------------------------------------------
uint32_t u_gprs_read_timeToNextDial(void)
{
	return( ctl_read_timeToNextDial() );
}
//------------------------------------------------------------------------------------
void u_gprs_set_timeToNextDial( uint32_t time_to_dial )
{
	ctl_set_timeToNextDial( time_to_dial );
}
//------------------------------------------------------------------------------------
bool u_gprs_modem_link_up(void)
{
	if ( GPRS_stateVars.state == G_DATA ) {
		return(true);
	}

	return(false);

}
//------------------------------------------------------------------------------------


