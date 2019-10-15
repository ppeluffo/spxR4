/*
 * spx_tkCtl.c
 *
 *  Created on: 4 de oct. de 2017
 *      Author: pablo
 *
 *  Se realizan una serie de controles de housekeeping.
	 *  - Control de terminal conectada: Ve si hay una terminal conectada y activa una flag
	 *    que se usa internamente para flashear los leds y externamente por medio de una
	 *    funcion para avisarle a otras tareas ( tkCmd ) que hay una terminal conectada
	 *  - Reset diario: Cuenta los ticks durante un dia y resetea al micro. Con esto tenemos un
	 *    control mas que en caso que algo este colgado, deberia solucionarlo
	 *  - Leds: los hace flashear cuando hay una terminal conectada. Si ademas el modem esta prendido
	 *    hace flasear el led de comunicaciones.
	 *    Para saber si el modem esta prendido se accede por la funcion u_gprs_modem_prendido().
	 *  - Control de los watchdogs: c/tarea tiene un watchdog que debe apagarlo periodicamente.
	 *    Localmente c/wdg se implementa como un timer con valores diferentes para c/tarea ya que sus
	 *    necesidades de correr son diferentes.
	 *    El WDG del micro se fija en 8s por lo que esta tarea debe correr c/5s para apagar a tiempo
	 *    el wdg. En este tiempo va disminuyendo los wdg.individuales y si alguno llega a 0 resetea al micro.
	 *  - Ticks: Implemento una lista de timers que c/ronda voy decreciendo para que me den a otras
	 *    tareas los valores de ciertos timers que varian ( timerpoll, timerdial ).
 *
 *
 */

#include "spx.h"
#include "gprs.h"

//------------------------------------------------------------------------------------
static void pv_ctl_init_system(void);
static void pv_ctl_wink_led(void);
static void pv_ctl_check_wdg(void);
static void pv_ctl_ticks(void);
static void pv_ctl_daily_reset(void);
static void pv_ctl_check_terminal(void);

#define MAX_TIMERS	2
#define TIME_TO_NEXT_POLL	0
#define TIME_TO_NEXT_DIAL	1
static uint32_t pv_timers[MAX_TIMERS] = { 0, 0, 0 };

static uint16_t watchdog_timers[NRO_WDGS] = { 0,0,0,0,0,0,0,0 };
static bool f_terminal_connected = false;;

// Timpo que espera la tkControl entre round-a-robin
#define TKCTL_DELAY_S	5

// La tarea pasa por el mismo lugar c/5s.
#define WDG_CTL_TIMEOUT	30

const char string_0[] PROGMEM = "CTL";
const char string_1[] PROGMEM = "CMD";
const char string_2[] PROGMEM = "CNT0";
const char string_3[] PROGMEM = "CNT1";
const char string_4[] PROGMEM = "DAT";
const char string_5[] PROGMEM = "DOUT";
const char string_6[] PROGMEM = "GRX";
const char string_7[] PROGMEM = "GTX";
const char string_8[] PROGMEM = "DIN";

const char * const wdg_names[] PROGMEM = { string_0, string_1, string_2, string_3, string_4, string_5, string_6, string_7, string_8 };

//------------------------------------------------------------------------------------
void tkCtl(void * pvParameters)
{

( void ) pvParameters;

	vTaskDelay( ( TickType_t)( 500 / portTICK_RATE_MS ) );

	pv_ctl_init_system();

	xprintf_P( PSTR("\r\nstarting tkControl..\r\n\0"));

	for( ;; )
	{

		// Paso c/5s plt 30s es suficiente.
		ctl_watchdog_kick(WDG_CTL, WDG_CTL_TIMEOUT);

		// Cada 5s controlo el watchdog y los timers.
		pv_ctl_check_wdg();
		pv_ctl_ticks();
		pv_ctl_check_terminal();
		pv_ctl_wink_led();
		pv_ctl_daily_reset();

		// Para entrar en tickless.
		// Cada 5s hago un chequeo de todo. En particular esto determina el tiempo
		// entre que activo el switch de la terminal y que esta efectivamente responde.
		vTaskDelay( ( TickType_t)( TKCTL_DELAY_S * 1000 / portTICK_RATE_MS ) );

	}
}
//------------------------------------------------------------------------------------
static void pv_ctl_init_system(void)
{

	// Esta funcion corre cuando el RTOS esta inicializado y corriendo lo que nos
	// permite usar las funciones del rtos-io y rtos en las inicializaciones.
	//

uint8_t wdg = 0;
FAT_t l_fat;
uint16_t recSize = 0;

	memset( &l_fat, '\0', sizeof(FAT_t));

	// Configuro los pines del micro que no se configuran en funciones particulares
	// LEDS:
	IO_config_LED_KA();
	IO_config_LED_COMMS();

	// TERMINAL CTL PIN
	IO_config_TERMCTL_PIN();
	IO_config_TERMCTL_PULLDOWN();

	// Deteccion inicial de la termial conectada o no.
	f_terminal_connected = false;
	if (  IO_read_TERMCTL_PIN() == 1 ) {
		f_terminal_connected = true;
	}

	// BAUD RATE PIN
	IO_config_BAUD_PIN();

	// Al comienzo leo este handle para asi usarlo para leer el estado de los stacks.
	// En la medida que no estoy usando la taskIdle podria deshabilitarla. !!!
	xHandle_idle = xTaskGetIdleTaskHandle();

	// Inicializo todos los watchdogs a 15s ( 3 * 5s de loop )
	for ( wdg = 0; wdg < NRO_WDGS; wdg++ ) {
		watchdog_timers[wdg] = (uint16_t)( TKCTL_DELAY_S * 6 );
	}

	// Determino la io_board attached
	spx_io_board = SPX_IO5CH;
	if ( I2C_scan_device( BUSADDR_INA_C ) ) {
		spx_io_board = SPX_IO8CH;
	}


	// Luego del posible error del bus I2C espero para que se reponga !!!
	vTaskDelay( ( TickType_t)( 100 ) );

	// Configuro los pines
	u_gprs_init_pines();

	// Leo los parametros del la EE y si tengo error, cargo por defecto
	if ( ! u_load_params_from_NVMEE() ) {
		u_load_defaults( NULL );
		xprintf_P( PSTR("\r\nLoading defaults !!\r\n\0"));
	}

	pv_timers[TIME_TO_NEXT_POLL] = systemVars.timerPoll;

	// Inicializo la memoria EE ( fileSysyem)
	if ( FF_open() ) {
		xprintf_P( PSTR("FSInit OK\r\n\0"));
	} else {
		FF_format(false );	// Reformateo soft.( solo la FAT )
		xprintf_P( PSTR("FSInit FAIL !!.Reformatted...\r\n\0"));
	}

	FAT_read(&l_fat);
	xprintf_P( PSTR("MEMsize=%d,wrPtr=%d,rdPtr=%d,delPtr=%d,r4wr=%d,r4rd=%d,r4del=%d \r\n\0"),FF_MAX_RCDS, l_fat.wrPTR,l_fat.rdPTR, l_fat.delPTR,l_fat.rcds4wr,l_fat.rcds4rd,l_fat.rcds4del );

	// Imprimo el tamanio de registro de memoria
	recSize = sizeof(st_dataRecord_t);
	xprintf_P( PSTR("RCD size %d bytes.\r\n\0"),recSize);

	// Arranco el RTC. Si hay un problema lo inicializo.
	RTC_init();

	// Inicializo los parametros operativos segun la spx_io_board
	if ( spx_io_board == SPX_IO5CH ) {

		NRO_ANINPUTS = IO5_ANALOG_CHANNELS;
		NRO_DINPUTS = IO5_DINPUTS_CHANNELS;
		NRO_COUNTERS = IO5_COUNTER_CHANNELS;

		xprintf_P( PSTR("SPX_IO5CH: DINPUTS=%d, COUNTERS=%d, ANINPUTS=%d\r\n\0"), NRO_DINPUTS , NRO_COUNTERS, NRO_ANINPUTS );

	} else if ( spx_io_board == SPX_IO8CH ) {

		range_config("OFF");

		NRO_ANINPUTS = IO8_ANALOG_CHANNELS;
		NRO_DINPUTS = IO8_DINPUTS_CHANNELS;
		NRO_COUNTERS = IO8_COUNTER_CHANNELS;

		xprintf_P( PSTR("SPX_IO8CH: DINPUTS=%d, COUNTERS=%d, ANINPUTS=%d\r\n\0"), NRO_DINPUTS, NRO_COUNTERS, NRO_ANINPUTS );

	}
	xprintf_P( PSTR("------------------------------------------------\r\n\0"));


	// Habilito a arrancar al resto de las tareas
	startTask = true;

}
//------------------------------------------------------------------------------------
static void pv_ctl_check_terminal(void)
{
	// Lee el pin de la terminal para ver si hay o no una conectada.
	// Si bien en la IO8 no es necesario desconectar la terminal ya que opera
	// con corriente, por simplicidad uso un solo codigo para ambas arquitecturas.

	if ( IO_read_TERMCTL_PIN() == 1) {
		f_terminal_connected = true;
	} else {
		f_terminal_connected = false;
	}
	return;
}
//------------------------------------------------------------------------------------
static void pv_ctl_wink_led(void)
{

	// SI la terminal esta desconectada salgo.
	if ( ! ctl_terminal_connected() )
		return;

	// Prendo los leds
	IO_set_LED_KA();
	if ( u_gprs_modem_prendido() ) {
		IO_set_LED_COMMS();
	}

	vTaskDelay( ( TickType_t)( 50 / portTICK_RATE_MS ) );
	//taskYIELD();

	// Apago
	IO_clr_LED_KA();
	IO_clr_LED_COMMS();

}
//------------------------------------------------------------------------------------
static void pv_ctl_check_wdg(void)
{
	// Cada tarea periodicamente reinicia su wdg timer.
	// Esta tarea los decrementa cada 5 segundos.
	// Si alguno llego a 0 es que la tarea se colgo y entonces se reinicia el sistema.

	uint8_t wdg = 0;
	char buffer[10] = { '\0','\0','\0','\0','\0','\0','\0','\0','\0','\0' } ;

		// Cada ciclo reseteo el wdg para que no expire.
		WDT_Reset();
		//return;
		// Si algun WDG no se borro, me reseteo
		while ( xSemaphoreTake( sem_WDGS, ( TickType_t ) 5 ) != pdTRUE )
			taskYIELD();

		for ( wdg = 0; wdg < NRO_WDGS; wdg++ ) {

			if ( watchdog_timers[wdg] > TKCTL_DELAY_S ) {
				watchdog_timers[wdg] -= TKCTL_DELAY_S;
			} else {
				watchdog_timers[wdg] = 0;
			}

			if ( watchdog_timers[wdg] == 0 ) {
				memset(buffer,'\0', 10);
				strcpy_P(buffer, (PGM_P)pgm_read_word(&(wdg_names[wdg])));
				xprintf_P( PSTR("CTL: WDG TO(%s) !!\r\n\0"),buffer);
				vTaskDelay( ( TickType_t)( 500 / portTICK_RATE_MS ) );

				// Me reseteo por watchdog
				while(1)
				  ;
				//CCPWrite( &RST.CTRL, RST_SWRST_bm );   /* Issue a Software Reset to initilize the CPU */

			}
		}

		xSemaphoreGive( sem_WDGS );
}
//------------------------------------------------------------------------------------
static void pv_ctl_ticks(void)
{
uint8_t i = 0;

	// Ajusto los timers hasta llegar a 0.

	for ( i = 0; i < MAX_TIMERS; i++ ) {

		if ( pv_timers[i] > TKCTL_DELAY_S ) {
			pv_timers[i] -= TKCTL_DELAY_S;
		} else {
			pv_timers[i] = 0;
		}
	}
}
//------------------------------------------------------------------------------------
static void pv_ctl_daily_reset(void)
{
	// Todos los dias debo resetearme para restaturar automaticamente posibles
	// problemas.
	// Se invoca 1 vez por minuto ( 60s ).

static uint32_t ticks_to_reset = 86400 / TKCTL_DELAY_S ; // ticks en 1 dia.

	while ( --ticks_to_reset > 0 ) {
		return;
	}

	xprintf_P( PSTR("Daily Reset !!\r\n\0") );
	vTaskDelay( ( TickType_t)( 1000 / portTICK_RATE_MS ) );

	CCPWrite( &RST.CTRL, RST_SWRST_bm );   /* Issue a Software Reset to initilize the CPU */


}
//------------------------------------------------------------------------------------
// FUNCIONES PUBLICAS
//------------------------------------------------------------------------------------
void ctl_watchdog_kick(uint8_t taskWdg, uint16_t timeout_in_secs )
{
	// Reinicia el watchdog de la tarea taskwdg con el valor timeout.
	// timeout es uint16_t por lo tanto su maximo valor en segundos es de 65536 ( 18hs )

	while ( xSemaphoreTake( sem_WDGS, ( TickType_t ) 5 ) != pdTRUE )
		taskYIELD();

	watchdog_timers[taskWdg] = timeout_in_secs;

	xSemaphoreGive( sem_WDGS );
}
//------------------------------------------------------------------------------------
void ctl_print_wdg_timers(void)
{

	// Muestra en pantalla el valor de los timers individuales de los watchdogs.
	// Se usa solo para diagnostico.

uint8_t wdg = 0;
char buffer[10] = { '\0','\0','\0','\0','\0','\0','\0','\0','\0','\0' };

	while ( xSemaphoreTake( sem_SYSVars, ( TickType_t ) 5 ) != pdTRUE )
		taskYIELD();

	for ( wdg = 0; wdg < NRO_WDGS; wdg++ ) {
		memset(buffer,'\0', 10);
		strcpy_P(buffer, (PGM_P)pgm_read_word(&(wdg_names[wdg])));
		xprintf_P( PSTR("%d(%s)->%d \r\n\0"),wdg,buffer,watchdog_timers[wdg]);
	}

	xSemaphoreGive( sem_SYSVars );

	xprintf_P( PSTR("\r\n\0"));

}
//------------------------------------------------------------------------------------
bool ctl_terminal_connected(void)
{
	return(f_terminal_connected);
}
//------------------------------------------------------------------------------------
uint16_t ctl_readTimeToNextPoll(void)
{
	return( (uint16_t) pv_timers[TIME_TO_NEXT_POLL] );
}
//------------------------------------------------------------------------------------
void ctl_reload_timerPoll( uint16_t new_time )
{
	// Como trabajo en modo tickless, no puedo estar cada 1s despertandome solo para
	// ajustar el timerpoll.
	// La alternativa es en tkData dormir todo lo necesario y al recargar el timerpoll,
	// en tkCtl setear una variable con el mismo valor e irla decrementandola c/5 s solo
	// a efectos de visualizarla.
	pv_timers[TIME_TO_NEXT_POLL] = new_time;
}
//------------------------------------------------------------------------------------
uint32_t ctl_read_timeToNextDial(void)
{
	return( pv_timers[TIME_TO_NEXT_DIAL] );
}
//------------------------------------------------------------------------------------
void ctl_set_timeToNextDial( uint32_t new_time )
{
	pv_timers[TIME_TO_NEXT_DIAL] = new_time;
}
//------------------------------------------------------------------------------------
