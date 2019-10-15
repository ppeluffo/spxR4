/*
 * spx.h
 *
 *  Created on: 8 dic. 2018
 *      Author: pablo
 */

#ifndef SRC_SPX_H_
#define SRC_SPX_H_

//------------------------------------------------------------------------------------
// INCLUDES
//------------------------------------------------------------------------------------
#include <avr/io.h>
#include <avr/wdt.h>
#include <stdlib.h>
#include <avr/interrupt.h>
#include <compat/deprecated.h>
#include <avr/pgmspace.h>
#include <stdarg.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <avr/sleep.h>
#include <ctype.h>
#include "avr_compiler.h"
#include "clksys_driver.h"
#include <inttypes.h>

#include "TC_driver.h"
#include "pmic_driver.h"
#include "wdt_driver.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "list.h"
#include "croutine.h"
#include "semphr.h"
#include "timers.h"
#include "limits.h"

#include "frtos-io.h"
#include "FRTOS-CMD.h"

#include "l_counters.h"
#include "l_drv8814.h"
#include "l_iopines.h"
#include "l_eeprom.h"
#include "l_file.h"
#include "l_i2c.h"
#include "l_ina3221.h"
#include "l_iopines.h"
#include "l_rtc79410.h"
#include "l_nvm.h"
#include "l_printf.h"
#include "l_rangeMeter.h"
#include "l_bytes.h"
#include "l_psensor.h"

//------------------------------------------------------------------------------------
// DEFINES
//------------------------------------------------------------------------------------
#define SPX_FW_REV "2.0.6"
#define SPX_FW_DATE "@ 20191015"

#define SPX_HW_MODELO "spxR4 HW:xmega256A3B R1.1"
#define SPX_FTROS_VERSION "FW:FRTOS10 TICKLESS"

//#define F_CPU (32000000UL)

//#define SYSMAINCLK 2
//#define SYSMAINCLK 8
#define SYSMAINCLK 32
//
#define MAX_ANALOG_CHANNELS		8
#define MAX_DINPUTS_CHANNELS	8
#define MAX_DOUTPUTS_CHANNELS	8
#define MAX_COUNTER_CHANNELS	2

#define IO5_ANALOG_CHANNELS		5
#define IO5_DINPUTS_CHANNELS	2
#define IO5_COUNTER_CHANNELS	2

#define IO8_ANALOG_CHANNELS		8
#define IO8_DINPUTS_CHANNELS	8
#define IO8_COUNTER_CHANNELS	2
#define IO8_DOUTPUTS_CHANNELS	8

#define MAX_PILOTO_PSLOTS		5

#define CHAR32	32
#define CHAR64	64
#define CHAR128	128
#define CHAR256	256

#define tkCtl_STACK_SIZE		512
#define tkCmd_STACK_SIZE		512
#define tkCounter_STACK_SIZE	512
#define tkData_STACK_SIZE		512
#define tkDoutputs_STACK_SIZE	512
#define tkGprs_rx_STACK_SIZE	1024
#define tkGprs_tx_STACK_SIZE	1024
#define tkDinputs_STACK_SIZE	512

#define tkCtl_TASK_PRIORITY	 		( tskIDLE_PRIORITY + 1 )
#define tkCmd_TASK_PRIORITY	 		( tskIDLE_PRIORITY + 1 )
#define tkCounter_TASK_PRIORITY	 	( tskIDLE_PRIORITY + 1 )
#define tkData_TASK_PRIORITY	 	( tskIDLE_PRIORITY + 1 )
#define tkDoutputs_TASK_PRIORITY	( tskIDLE_PRIORITY + 1 )
#define tkGprs_rx_TASK_PRIORITY		( tskIDLE_PRIORITY + 1 )
#define tkGprs_tx_TASK_PRIORITY		( tskIDLE_PRIORITY + 1 )
#define tkDinputs_TASK_PRIORITY		( tskIDLE_PRIORITY + 1 )

#define TDIAL_MIN_DISCRETO 900

#define MODO_DISCRETO ( (systemVars.gprs_conf.timerDial >= TDIAL_MIN_DISCRETO ) ? true : false )

// Mensajes entre tareas
#define TK_FRAME_READY			0x01	//

typedef enum { DEBUG_NONE = 0, DEBUG_COUNTER, DEBUG_DATA, DEBUG_GPRS, DEBUG_OUTPUTS, DEBUG_PILOTO } t_debug;
typedef enum { USER_NORMAL, USER_TECNICO } usuario_t;
typedef enum { SPX_IO5CH = 0, SPX_IO8CH } ioboard_t;
typedef enum { CONSIGNA_OFF = 0, CONSIGNA_DIURNA, CONSIGNA_NOCTURNA } consigna_t;
typedef enum { modoPWRSAVE_OFF = 0, modoPWRSAVE_ON } t_pwrSave;
typedef enum { CTL_BOYA, CTL_SISTEMA } doutputs_control_t;
typedef enum { OFF = 0, CONSIGNA, PERFORACIONES, PILOTOS  } doutputs_modo_t;
typedef enum { CNT_LOW_SPEED = 0, CNT_HIGH_SPEED  } dcounters_modo_t;
typedef enum { OPEN = 0, CLOSE } t_valve_status;
typedef enum { VR_CHICA = 0, VR_MEDIA, VR_GRANDE } t_valvula_reguladora;

TaskHandle_t xHandle_idle, xHandle_tkCtl, xHandle_tkCmd, xHandle_tkCounter0 ,xHandle_tkCounter1 , xHandle_tkData, xHandle_tkDoutputs, xHandle_tkDinputs, xHandle_tkGprsRx, xHandle_tkGprsTx;

bool startTask;
uint8_t spx_io_board;

xSemaphoreHandle sem_SYSVars;
StaticSemaphore_t SYSVARS_xMutexBuffer;
#define MSTOTAKESYSVARSSEMPH ((  TickType_t ) 10 )

xSemaphoreHandle sem_WDGS;
StaticSemaphore_t WDGS_xMutexBuffer;
#define MSTOTAKEWDGSSEMPH ((  TickType_t ) 10 )

xSemaphoreHandle sem_DATA;
StaticSemaphore_t DATA_xMutexBuffer;
#define MSTOTAKEDATASEMPH ((  TickType_t ) 10 )


void tkCtl(void * pvParameters);
void tkCmd(void * pvParameters);
void tkCounter0(void * pvParameters);
void tkCounter1(void * pvParameters);
void tkData(void * pvParameters);
void tkDoutputs(void * pvParameters);
void tkGprsRx(void * pvParameters);
void tkGprsTx(void * pvParameters);
void tkDinputs(void * pvParameters);

#define DLGID_LENGTH		12
#define PARAMNAME_LENGTH	5
#define IP_LENGTH			24
#define APN_LENGTH			32
#define PORT_LENGTH			7
#define SCRIPT_LENGTH		64
#define PASSWD_LENGTH		15
#define PARAMNAME_LENGTH	5

uint8_t NRO_COUNTERS;
uint8_t NRO_ANINPUTS;
uint8_t NRO_DINPUTS;

// Estructura de un registro de IO5CH
typedef struct {
	float ainputs[IO5_ANALOG_CHANNELS];			// 4 * 5 = 20
	uint8_t dinputsA[IO5_DINPUTS_CHANNELS];		// 1 * 2 =  2
	float counters[IO5_COUNTER_CHANNELS];		// 4 * 2 =  8
	int16_t range;								// 2 * 1 =  2
	float psensor;							// 2 * 1 =  2
	float battery;								// 4 * 1 =  4
	uint8_t plt_Vcounters[2];					// 2 * 1 =  2
} st_io5_t;										// ----- = 40

// Estructura de un registro de IO8CH
typedef struct {
	float ainputs[IO8_ANALOG_CHANNELS];			// 4 * 8 = 32
	uint8_t dinputsA[IO8_DINPUTS_CHANNELS];		// 1 * 4 =  4
	float counters[IO8_COUNTER_CHANNELS];		// 4 * 2 =  8
} st_io8_t; 									// ----- = 52

// Estructura de datos comun independiente de la arquitectura de IO
typedef union u_dataframe {
	st_io5_t io5;	// 36
	st_io8_t io8;	// 52
} u_dataframe_t;	// 52

typedef struct {
	u_dataframe_t df;	// 52
	RtcTimeType_t rtc;	//  7
} st_dataRecord_t;		// 59

// Estructura de datos de un dataframe ( considera todos los canales que se pueden medir )
typedef struct {
	float ainputs[MAX_ANALOG_CHANNELS];
	uint8_t dinputsA[MAX_DINPUTS_CHANNELS];
	float counters[MAX_COUNTER_CHANNELS];
	int16_t range;
	float psensor;
	float battery;
	RtcTimeType_t rtc;
	uint8_t plt_Vcounters[2];
} dataframe_s;

// Estructuras para las consignas
typedef struct {
	uint8_t hour;
	uint8_t min;
} st_time_t;

typedef struct {
	st_time_t hhmm_c_diurna;
	st_time_t hhmm_c_nocturna;
	consigna_t c_aplicada;
} st_consigna_t;

typedef struct {		// Elemento de piloto: presion, hora.
	st_time_t hhmm;
	float pout;
} st_piloto_data_t;

typedef struct {
	float band;				// Banda de variacion de la presion de trabajo ( configurable )
	uint8_t max_steps;		// Maxima cantida de pasos para llegar a la presion ( configurable )
	t_valvula_reguladora tipo_valvula;
	st_piloto_data_t pSlots [MAX_PILOTO_PSLOTS];	// lista de presiones y horas en que se pone c/u.
} st_pilotos_t;

typedef struct {
	uint8_t outs;
	uint8_t	control;
} st_perforacion_t;

typedef struct {
	bool pwrs_enabled;
	st_time_t hora_start;
	st_time_t hora_fin;
} st_pwrsave_t;

// Configuracion de canales de contadores
typedef struct {
	char name[MAX_COUNTER_CHANNELS][PARAMNAME_LENGTH];
	float magpp[MAX_COUNTER_CHANNELS];
	uint16_t pwidth[MAX_COUNTER_CHANNELS];
	uint16_t period[MAX_COUNTER_CHANNELS];
	uint8_t speed[MAX_COUNTER_CHANNELS];
} counters_conf_t;

// Configuracion de canales digitales
typedef struct {
	char name[MAX_DINPUTS_CHANNELS][PARAMNAME_LENGTH];
	uint16_t tpoll[MAX_DINPUTS_CHANNELS];
} dinputs_conf_t;

// Configuracion de canales analogicos
typedef struct {
	uint8_t imin[MAX_ANALOG_CHANNELS];	// Coeficientes de conversion de I->magnitud (presion)
	uint8_t imax[MAX_ANALOG_CHANNELS];
	float mmin[MAX_ANALOG_CHANNELS];
	float mmax[MAX_ANALOG_CHANNELS];
	char name[MAX_ANALOG_CHANNELS][PARAMNAME_LENGTH];
	float offset[MAX_ANALOG_CHANNELS];
	uint16_t inaspan[MAX_ANALOG_CHANNELS];
	uint8_t pwr_settle_time;
	float ieq_min[MAX_ANALOG_CHANNELS];
	float ieq_max[MAX_ANALOG_CHANNELS];
} ainputs_conf_t;

// Configuracion de salidas
typedef struct {
	uint8_t modo;
	st_consigna_t consigna;
	st_pilotos_t piloto;
	st_perforacion_t perforacion;
} doutputs_conf_t;

typedef struct {
	char dlgId[DLGID_LENGTH];
	char apn[APN_LENGTH];
	char server_tcp_port[PORT_LENGTH];
	char server_ip_address[IP_LENGTH];
	char serverScript[SCRIPT_LENGTH];
	char simpwd[PASSWD_LENGTH];
	uint32_t timerDial;
	st_pwrsave_t pwrSave;
} gprs_conf_t;

// Configuracion del sensor i2c de presion
typedef struct {
	char name[PARAMNAME_LENGTH];
	float pmax;
	float pmin;
	float offset;
} psensor_conf_t;

typedef struct {

	// Variables de trabajo.

	t_debug debug;
	uint16_t timerPoll;
	bool rangeMeter_enabled;

	counters_conf_t counters_conf;	// Estructura con la configuracion de los contadores
	dinputs_conf_t dinputs_conf;	// Estructura con la configuracion de las entradas digitales
	ainputs_conf_t ainputs_conf;	// Estructura con la configuracion de las entradas analogicas
	doutputs_conf_t doutputs_conf;	//
	gprs_conf_t	gprs_conf;
	psensor_conf_t psensor_conf;

	// El checksum DEBE ser el ultimo byte del systemVars !!!!
	uint8_t checksum;

} systemVarsType;

systemVarsType systemVars;

// UTILS
void initMCU(void);
void u_configure_systemMainClock(void);
void u_configure_RTC32(void);
uint8_t u_control_string( char *s_name );
void u_convert_str_to_time_t ( char *time_str, st_time_t *time_struct );
void u_convert_int_to_time_t ( int int16time, st_time_t *time_struct );
void u_load_defaults( char *opt );
uint8_t u_save_params_in_NVMEE(void);
bool u_load_params_from_NVMEE(void);
void u_config_timerpoll ( char *s_timerpoll );
void u_format_memory(void);

void u_df_print_range( dataframe_s *df );
void u_df_print_psensor( dataframe_s *df );

bool u_check_more_Rcds4Del ( FAT_t *fat );
bool u_check_more_Rcds4Tx(void);

// TKCTL
void ctl_watchdog_kick(uint8_t taskWdg, uint16_t timeout_in_secs );
void ctl_print_wdg_timers(void);
uint16_t ctl_readTimeToNextPoll(void);
void ctl_reload_timerPoll( uint16_t new_time );
bool ctl_terminal_connected(void);

uint32_t ctl_read_timeToNextDial(void);
void ctl_set_timeToNextDial( uint32_t new_time );

// DINPUTS
void dinputs_init( void );
int8_t dinputs_read_channel ( uint8_t din );
void dinputs_config_defaults(void);
bool dinputs_config_channel( uint8_t channel,char *s_aname ,char *s_tpoll );
void dinputs_df_print( dataframe_s *df );

// RANGE
int16_t range_read(void);
bool range_config ( char *s_mode );
void range_config_defaults(void);

// PSENSOR
void psensor_init(void);
bool  psensor_read( float *psens );
void psensor_test_read (void);
bool psensor_config ( char *s_pname, char *s_pmin, char *s_pmax, char *s_offset  );
void psensor_config_defaults(void);

// AINPUTS
void ainputs_prender_12V_sensors(void);
void ainputs_apagar_12Vsensors(void);
uint16_t ainputs_read_channel_raw(uint8_t channel_id );
float ainputs_read_channel ( uint8_t io_channel );
uint16_t ainputs_read_battery_raw(void);
float ainputs_read_battery(void);
void ainputs_awake(void);
void ainputs_sleep(void);
void ainputs_df_print( dataframe_s *df );
void ainputs_df_print_battery( dataframe_s *df);

bool ainputs_config_channel( uint8_t channel,char *s_aname,char *s_imin,char *s_imax,char *s_mmin,char *s_mmax );
void ainputs_config_defaults(void);
bool ainputs_config_offset( char *s_channel, char *s_offset );
void ainputs_config_timepwrsensor ( char *s_sensortime );
void ainputs_config_span ( char *s_channel, char *s_span );
bool ainputs_config_autocalibrar( char *s_channel, char *s_mag_val );
bool ainputs_config_ical( char *s_channel, char *s_ieqv );
float analog_convert_ieqv( float i_real, uint8_t io_channel );

// COUNTERS
float counters_read_channel( uint8_t cnt, bool reset_counter );
void counters_config_defaults(void);
bool counters_config_channel( uint8_t channel,char *s_param0, char *s_param1, char *s_param2, char *s_param3, char *s_param4 );
void counters_df_print( dataframe_s *df );

// TKDATA
void data_read_frame( bool polling );
void data_read_pAB( float *pA, float *pB );
void signal_tkData_poll_on(void);
void signal_tkData_poll_off(void);

// DINPUTS
void dinputs_read_din( uint16_t *d0, uint16_t *d1 );

// DOUTPUTS
void doutputs_config_defaults( char *opt );
bool doutputs_config_mode( char *mode );
bool doutputs_cmd_write_valve( char *param1, char *param2 );
bool doutputs_cmd_write_outputs( char *param_pin, char *param_state );

// DOUTPUTS: Consignas
bool consigna_write( char *tipo_consigna_str);
void tk_init_consigna(void);
void tk_consigna(void);
bool consigna_config ( char *hhmm1, char *hhmm2 );

// DOUTPUTS: Perforaciones
void tk_init_perforaciones(void);
void tk_perforaciones(void);
void perforaciones_set_douts( uint8_t dout );
void perforaciones_set_douts_from_gprs( uint8_t dout );
uint16_t perforaciones_read_clt_timer(void);

// DOUTPUTS: Piloto
void tk_pilotos(void);
void tk_init_pilotos(void);
void pilotos_readCounters( uint8_t *VA_cnt, uint8_t *VB_cnt, uint8_t *VA_status, uint8_t *VB_status );
void pilotos_df_print( dataframe_s *df );
bool piloto_config( char *param1, char *param2, char *param3, char *param4 );
void piloto_config_online( char *slot, char *pout );

// WATCHDOG
uint8_t wdg_resetCause;

#define WDG_CTL			0
#define WDG_CMD			1
#define WDG_COUNT0		2
#define WDG_COUNT1		3
#define WDG_DAT			4
#define WDG_DOUT		5
#define WDG_GPRSRX		6
#define WDG_GPRSTX		7
#define WDG_DINPUTS		8

#define NRO_WDGS		9

#define WDG_DOUT_TIMEOUT	100

#endif /* SRC_SPX_H_ */
