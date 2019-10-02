/*
 * spx_tkXbee.c
 *
 *  Created on: 20 set. 2019
 *      Author: pablo
 *
 *  Ver el estado del FS cuando se transmite un dato.
 *  Generalizar con tkGPRS las tareas de leer de memoria y chequear status
 *
 */

#include "spx.h"

// La tarea puede estar hasta 25s en standby
#define WDG_XBEE_TIMEOUT	60
#define XBEE_RX_BUFFER_LEN 128

#define TO_1MIN		600
#define TO_10MIN	6000
#define TO_30S		300
#define TO_15S		150
#define TO_10S		100

static uint8_t xbee_buffer[XBEE_RX_BUFFER_LEN];
static uint8_t xbee_ptr = 0;

#define TIMER_GPRS_QUERY 	0
#define TIMER_PING 			1
#define TIMER_DATA 			2
#define NRO_MSG_TIMERS		3

static int16_t msg_timer[NRO_MSG_TIMERS];

// Estructura donde mantengo el status del Xbee.
struct {
	uint8_t nck_counter;
	t_link xbee_link;
	t_link local_gprs_link;
	t_link remote_gprs_link;
	bool flag_frame_for_gprs_tx;
	bool flag_ack_for_gprs;

} xbee_status;

void xbee_init(void);
void xbee_process_rx_frame(void);
void xbee_state_link_up(void);
void xbee_state_link_down(void);

bool xbee_rx_ACK(void);
bool xbee_rx_ack(void);
bool xbee_rx_NCK(void);
bool xbee_rx_GPRS_QUERY(void);
bool xbee_rx_GPRS_IS_ON(void);
bool xbee_rx_GPRS_IS_OFF(void);
bool xbee_rx_DATA(void);
bool xbee_rx_data(void);
bool xbee_rx_ping(void);
bool xbee_rx_pong(void);

void xbee_rx_process_ACK(void);
void xbee_rx_process_ack(void);
void xbee_rx_process_NCK(void);
void xbee_rx_process_GPRS_QUERY(void);
void xbee_rx_process_GPRS_IS_ON(void);
void xbee_rx_process_GPRS_IS_OFF(void);
void xbee_rx_process_DATA(void);
void xbee_rx_process_data(void);
void xbee_rx_process_ping(void);
void xbee_rx_process_pong(void);
void xbee_flush_rx_buffer(void);

void xbee_tx_ack(uint16_t frame_id);
void xbee_tx_NCK(void);
void xbee_tx_GPRS_QUERY(void);
void xbee_tx_GPRS_IS_ON(void);
void xbee_tx_GPRS_IS_OFF(void);
void xbee_tx_PAYLOAD(void);
void xbee_tx_ping(void);
void xbee_tx_pong(void);

void xbee_stop_timer(uint8_t timer_id);
void xbee_check_msg_to(void);

//------------------------------------------------------------------------------------
void tkXbee(void * pvParameters)
{

( void ) pvParameters;

	// Espero la notificacion para arrancar
	while ( !startTask )
		vTaskDelay( ( TickType_t)( 100 / portTICK_RATE_MS ) );

	xprintf_P( PSTR("starting tkXbee..\r\n\0"));

	// Prendo el xbee
	IO_set_XBEE_PWR();
	xbee_flush_rx_buffer();
	xbee_status.xbee_link = LINK_DOWN;

	// loop
	for( ;; )
	{

		switch(xbee_status.xbee_link) {
		case LINK_DOWN:
			xbee_state_link_down();
			break;
		case LINK_UP:
			xbee_state_link_up();
			break;
		}

	}
}
//------------------------------------------------------------------------------------
void xbee_init(void)
{

uint8_t i;

	xbee_status.local_gprs_link = LINK_DOWN;
	xbee_status.remote_gprs_link = LINK_DOWN;
	xbee_status.xbee_link = LINK_DOWN;
	xbee_status.nck_counter = 0;

	// Inicializo todos los timers de mensajes.
	for ( i=0; i < NRO_MSG_TIMERS; i++)
		xbee_stop_timer(i);

}
//------------------------------------------------------------------------------------
// XBEE LINK
//------------------------------------------------------------------------------------
void xbee_state_link_down(void)
{
	// Esta funcion implementa el estado con el enlace DOWN del xbee.
	// Cada 1 minuto envio un mensaje de PING.
	// Si el enlace local gprs esta down, c/1 minuto manda un mensaje a la
	// tarea tkGprs para que intente levantarlo.
	// Cada 10 minutos prendo y apago el modulo xbee
	// Cuando está el link xbee down, no importan los TO de los frames.

uint16_t timer_1MIN = TO_1MIN;			// 1 minuto ( 600 * 0.1s )
uint16_t timer_10MIN = TO_10MIN;		// 10 minutos
uint32_t ulNotifiedValue;
BaseType_t xResult;

	// Entry:
	xprintf_P( PSTR("XBEE link down...\r\n\0"));

	xbee_init();

	// Aviso a mi tarea de gprs que no tengo enlace xbee.
	while ( xTaskNotify(xHandle_tkGprsRx, SGN_XBEE_IS_DOWN , eSetBits ) != pdPASS ) {
		vTaskDelay( ( TickType_t)( 100 / portTICK_RATE_MS ) );
	}

	// Loop
	while ( xbee_status.xbee_link == LINK_DOWN )
	{

		ctl_watchdog_kick(WDG_XBEE, WDG_XBEE_TIMEOUT);

		// Espero 100ms
		vTaskDelay( ( TickType_t)( 100 / portTICK_RATE_MS ) );

		// Proceso los frames xbee recibidos.
		xbee_process_rx_frame();

		// Proceso señales recibidas desde otras tareas.
		xResult = xTaskNotifyWait( 0x00, ULONG_MAX, &ulNotifiedValue, ((TickType_t) 1 / portTICK_RATE_MS ) );
		if ( xResult == pdTRUE ) {
			if ( ( ulNotifiedValue & SGN_LOCAL_GPRS_IS_UP ) != 0 ) {
				// tkGprs indica que el enlace esta up
				xbee_status.local_gprs_link = LINK_UP;
			} else  if ( ( ulNotifiedValue & SGN_LOCAL_GPRS_IS_DOWN ) != 0 ) {
				// tkGprs indica que el enlace esta down
				xbee_status.local_gprs_link = LINK_DOWN;
			}
		}

		// Operaciones pautadas por timer
		if ( timer_1MIN-- == 0 ) {
			timer_1MIN = TO_1MIN;
			// Con el link down, c/1minuto mando un ping.
			xbee_tx_ping();

			// Pido el estado del enlace gprs local
			while ( xTaskNotify(xHandle_tkGprsRx, SGN_GPRS_QUERY , eSetBits ) != pdPASS ) {
				vTaskDelay( ( TickType_t)( 100 / portTICK_RATE_MS ) );
			}

			// Si mi enlace GPRS esta down, mando una señal para que se levante.
			if ( xbee_status.local_gprs_link == LINK_DOWN ) {
				while ( xTaskNotify(xHandle_tkGprsRx, SGN_PRENDER_GPRS , eSetBits ) != pdPASS ) {
					vTaskDelay( ( TickType_t)( 100 / portTICK_RATE_MS ) );
				}
			}
		}

		// Cada 10 minutos apago y prendo el modulo xbee para que se reinicialize
		if ( timer_10MIN-- == 0 ) {
			timer_10MIN = TO_10MIN;
			IO_clr_XBEE_PWR();
			vTaskDelay( ( TickType_t)( 1000 / portTICK_RATE_MS ) );
			IO_set_XBEE_PWR();
		}

	}

	// Exit

}
//------------------------------------------------------------------------------------
void xbee_state_link_up(void)
{

	// Esta tarea monitorea el estado del enlace Xbee UP.
	// Debo controlar el link gprs local, el remoto y el enlace Xbee.
	// Para monitorear el link gprs local, c/1 minuto mando un query a la tkGprs.
	// Para monitorear el link xbee mandamos un ping c/1min, y para monitorear el
	// gprs remoto mandamos un gprs_query cada 1 min.
	// Aqui importa controlar el TO de los frames enviados ya que una expiración
	// de un TO indica un problema.

uint16_t timer_1m = TO_1MIN;			// 1 minuto ( 600 * 0.1s )
uint32_t ulNotifiedValue;
BaseType_t xResult;

	// Entry:
	xprintf_P( PSTR("XBEE link up...\r\n\0"));

	xbee_init();

	// Aviso a mi tarea de gprs que el enlace xbee esta activo.
	while ( xTaskNotify(xHandle_tkGprsRx, SGN_XBEE_IS_UP , eSetBits ) != pdPASS ) {
		vTaskDelay( ( TickType_t)( 100 / portTICK_RATE_MS ) );
	}

	// Loop
	while ( xbee_status.xbee_link == LINK_UP )
	{

		ctl_watchdog_kick(WDG_XBEE, WDG_XBEE_TIMEOUT);

		// Espero 100ms
		vTaskDelay( ( TickType_t)( 100 / portTICK_RATE_MS ) );

		// Controlo los TO de los frames enviados.
		xbee_check_msg_to();

		// Proceso los frames xbee recibidos.
		xbee_process_rx_frame();

		// Proceso señales recibidas desde otras tareas.
		xResult = xTaskNotifyWait( 0x00, ULONG_MAX, &ulNotifiedValue, ((TickType_t) 1 / portTICK_RATE_MS ) );
		if ( xResult == pdTRUE ) {
			if ( ( ulNotifiedValue & SGN_LOCAL_GPRS_IS_UP ) != 0 ) {
				// tkGprs indica que el enlace esta up
				xbee_status.local_gprs_link = LINK_UP;
			} else  if ( ( ulNotifiedValue & SGN_LOCAL_GPRS_IS_DOWN ) != 0 ) {
				// tkGprs indica que el enlace esta down
				xbee_status.local_gprs_link = LINK_DOWN;
			}
		}

		// Funciones pautadas por el timer.
		if ( timer_1m-- == 0 ) {
			timer_1m = TO_1MIN;

			// Pido el estado del enlace gprs local
			while ( xTaskNotify(xHandle_tkGprsRx, SGN_GPRS_QUERY , eSetBits ) != pdPASS ) {
				vTaskDelay( ( TickType_t)( 100 / portTICK_RATE_MS ) );
			}

			// Monitoreo el enlace Xbee
			xbee_tx_ping();

			// Monitoreo el GPRS remoto
			xbee_tx_GPRS_QUERY();
		}

	}
	// Exit


}
//------------------------------------------------------------------------------------
// FUNCIONES DE LA subTASK de RECEPCION
//------------------------------------------------------------------------------------
void xbee_process_rx_frame(void)
{
	// Aqui realizamos la tarea de RX.
	// Almaceno el dato en el buffer local de recepcion
	// Si tengo una fin de linea proceso.
	// Si tengo un buffer overflow lo reseteo.

uint8_t c;

	// RX
	// Recibo datos del xbee y cuando tengo un frame lo trasmito por el gprs
	// el read se bloquea 50ms. lo que genera la espera.
	c = '\0';	// Lo borro para que luego del un CR no resetee siempre el timer.
	while ( frtos_read( fdTERM, (char *)&c, 1 ) == 1 ) {
		// Almaceno el dato en un buffer local
		xbee_buffer[xbee_ptr++] = c;

		// Controlo overflow
		if ( xbee_ptr == XBEE_RX_BUFFER_LEN ) {
			xbee_ptr = 0;
			memset(xbee_buffer, '\0', XBEE_RX_BUFFER_LEN);
			if ( systemVars.debug == DEBUG_XBEE) {
				xprintf_P( PSTR("XBEE rx_buffer_overflow\r\n\0"));
			}
			return;
		}

		// Analizo si llego un buffer completo. Los frames terminan en \n.
		if ( c != '\n') {
			return;
		}
	}

	// Tengo un frame xbee completo: lo analizo.
	if ( systemVars.debug == DEBUG_XBEE) {
		xprintf_P( PSTR("XBEE RX: %s\r\n\0"),xbee_buffer);
	}

	// Analizo que tengo y voy procesando.

	if ( xbee_rx_ACK() ) {
		xbee_rx_process_ACK();
		return;
	}

	if ( xbee_rx_ack() ) {
		xbee_rx_process_ack();
		return;
	}

	if ( xbee_rx_NCK() ) {
		xbee_rx_process_NCK();
		return;
	}

	if ( xbee_rx_GPRS_QUERY() ) {
		xbee_rx_process_GPRS_QUERY();
		return;
	}

	if ( xbee_rx_GPRS_IS_ON() ) {
		xbee_rx_process_GPRS_IS_ON();
		return;
	}

	if ( xbee_rx_GPRS_IS_OFF() ) {
		xbee_rx_process_GPRS_IS_OFF();
		return;
	}

	if ( xbee_rx_DATA() ) {
		xbee_rx_process_DATA();
		return;
	}

	if ( xbee_rx_data() ) {
		xbee_rx_process_data();
		return;
	}

	if ( xbee_rx_ping() ) {
		xbee_rx_process_ping();
		return;
	}

	if ( xbee_rx_pong() ) {
		xbee_rx_process_pong();
		return;
	}
}
//------------------------------------------------------------------------------------
bool xbee_rx_ACK(void)
{
	if ( !strncmp_P( (char *)xbee_buffer, PSTR("ACK"), XBEE_RX_BUFFER_LEN )) {
		return(true);
	} else {
		return(false);
	}
}
//------------------------------------------------------------------------------------
bool xbee_rx_ack(void)
{
	if ( !strncmp_P( (char *)xbee_buffer, PSTR("ack"), XBEE_RX_BUFFER_LEN )) {
		return(true);
	} else {
		return(false);
	}
}
//------------------------------------------------------------------------------------
bool xbee_rx_NCK(void)
{
	if ( !strncmp_P( (char *)xbee_buffer, PSTR("NCK"), XBEE_RX_BUFFER_LEN )) {
		return(true);
	} else {
		return(false);
	}
}
//------------------------------------------------------------------------------------
bool xbee_rx_GPRS_QUERY(void)
{
	if ( !strncmp_P( (char *)xbee_buffer, PSTR("GPRS_QUERY"), XBEE_RX_BUFFER_LEN )) {
		return(true);
	} else {
		return(false);
	}
}
//------------------------------------------------------------------------------------
bool xbee_rx_GPRS_IS_ON(void)
{
	if ( !strncmp_P( (char *)xbee_buffer, PSTR("GPRS_IS_ON"), XBEE_RX_BUFFER_LEN )) {
		return(true);
	} else {
		return(false);
	}
}
//------------------------------------------------------------------------------------
bool xbee_rx_GPRS_IS_OFF(void)
{
	if ( !strncmp_P( (char *)xbee_buffer, PSTR("GPRS_IS_OFF"), XBEE_RX_BUFFER_LEN )) {
		return(true);
	} else {
		return(false);
	}
}
//------------------------------------------------------------------------------------
bool xbee_rx_DATA(void)
{
	if ( !strncmp_P( (char *)xbee_buffer, PSTR("DATA"), XBEE_RX_BUFFER_LEN )) {
		return(true);
	} else {
		return(false);
	}
}
//------------------------------------------------------------------------------------
bool xbee_rx_data(void)
{
	if ( !strncmp_P( (char *)xbee_buffer, PSTR("data"), XBEE_RX_BUFFER_LEN )) {
		return(true);
	} else {
		return(false);
	}
}
//------------------------------------------------------------------------------------
bool xbee_rx_ping(void)
{
	if ( !strncmp_P( (char *)xbee_buffer, PSTR("PING"), XBEE_RX_BUFFER_LEN )) {
		return(true);
	} else {
		return(false);
	}
}
//------------------------------------------------------------------------------------
bool xbee_rx_pong(void)
{
	if ( !strncmp_P( (char *)xbee_buffer, PSTR("PONG"), XBEE_RX_BUFFER_LEN )) {
		return(true);
	} else {
		return(false);
	}
}
//------------------------------------------------------------------------------------
// FUNCIONES DE PROCESO de mensajes recibidos
//------------------------------------------------------------------------------------
void xbee_rx_process_ACK(void)
{
	// Recibi un frame de ACK indicando que el frame de datos enviado fue recibido
	// por el servidor.
	// Este frame puede traer informacion adicional para interpretar
	// Como la trasmision la disparo tkGprs, le aviso a esta que tengo el ACK.
	// Este debera leerlo y procesarlo ( similar al recibir un DATA)
	//
	// xsrc:OSE107,xtype:CTL,xmsg:{ACK,CTL:51}\n

uint16_t timeout = TO_10S;

	if ( systemVars.debug == DEBUG_XBEE ) {
		xprintf_P( PSTR("XBEE process_ACK\r\n\0"));
	}

	// Este frame apaga el timer de TO de mensajes
	xbee_stop_timer(TIMER_DATA);

	// Aviso a tkGPRS que tengo un ACK
	xbee_status.flag_ack_for_gprs = true;
	while ( xTaskNotify(xHandle_tkGprsRx, SGN_XBEE_ACK , eSetBits ) != pdPASS ) {
		vTaskDelay( ( TickType_t)( 100 / portTICK_RATE_MS ) );
	}

	// Espero si los pudo transmitir.
	while ( ( timeout-- > 0) || !xbee_status.flag_ack_for_gprs ) {
		vTaskDelay( ( TickType_t)( 100 / portTICK_RATE_MS ) );
	}

	if ( timeout == 0 ) {
		// Pasaron 10s y no pude procesar el ACK. ERROR !!!
		xprintf_P( PSTR("XBEE process_ACK. ERROR INTERNO !!!\r\n\0"));
	}

	// Dejo la flag reseteada.
	xbee_status.flag_ack_for_gprs = false;
	xbee_status.nck_counter = 0;
	xbee_flush_rx_buffer();


}
//------------------------------------------------------------------------------------
void xbee_rx_process_ack(void)
{
	// Recibi un frame de ack indicando que el frame enviado fue recibido por el nodo
	// remoto.
	// Este frame fue del tipo 'data' por lo tanto no reenviado al server.
	// No debo borrarlo de memoria porque aún no llego al server.
	// Este ack es local por lo tanto no puedo actualizar el estado del link del gprs remoto.
	//
	// xsrc:OSE107,xtype:CLT,xmsg:{ack,CTL:51}\n

	// Este frame apaga el timer de TO de mensajes
	xbee_stop_timer(TIMER_DATA);

	if ( systemVars.debug == DEBUG_XBEE ) {
		xprintf_P( PSTR("XBEE process_ack\r\n\0"));
	}

	// Reseteo el contador de nack's.
	xbee_status.nck_counter = 0;
	xbee_flush_rx_buffer();
}
//------------------------------------------------------------------------------------
void xbee_rx_process_NCK(void)
{
	// El nodo remoto respondio que no pudo mandar el frame de 'DATA' al server.
	// No borro el frame de memoria.
	// Luego de 5 nck seguidos asumo que se cayo el enlace gprs remoto.
	//
	// xsrc:OSE107,xtype:CTL,xmsg:{NCK}\n

	// Este frame apaga el timer de TO de mensajes
	xbee_stop_timer(TIMER_DATA);

	if ( systemVars.debug == DEBUG_XBEE ) {
		xprintf_P( PSTR("XBEE process_NCK\0"));
	}

	// Monitoreo el estado del enlace gprs remoto.
	if ( xbee_status.nck_counter++== 5 ) {
		xbee_status.nck_counter = 0;
		xbee_status.remote_gprs_link = LINK_DOWN;
	}

	xbee_flush_rx_buffer();
}
//------------------------------------------------------------------------------------
void xbee_rx_process_GPRS_QUERY(void)
{
	// El nodo remoto me pregunta el estado del enlace GPRS local.
	//
	// xsrc:OSE107,xtype:CTL,xmsg:{GPRS_QUERY}\n

	if ( systemVars.debug == DEBUG_XBEE ) {
		xprintf_P( PSTR("XBEE process_GPRS_QUERY\r\n\0"));
	}

	if ( xbee_status.local_gprs_link == LINK_UP ) {
		xbee_tx_GPRS_IS_ON();
	} else {
		xbee_tx_GPRS_IS_OFF();
	}

	xbee_flush_rx_buffer();
}
//------------------------------------------------------------------------------------
void xbee_rx_process_GPRS_IS_ON(void)
{
	// Me avisa el nodo remoto que tiene el enlace GPRS activo.

	// Este frame apaga el timer de TO de mensajes
	xbee_stop_timer(TIMER_GPRS_QUERY);

	xbee_status.remote_gprs_link = LINK_UP;
	xbee_flush_rx_buffer();
}
//------------------------------------------------------------------------------------
void xbee_rx_process_GPRS_IS_OFF(void)
{
	//  Me avisa el nodo remoto que tiene el enlace GPRS in-activo.

	// Este frame apaga el timer de TO de mensajes
	xbee_stop_timer(TIMER_GPRS_QUERY);

	xbee_status.remote_gprs_link = LINK_DOWN;
	xbee_flush_rx_buffer();
}
//------------------------------------------------------------------------------------
void xbee_rx_process_DATA(void)
{
	// Recibi un frame de 'DATA' que debo reenviar por GPRS al server
	// El frame esta en xbee_buffer. Mientras no salga de esta funcion
	// esta protegido.
	// Marco una flag que solo luego de trasmitir, tkGprs la apaga y le
	// mando una señal a tkGprs.
	// Espero hasta 15s que lo transmita.

uint16_t timeout = TO_15S;

	if ( systemVars.debug == DEBUG_XBEE ) {
		xprintf_P( PSTR("XBEE process_DATA\r\n\0"));
	}

	// Aviso a tkGPRS que tengo datos para mandar.
	xbee_status.flag_frame_for_gprs_tx = true;
	while ( xTaskNotify(xHandle_tkGprsRx, SGN_XBEE_FRAME_READY , eSetBits ) != pdPASS ) {
		vTaskDelay( ( TickType_t)( 100 / portTICK_RATE_MS ) );
	}

	// Espero si los pudo transmitir.
	while ( ( timeout-- > 0) || !xbee_status.flag_frame_for_gprs_tx ) {
		vTaskDelay( ( TickType_t)( 100 / portTICK_RATE_MS ) );
	}

	if ( timeout == 0 ) {
		// Pasaron 15 y no pude enviar el frame.
		xbee_tx_NCK();
	}

	// Dejo la flag reseteada.
	xbee_status.flag_frame_for_gprs_tx = false;
	xbee_flush_rx_buffer();
}
//------------------------------------------------------------------------------------
void pub_xbee_clear_txframe_flag(void)
{
	xbee_status.flag_frame_for_gprs_tx = false;
}
//------------------------------------------------------------------------------------
char *pub_xbee_get_buffer_ptr(void)
{
	// Funcion peligrosa que le da a tkGprs el puntero del buffer de datos
	// de xbee para poder transmitirlo
	return((char*)&xbee_buffer);

}
//------------------------------------------------------------------------------------
void xbee_rx_process_data(void)
{
	// Recibi un frame de 'data' que debo procesar localmente
	// y no mandar al server.
	// Por ahora solo respondo con 'ack'
	// Decodifico su id.
	// El frame recibido es del tipo:
	//
	// xsrc:OSE107,xtype:data,xmsg:{CTL:51,FECHA:190926,HORA:123450,pA:3.45,q0:21.3,BOMBA1:1}\n

char *p = NULL;
char localStr[32] = { 0 };
char *stringp = NULL;
char *token = NULL;
char *delim = "{:,";
int16_t frame_id = -1;


	p = strstr( (const char *)&xbee_buffer, "CTL");
	if ( p == NULL ) {
		return;
	}

	// Copio el mensaje enviado a un buffer local porque la funcion strsep lo modifica.
	memset( &localStr, '\0', sizeof(localStr) );
	memset(localStr,'\0',32);
	memcpy(localStr,p,sizeof(localStr));

	stringp = localStr;
	token = strsep(&stringp,delim);	// CTL
	token = strsep(&stringp,delim);	// frame_id
	frame_id = atoi(token);

	if ( systemVars.debug == DEBUG_XBEE ) {
		xprintf_P( PSTR("XBEE process_data\r\n\0"));
	}

	xbee_tx_ack(frame_id);
	xbee_flush_rx_buffer();
}
//------------------------------------------------------------------------------------
void xbee_rx_process_ping(void)
{
	// Al recibir un ping debo responder con un pong
	//
	// xsrc:OSE107,xtype:CTL,xmsg:{PING}\n

	if ( systemVars.debug == DEBUG_XBEE ) {
		xprintf_P( PSTR("XBEE process_PING\0"));
	}

	xbee_tx_pong();
	xbee_flush_rx_buffer();
}
//------------------------------------------------------------------------------------
void xbee_rx_process_pong(void)
{
	// Al recibir un pong marco el enlace XBEE como UP
	//
	// xsrc:OSE107,xtype:CTL,xmsg:{PONG}\n

	if ( systemVars.debug == DEBUG_XBEE ) {
		xprintf_P( PSTR("XBEE process_PONG\0"));
	}

	// Este frame apaga el timer de TO de mensajes
	xbee_stop_timer(TIMER_PING);

	xbee_status.xbee_link = LINK_UP;
	xbee_flush_rx_buffer();
}
//------------------------------------------------------------------------------------
void xbee_flush_rx_buffer(void)
{
	xbee_ptr = 0;
	memset(xbee_buffer, '\0', XBEE_RX_BUFFER_LEN);
}
//------------------------------------------------------------------------------------
// FUNCIONES DE LA subTASK de TRASMISION
//------------------------------------------------------------------------------------
void pub_xbee_tx_ACK( char *msg )
{
	// Transmito un ACK.
	// El ACK lo decodifico del payload del ack recibido por el GPRS por lo tanto
	// lo debe transmitir tkGprs cuando lo tiene, por eso esta funcion es publica.


	xCom_printf_P( fdXBEE, "%s", msg );
	if ( systemVars.debug == DEBUG_XBEE ) {
		xprintf_P( PSTR("XBEE: %s"),msg);
	}
}
//------------------------------------------------------------------------------------
void xbee_tx_ack(uint16_t frame_id)
{
	while ( xSemaphoreTake( sem_XBEE, ( TickType_t ) 5 ) != pdTRUE )
		taskYIELD();

	xCom_printf_P( fdXBEE, PSTR("xsrc:%s,xtype:CTL,xmsq:{ack,CTL=%d}\n"),systemVars.gprs_conf.dlgId,frame_id);

	xSemaphoreGive( sem_XBEE );

	if (systemVars.debug == DEBUG_XBEE ) {
		xprintf_P( PSTR("XBEE: xsrc:%s,xtype:CTL,xmsq:{ack,CTL=%d}\n"),systemVars.gprs_conf.dlgId,frame_id);
	}
}
//------------------------------------------------------------------------------------
void xbee_tx_NCK(void)
{
	while ( xSemaphoreTake( sem_XBEE, ( TickType_t ) 5 ) != pdTRUE )
		taskYIELD();

	xCom_printf_P( fdXBEE, PSTR("xsrc:%s,xtype:CTL,xmsq:{NCK}\n"),systemVars.gprs_conf.dlgId);

	xSemaphoreGive( sem_XBEE );

	if (systemVars.debug == DEBUG_XBEE ) {
		xprintf_P( PSTR("XBEE: xsrc:%s,xtype:CTL,xmsq:{NCK}\n"),systemVars.gprs_conf.dlgId);
	}
}
//------------------------------------------------------------------------------------
void xbee_tx_GPRS_QUERY(void)
{

	// Envio el frame
	while ( xSemaphoreTake( sem_XBEE, ( TickType_t ) 5 ) != pdTRUE )
		taskYIELD();

	xCom_printf_P( fdXBEE, PSTR("xsrc:%s,xtype:CTL,xmsq:{GPRS_QUERY}\n"),systemVars.gprs_conf.dlgId);

	xSemaphoreGive( sem_XBEE );

	// Activo el timer
	msg_timer[TIMER_GPRS_QUERY] = TO_30S;

	if (systemVars.debug == DEBUG_XBEE ) {
		xprintf_P( PSTR("XBEE: xsrc:%s,xtype:CTL,xmsq:{GPRS_QUERY}\n"),systemVars.gprs_conf.dlgId);
	}
}
//------------------------------------------------------------------------------------
void xbee_tx_GPRS_IS_ON(void)
{
	while ( xSemaphoreTake( sem_XBEE, ( TickType_t ) 5 ) != pdTRUE )
		taskYIELD();

	xCom_printf_P( fdXBEE, PSTR("xsrc:%s,xtype:CTL,xmsq:{GPRS_IS_ON}\n"),systemVars.gprs_conf.dlgId);

	xSemaphoreGive( sem_XBEE );

	if (systemVars.debug == DEBUG_XBEE ) {
		xprintf_P( PSTR("XBEE: xsrc:%s,xtype:CTL,xmsq:{GPRS_IS_ON}\n"),systemVars.gprs_conf.dlgId);
	}
}
//------------------------------------------------------------------------------------
void xbee_tx_GPRS_IS_OFF(void)
{
	while ( xSemaphoreTake( sem_XBEE, ( TickType_t ) 5 ) != pdTRUE )
		taskYIELD();

	xCom_printf_P( fdXBEE, PSTR("xsrc:%s,xtype:CTL,xmsq:{GPRS_IS_OFF}\n"),systemVars.gprs_conf.dlgId);

	xSemaphoreGive( sem_XBEE );

	if (systemVars.debug == DEBUG_XBEE ) {
		xprintf_P( PSTR("XBEE: xsrc:%s,xtype:CTL,xmsq:{GPRS_IS_OFF}\n"),systemVars.gprs_conf.dlgId);
	}
}
//------------------------------------------------------------------------------------
void pub_xbee_tx_DATAFRAME(void)
{
	// Esta función publica se usa desde otros modulos para mandar un frame por el canal xbee.
	// Aqui se determina si el frame debe ser DATA o data.

	while ( xSemaphoreTake( sem_XBEE, ( TickType_t ) 5 ) != pdTRUE )
		taskYIELD();

	if ( ( xbee_status.local_gprs_link == LINK_UP ) && ( xbee_status.xbee_link == LINK_UP) ) {
		switch( systemVars.xbee ) {
		case XBEE_MASTER:
			// Mando frame DATA
			xCom_printf_P( fdXBEE, PSTR("xsrc:%s,xtype:DATA,xmsq:{"),systemVars.gprs_conf.dlgId);
			if (systemVars.debug == DEBUG_XBEE ) {
				xprintf_P( PSTR("XBEE: xsrc:%s,xtype:DATA,xmsq:{"),systemVars.gprs_conf.dlgId);
			}
			break;
		case XBEE_SLAVE:
			// Mando frame data
			xCom_printf_P( fdXBEE, PSTR("xsrc:%s,xtype:data,xmsq:{"),systemVars.gprs_conf.dlgId);
			if (systemVars.debug == DEBUG_XBEE ) {
				xprintf_P( PSTR("XBEE: xsrc:%s,xtype:data,xmsq:{"),systemVars.gprs_conf.dlgId);
			}
			break;
		}
	}

	xbee_tx_PAYLOAD();

	xCom_printf_P( fdXBEE, PSTR("}\n"));
	if (systemVars.debug == DEBUG_XBEE ) {
		xprintf_P( PSTR("}\n"));
	}

	xSemaphoreGive( sem_XBEE );

	// Activo el timer
	msg_timer[TIMER_DATA] = TO_30S;
}
//------------------------------------------------------------------------------------
void xbee_tx_PAYLOAD(void)
{
	// Trasmite los datos relevados por xbee.

FAT_t fat;
st_dataRecord_t dr;
dataframe_s df;
int8_t bRead;
uint8_t channel = 0;

	FF_rewind();
	memset ( &fat, '\0', sizeof(FAT_t));
	memset ( &df, '\0', sizeof( st_dataRecord_t));

	// Paso1: Leo un registro de memoria
	bRead = FF_readRcd( &dr, sizeof(st_dataRecord_t));
	if ( bRead == 0) {
		return;
	}

	FAT_read(&fat);

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
		memcpy( &df.counters, &dr.df.io8.counters, ( NRO_COUNTERS * sizeof(float)));
		memcpy( &df.rtc, &dr.rtc, sizeof(RtcTimeType_t) );
		break;
	default:
		return;
	}

	xCom_printf_P( fdXBEE, PSTR("&CTL=%d&LINE=%04d%02d%02d,%02d%02d%02d\0"), fat.rdPTR, dr.rtc.year, dr.rtc.month, dr.rtc.day, dr.rtc.hour, dr.rtc.min, dr.rtc.sec );
	if ( systemVars.debug ==  DEBUG_XBEE ) {
		xprintf_P( PSTR("&CTL=%d&LINE=%04d%02d%02d,%02d%02d%02d\0"), fat.rdPTR, dr.rtc.year, dr.rtc.month, dr.rtc.day, dr.rtc.hour, dr.rtc.min, dr.rtc.sec );
	}
	// Canales analogicos: Solo muestro los que tengo configurados.
	for ( channel = 0; channel < NRO_ANINPUTS; channel++) {
		if ( ! strcmp ( systemVars.ainputs_conf.name[channel], "X" ) )
			continue;

		xCom_printf_P( fdXBEE, PSTR(",%s=%.02f"),systemVars.ainputs_conf.name[channel], df.ainputs[channel] );
		// DEBUG & LOG
		if ( systemVars.debug ==  DEBUG_XBEE ) {
			xprintf_P( PSTR(",%s=%.02f"),systemVars.ainputs_conf.name[channel], df.ainputs[channel] );
		}
	}

	// Canales digitales.
	for ( channel = 0; channel < NRO_DINPUTS; channel++) {
		if ( ! strcmp ( systemVars.dinputs_conf.name[channel], "X" ) )
			continue;

		xCom_printf_P( fdXBEE, PSTR(",%s=%d"), systemVars.dinputs_conf.name[channel], df.dinputsA[channel] );
		// DEBUG & LOG
		if ( systemVars.debug ==  DEBUG_XBEE ) {
			xprintf_P( PSTR(",%s=%d\0"), systemVars.dinputs_conf.name[channel], df.dinputsA[channel] );
		}
	}

	// Contadores
	for ( channel = 0; channel < NRO_COUNTERS; channel++) {
		// Si el canal no esta configurado no lo muestro.
		if ( ! strcmp ( systemVars.counters_conf.name[channel], "X" ) )
			continue;

		xCom_printf_P( fdXBEE, PSTR(",%s=%.02f"),systemVars.counters_conf.name[channel], df.counters[channel] );
		// DEBUG & LOG
		if ( systemVars.debug ==  DEBUG_XBEE ) {
			xprintf_P(PSTR(",%s=%.02f"),systemVars.counters_conf.name[channel], df.counters[channel] );
		}
	}

	// Range
	if ( ( spx_io_board == SPX_IO5CH ) && ( systemVars.rangeMeter_enabled ) ) {
		xCom_printf_P( fdXBEE, PSTR(",DIST=%d"), df.range );
		// DEBUG & LOG
		if ( systemVars.debug ==  DEBUG_XBEE ) {
			xprintf_P(PSTR(",DIST=%d"), df.range );
		}
	}

	// bateria
	if ( spx_io_board == SPX_IO5CH ) {
		xCom_printf_P( fdXBEE, PSTR(",bt=%.02f"), df.battery );
		// DEBUG & LOG
		if ( systemVars.debug ==  DEBUG_XBEE ) {
			xprintf_P(PSTR( ",bt=%.02f"), df.battery );
		}
	}

	vTaskDelay( (portTickType)( 250 / portTICK_RATE_MS ) );

}
//------------------------------------------------------------------------------------
void xbee_tx_ping(void)
{
	// Envio el frame
	while ( xSemaphoreTake( sem_XBEE, ( TickType_t ) 5 ) != pdTRUE )
		taskYIELD();

	xCom_printf_P( fdXBEE, PSTR("xsrc:%s,xtype:CTL,xmsq:{PING}\n"),systemVars.gprs_conf.dlgId);

	xSemaphoreGive( sem_XBEE );

	// Activo el timer
	msg_timer[TIMER_PING] = TO_30S;

	if (systemVars.debug == DEBUG_XBEE ) {
		xprintf_P( PSTR("XBEE: xsrc:%s,xtype:CTL,xmsq:{PING}\n"),systemVars.gprs_conf.dlgId);
	}
}
//------------------------------------------------------------------------------------
void xbee_tx_pong(void)
{
	while ( xSemaphoreTake( sem_XBEE, ( TickType_t ) 5 ) != pdTRUE )
		taskYIELD();

	xCom_printf_P( fdXBEE, PSTR("xsrc:%s,xtype:CTL,xmsq:{PONG}\n"),systemVars.gprs_conf.dlgId);

	xSemaphoreGive( sem_XBEE );

	if (systemVars.debug == DEBUG_XBEE ) {
		xprintf_P( PSTR("XBEE: xsrc:%s,xtype:CTL,xmsq:{PONG}\n"),systemVars.gprs_conf.dlgId);
	}
}
//------------------------------------------------------------------------------------
// USO GENERAL
//------------------------------------------------------------------------------------
void xbee_stop_timer(uint8_t timer_id)
{
	// Apaga el timer
	msg_timer[timer_id] = -1;
}
//------------------------------------------------------------------------------------
void xbee_check_msg_to(void)
{
	// Esta función se ejecuta solo cuando el link xbee esta UP.
	// Cuando se manda un frame se arranca un timer el cual aqui se controla y si
	// expira se debe interpretar.
	// La funcion se ejecuta c/0.1s por lo tanto para llegar a 1s contamos hasta 10.

uint8_t i;

	// Actualizo los timers.
	for (i=0; i<NRO_MSG_TIMERS;i++) {
		if (msg_timer[i] > 0 ) {
			msg_timer[i]--;
		}
	}

	// Interpreto los timers.
	if ( msg_timer[TIMER_GPRS_QUERY] == 0 ) {
		// Enlace Xbee off.
		xbee_status.xbee_link = LINK_DOWN;
	}

	if ( msg_timer[TIMER_PING] == 0 ) {
		// Enlace Xbee off.
		xbee_status.xbee_link = LINK_DOWN;
	}

	if ( msg_timer[TIMER_DATA] == 0 ) {
		// Enlace gprs remoto off
		xbee_status.remote_gprs_link = LINK_DOWN;
	}

}
//------------------------------------------------------------------------------------
