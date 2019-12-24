/*
 * spx_tkData.c
 *
 *  Created on: 11 dic. 2018
 *      Author: pablo
 *
 *  Esta tarea se encarga de leer los canales analogicos y luego armar un frame de datos
 *  con los datos de los contadores y entradas digitales.
 *  Luego los muestra en pantalla y los guarda en memoria.
 *  Todo esta pautado por el timerPoll.
 *  Usamos una estructura unica global para leer los datos.
 *  Cuando usamos el comando de 'read frame' desde el cmdMode, escribimos sobre la misma
 *  estructura de datos por lo cual si coincide con el momento de poleo se pueden
 *  llegar a sobreescribir.
 *  Esto en realidad no seria importante ya que en modo cmd esta el operador.
 *
 */

#include "spx.h"

dataframe_s data_df;
st_dataRecord_t data_dr;
FAT_t data_fat;

bool sensores_prendidos = false;
static bool poll_enable = true;

//------------------------------------------------------------------------------------
// PROTOTIPOS

static void pv_data_init(void);
static void pv_data_read_frame(void);
static void pv_data_read_analogico( void );
static void pv_data_read_contadores( void );
static void pv_data_read_rangemeter( void );
static void pv_data_read_psensor( void );
static void pv_data_read_temp( void );
static void pv_data_read_dinputs( void );
static void pv_data_read_pilotos( void );
static void pv_data_print_frame( void );
static void pv_data_guardar_en_BD(void);
static void pv_data_signal_to_tkgprs(void);;

// La tarea pasa por el mismo lugar c/timerPoll secs.
#define WDG_DAT_TIMEOUT	 ( systemVars.timerPoll + 60 )

//------------------------------------------------------------------------------------
void tkData(void * pvParameters)
{

( void ) pvParameters;

uint32_t waiting_ticks = 0;
TickType_t xLastWakeTime = 0;

	// Espero la notificacion para arrancar
	while ( !startTask )
		vTaskDelay( ( TickType_t)( 100 / portTICK_RATE_MS ) );

	xprintf_P( PSTR("starting tkData..\r\n\0"));

	pv_data_init();

   // Initialise the xLastWakeTime variable with the current time.
    xLastWakeTime = xTaskGetTickCount();

    // Al arrancar poleo a los 10s
    waiting_ticks = (uint32_t)(10) * 1000 / portTICK_RATE_MS;

	// loop
	for( ;; )
	{

		ctl_watchdog_kick(WDG_DAT , WDG_DAT_TIMEOUT);

		// Espero. Da el tiempo necesario para entrar en tickless.
		vTaskDelayUntil( &xLastWakeTime, waiting_ticks );

		memset( &data_df, '\0', sizeof(dataframe_s));

		// Leo analog,digital,rtc,salvo en BD e imprimo.
		pv_data_read_frame();

		// Muestro en pantalla y si tengo xbee tambien lo muestro ahi.
		pv_data_print_frame();

		// Guardo en la BD
		pv_data_guardar_en_BD();

		pv_data_signal_to_tkgprs();

		// Calculo el tiempo para una nueva espera
		while ( xSemaphoreTake( sem_SYSVars, ( TickType_t ) 5 ) != pdTRUE )
			taskYIELD();

			// timpo real que voy a dormir esta tarea
			waiting_ticks = (uint32_t)(systemVars.timerPoll) * 1000 / portTICK_RATE_MS;
			// tiempo similar que voy a decrementar y mostrar en consola.
			ctl_reload_timerPoll(systemVars.timerPoll);

		xSemaphoreGive( sem_SYSVars );

	}

}
//------------------------------------------------------------------------------------
// FUNCIONES PUBLICAS
//------------------------------------------------------------------------------------
void data_read_frame( bool polling )
{
	// Esta funcion puede polear y mostrar el resultado.
	// Si la flag polling es false, no poleo. ( requerido en cmd::status )

	if ( polling ) {
		// Leo analog,digital,rtc e imprimo.
		while ( xSemaphoreTake( sem_DATA, ( TickType_t ) 10 ) != pdTRUE )
			taskYIELD();

		pv_data_read_frame();

		xSemaphoreGive( sem_DATA );
	}

	pv_data_print_frame();

}
//------------------------------------------------------------------------------------
void data_read_pAB( float *pA, float *pB )
{
	// Utilizada desde tk_pilotos para leer las presiones de alta y baja

	while ( xSemaphoreTake( sem_DATA, ( TickType_t ) 10 ) != pdTRUE )
		taskYIELD();

	// Leo todos los canales analogicos.
	pv_data_read_analogico();
	*pA = data_df.ainputs[0];
	*pB = data_df.ainputs[1];

	xSemaphoreGive( sem_DATA );
}
//------------------------------------------------------------------------------------
// FUNCIONES PRIVADAS
//------------------------------------------------------------------------------------
static void pv_data_init(void)
{
	// Inicializo los INA con los que mido las entradas analogicas.
	ainputs_awake();
	ainputs_sleep();

    // Inicializo el sistema de medida de ancho de pulsos
    if ( spx_io_board == SPX_IO5CH ) {
    	RMETER_init( SYSMAINCLK );
     	xprintf_P( PSTR("RMETER init..\r\n\0"));

    }

    psensor_init();
    tempsensor_init();

    sensores_prendidos = false;

}
//------------------------------------------------------------------------------------
static void pv_data_read_analogico( void )
{
	// Prendo los sensores, espero un settle time de 1s, los leo y apago los sensores.
	// En las placas SPX_IO8 siempre dejo los sensores prendidos y no los apago mas.
	// Los prendo si estoy con una placa IO5 y los sensores estan apagados.

	if ( ! poll_enable )
		return;

	ainputs_awake();
	//
	if ( ! sensores_prendidos ) {

		ainputs_prender_12V_sensors();
		sensores_prendidos = true;

		// Normalmente espero 1s de settle time que esta bien para los sensores
		// pero cuando hay un caudalimetro de corriente, necesita casi 5s
		// vTaskDelay( ( TickType_t)( 1000 / portTICK_RATE_MS ) );
		vTaskDelay( ( TickType_t)( ( 1000 * systemVars.ainputs_conf.pwr_settle_time ) / portTICK_RATE_MS ) );

	}

	// Leo.
	// Los canales de IO no son los mismos que los canales del INA !! ya que la bateria
	// esta en el canal 1 del ina2
	// Lectura general.
	data_df.ainputs[0] = ainputs_read_channel(0);
	data_df.ainputs[1] = ainputs_read_channel(1);
	data_df.ainputs[2] = ainputs_read_channel(2);
	data_df.ainputs[3] = ainputs_read_channel(3);
	data_df.ainputs[4] = ainputs_read_channel(4);

	if ( spx_io_board == SPX_IO8CH ) {
		data_df.ainputs[5] = ainputs_read_channel(5);
		data_df.ainputs[6] = ainputs_read_channel(6);
		data_df.ainputs[7] = ainputs_read_channel(7);
	}

	if ( spx_io_board == SPX_IO5CH ) {
		// Leo la bateria
		// Convierto el raw_value a la magnitud ( 8mV por count del A/D)
		data_df.battery = ainputs_read_battery();
	}

	// Apago los sensores y pongo a los INA a dormir si estoy con la board IO5.
	// Sino dejo todo prendido porque estoy en modo continuo
	//if ( (spx_io_board == SPX_IO5CH) && ( systemVars.timerPoll > 180 ) ) {
	if ( spx_io_board == SPX_IO5CH ) {
		ainputs_apagar_12Vsensors();
		sensores_prendidos = false;
	}
	//
	ainputs_sleep();

}
//------------------------------------------------------------------------------------
static void pv_data_read_contadores( void )
{
	// Leo los contadores

uint8_t i = 0;

	for ( i = 0; i < NRO_COUNTERS; i++ ) {
		data_df.counters[i] = counters_read_channel(i, true);
	}
}
//------------------------------------------------------------------------------------
static void pv_data_read_rangemeter( void )
{

	if ( ( spx_io_board == SPX_IO5CH ) && ( systemVars.rangeMeter_enabled == true ) ) {
		// Leo el ancho de pulso ( rangeMeter ). Demora 5s si esta habilitado
		data_df.range = range_read();
	}
}
//------------------------------------------------------------------------------------
static void pv_data_read_psensor( void )
{

	if ( ! strcmp ( systemVars.psensor_conf.name, "X" ) )
		return;

	psensor_read(&data_df.psensor);

}
//------------------------------------------------------------------------------------
static void pv_data_read_temp( void )
{

	// La temp. esta asociada al sensor de presion !!!
	if ( ! strcmp ( systemVars.psensor_conf.name, "X" ) )
		return;

	tempsensor_read(&data_df.temp);

}
//------------------------------------------------------------------------------------

static void pv_data_read_dinputs( void )
{
uint8_t ch;

	// Leo las entradas digitales
	if ( spx_io_board == SPX_IO8CH ) {
		for ( ch = 0; ch < NRO_DINPUTS; ch++ ) {
			data_df.dinputsA[ch] = dinputs_read_channel(ch);
		}
		return;
	}


	dinputs_read_din( (uint16_t *)&data_df.dinputsA[0], (uint16_t *)&data_df.dinputsA[1] );

}
//------------------------------------------------------------------------------------
static void pv_data_read_pilotos( void )
{
uint8_t VA_cnt = 0;
uint8_t VB_cnt = 0;
uint8_t VA_status, VB_status;

	if ( systemVars.doutputs_conf.modo == PILOTOS ) {
		pilotos_readCounters(&VA_cnt, &VB_cnt, &VA_status, &VB_status );
	}

	data_df.plt_Vcounters[0] = VA_cnt;
	data_df.plt_Vcounters[1] = VB_cnt;

}
//------------------------------------------------------------------------------------
static void pv_data_read_frame( void )
{
	// Leo todos los canales de a 1 que forman un frame de datos analogicos

int8_t xBytes = 0;

	pv_data_read_analogico();
	pv_data_read_contadores();
    pv_data_read_dinputs();
    pv_data_read_rangemeter();
    pv_data_read_psensor();
    pv_data_read_temp();
    pv_data_read_pilotos();

	// Agrego el timestamp
	xBytes = RTC_read_dtime( &data_df.rtc );
	if ( xBytes == -1 )
		xprintf_P(PSTR("ERROR: I2C:RTC:pub_data_read_frame\r\n\0"));

}
//------------------------------------------------------------------------------------
static void pv_data_print_frame( void)
{
	// Imprime el frame actual en consola

	// HEADER
	xprintf_P(PSTR("frame: " ) );

	// timeStamp.
	xprintf_P(PSTR("%04d%02d%02d,"),data_df.rtc.year, data_df.rtc.month, data_df.rtc.day );
	xprintf_P(PSTR("%02d%02d%02d"), data_df.rtc.hour, data_df.rtc.min, data_df.rtc.sec );

	ainputs_df_print( &data_df );
	dinputs_df_print( &data_df );
    counters_df_print( &data_df );
	u_df_print_range( &data_df );
	u_df_print_psensor( &data_df );
	u_df_print_temp( &data_df );
	pilotos_df_print( &data_df);
	ainputs_df_print_battery( &data_df );

	// TAIL
	xprintf_P(PSTR("\r\n\0") );

}
//------------------------------------------------------------------------------------
static void pv_data_guardar_en_BD(void)
{

	// Solo los salvo en la BD si estoy en modo normal.
	// En otros casos ( service, monitor_frame, etc, no.

int8_t bytes_written = 0;
static bool primer_frame = true;

	memset( &data_fat, '\0', sizeof(FAT_t));
	memset( &data_dr, '\0', sizeof(st_dataRecord_t));

	// Para no incorporar el error de los contadores en el primer frame no lo guardo.
	if ( primer_frame ) {
		primer_frame = false;
		return;
	}

	// Copio al dr solo los campos que correspondan
	switch ( spx_io_board ) {
	case SPX_IO5CH:
		memcpy( &data_dr.df.io5.ainputs, &data_df.ainputs, ( NRO_ANINPUTS * sizeof(float)));
		memcpy( &data_dr.df.io5.dinputsA, &data_df.dinputsA, ( NRO_DINPUTS * sizeof(uint16_t)));
		memcpy( &data_dr.df.io5.counters, &data_df.counters, ( NRO_COUNTERS * sizeof(float)));
		data_dr.df.io5.range = data_df.range;
		data_dr.df.io5.psensor = data_df.psensor;
		data_dr.df.io5.temp = data_df.temp;
		data_dr.df.io5.battery = data_df.battery;
		memcpy( &data_dr.rtc, &data_df.rtc, sizeof(RtcTimeType_t) );
		memcpy( &data_dr.df.io5.plt_Vcounters, &data_df.plt_Vcounters, 2 * sizeof(uint8_t) );
		break;
	case SPX_IO8CH:
		memcpy( &data_dr.df.io8.ainputs, &data_df.ainputs, ( NRO_ANINPUTS * sizeof(float)));
		memcpy( &data_dr.df.io8.dinputsA, &data_df.dinputsA, ( NRO_DINPUTS * sizeof(uint8_t)));
		memcpy( &data_dr.df.io8.counters, &data_df.counters, ( NRO_COUNTERS * sizeof(float)));
		memcpy( &data_dr.rtc, &data_df.rtc, sizeof(RtcTimeType_t) );
		break;
	default:
		return;
	}

	// Guardo en BD
	bytes_written = FF_writeRcd( &data_dr, sizeof(st_dataRecord_t) );

	if ( bytes_written == -1 ) {
		// Error de escritura o memoria llena ??
		xprintf_P(PSTR("DATA: WR ERROR (%d)\r\n\0"),FF_errno() );
		// Stats de memoria
		FAT_read(&data_fat);
		xprintf_P( PSTR("DATA: MEM [wr=%d,rd=%d,del=%d]\0"), data_fat.wrPTR,data_fat.rdPTR, data_fat.delPTR );
	}

}
//------------------------------------------------------------------------------------
static void pv_data_signal_to_tkgprs(void)
{
	// Aviso a tkGprs que hay un frame listo. En modo continuo lo va a trasmitir enseguida.

	if ( ! MODO_DISCRETO ) {
		while ( xTaskNotify(xHandle_tkGprsRx, TK_FRAME_READY , eSetBits ) != pdPASS ) {
			vTaskDelay( ( TickType_t)( 100 / portTICK_RATE_MS ) );
		}
	}
}
//------------------------------------------------------------------------------------
void signal_tkData_poll_off(void)
{
	poll_enable = false;
}
//------------------------------------------------------------------------------------
void signal_tkData_poll_on(void)
{
	poll_enable = true;
}
//------------------------------------------------------------------------------------
