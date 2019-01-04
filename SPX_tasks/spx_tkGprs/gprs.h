/*
 * spx_tkGprs.h
 *
 *  Created on: 25 de oct. de 2017
 *      Author: pablo
 */

#ifndef SRC_SPXR3_TKGPRS_SPXR3_TKGPRS_H_
#define SRC_SPXR3_TKGPRS_SPXR3_TKGPRS_H_

#include "frtos-io.h"
#include "spx.h"

#define MAX_HW_TRIES_PWRON 		3	// Intentos de prender HW el modem
#define MAX_SW_TRIES_PWRON 		3	// Intentos de prender SW el modem
#define MAX_TRYES_NET_ATTCH		6	// Intentos de atachearme a la red GPRS
#define MAX_IP_QUERIES 			4	// Intentos de conseguir una IP
#define MAX_INIT_TRYES			4	// Intentos de procesar un frame de INIT
#define MAX_TRYES_OPEN_SOCKET	4 	// Intentos de abrir un socket
#define MAX_RCDS_WINDOW_SIZE	8	// Maximos registros enviados en un bulk de datos
#define MAX_TX_WINDOW_TRYES		4	// Intentos de enviar el mismo paquete de datos

// Datos del buffer local de recepcion de datos del GPRS.
// Es del mismo tamanio que el ringBuffer asociado a la uart RX.
// Es lineal, no ringBuffer !!! ( para poder usar las funciones de busqueda de strings )
#define UART_GPRS_RXBUFFER_LEN GPRS_RXSTORAGE_SIZE
struct {
	char buffer[UART_GPRS_RXBUFFER_LEN];
	uint16_t ptr;
} pv_gprsRxCbuffer;

#define IMEIBUFFSIZE	24
char buff_gprs_imei[IMEIBUFFSIZE];
char buff_gprs_ccid[IMEIBUFFSIZE];

int32_t waiting_time;

typedef enum { G_ESPERA_APAGADO = 0, G_PRENDER, G_CONFIGURAR, G_MON_SQE, G_GET_IP, G_INIT_FRAME, G_DATA } t_gprs_states;
typedef enum { SOCK_CLOSED = 0, SOCK_OPEN, SOCK_ERROR, SOCK_FAIL } t_socket_status;
typedef enum { INIT_ERROR = 0, INIT_SOCK_CLOSE, INIT_OK, INIT_NOT_ALLOWED } t_init_responses;

struct {
	bool modem_prendido;
	bool signal_redial;
	bool signal_frameReady;
	bool monitor_sqe;
	uint8_t state;
	uint8_t	dcd;
	//uint8_t inits_counter;
	uint8_t csq;
	uint8_t dbm;
	char dlg_ip_address[IP_LENGTH];

} GPRS_stateVars;

#define bool_CONTINUAR	true
#define bool_RESTART	false

bool st_gprs_esperar_apagado(void);
bool st_gprs_prender(void);
bool st_gprs_configurar(void);
bool st_gprs_monitor_sqe(void);
bool st_gprs_get_ip(void);
bool st_gprs_init_frame(void);
bool st_gprs_data(void);

void u_gprs_rxbuffer_flush(void);
bool u_gprs_check_response( const char *rsp );
void u_gprs_modem_pwr_off(void);
void u_gprs_modem_pwr_on(uint8_t pwr_time );
void u_gprs_modem_pwr_sw(void);
void u_gprs_flush_RX_buffer(void);
void u_gprs_flush_TX_buffer(void);
void u_gprs_open_socket(void);
t_socket_status u_gprs_check_socket_status(void);
void u_gprs_print_RX_response(void);
void u_gprs_print_RX_Buffer(void);
void u_gprs_ask_sqe(void);
int32_t u_gprs_readTimeToNextDial(void);
void u_gprs_redial(void);
void u_gprs_config_timerdial ( char *s_timerdial );
void u_gprs_modem_pwr_off(void);
void u_gprs_modem_pwr_on(uint8_t pwr_time );
void u_gprs_modem_pwr_sw(void);
void u_gprs_configPwrSave( char *s_modo, char *s_startTime, char *s_endTime);
bool u_gprs_modem_prendido(void);
void u_gprs_load_defaults(void);
void u_gprs_init_pines(void);
bool u_gprs_modem_prendido(void);


#endif /* SRC_SPXR3_TKGPRS_SPXR3_TKGPRS_H_ */
