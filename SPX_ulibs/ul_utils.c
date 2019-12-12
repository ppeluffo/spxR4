/*
 * spx_utis.c
 *
 *  Created on: 10 dic. 2018
 *      Author: pablo
 */

#include "spx.h"
#include "gprs.h"

#define RTC32_ToscBusy()        !( VBAT.STATUS & VBAT_XOSCRDY_bm )

//------------------------------------------------------------------------------------
void initMCU(void)
{
	// Inicializa los pines del micro

	// PWR_SLEEP
//	IO_config_PWR_SLEEP();
//	IO_set_PWR_SLEEP();


	// ANALOG: SENSOR VCC CONTROL
	IO_config_SENS_12V_CTL();

}
//------------------------------------------------------------------------------------
void RTC32_ToscEnable( bool use1khz );
//------------------------------------------------------------------------------------
void u_configure_systemMainClock(void)
{
/*	Configura el clock principal del sistema
	Inicialmente se arranca en 2Mhz.
	La configuracion del reloj tiene 2 componentes: el clock y el oscilador.
	OSCILADOR:
	Primero debo elejir cual oscilador voy a usar para alimentar los prescalers que me den
	el clock del sistema y esperar a que este este estable.
	CLOCK:
	Elijo cual oscilador ( puedo tener mas de uno prendido ) va a ser la fuente principal
	del closck del sistema.
	Luego configuro el prescaler para derivar los clocks de los perifericos.
	Puedo por ultimo 'lockear' esta configuracion para que no se cambie por error.
	Los registros para configurar el clock son 'protegidos' por lo que los cambio
	utilizando la funcion CCPwrite.

	Para nuestra aplicacion vamos a usar un clock de 32Mhz.
	Como vamos a usar el ADC debemos prestar atencion al clock de perifericos clk_per ya que luego
	el ADC clock derivado del clk_per debe estar entre 100khz y 1.4Mhz ( AVR1300 ).

	Opcionalmente podriamos deshabilitar el oscilador de 2Mhz para ahorrar energia.
*/

#if SYSMAINCLK == 32
	// Habilito el oscilador de 32Mhz
	OSC.CTRL |= OSC_RC32MEN_bm;

	// Espero que este estable
	do {} while ( (OSC.STATUS & OSC_RC32MRDY_bm) == 0 );

	// Seteo el clock para que use el oscilador de 32Mhz.
	// Uso la funcion CCPWrite porque hay que se cuidadoso al tocar estos
	// registros que son protegidos.
	CCPWrite(&CLK.CTRL, CLK_SCLKSEL_RC32M_gc);
	//
	// El prescaler A ( CLK.PSCCTRL ), B y C ( PSBCDIV ) los dejo en 0 de modo que no
	// hago division y con esto tengo un clk_per = clk_sys. ( 32 Mhz ).
	//
#endif

#if SYSMAINCLK == 8
	// Habilito el oscilador de 32Mhz y lo divido por 4
	OSC.CTRL |= OSC_RC32MEN_bm;

	// Espero que este estable
	do {} while ( (OSC.STATUS & OSC_RC32MRDY_bm) == 0 );

	// Seteo el clock para que use el oscilador de 32Mhz.
	// Uso la funcion CCPWrite porque hay que se cuidadoso al tocar estos
	// registros que son protegidos.
	CCPWrite(&CLK.CTRL, CLK_SCLKSEL_RC32M_gc);
	//
	// Pongo el prescaler A por 4 y el B y C en 0.
	CLKSYS_Prescalers_Config( CLK_PSADIV_4_gc, CLK_PSBCDIV_1_1_gc );

	//
#endif

#if SYSMAINCLK == 2
	// Este es el oscilador por defecto por lo cual no tendria porque configurarlo.
	// Habilito el oscilador de 2Mhz
	OSC.CTRL |= OSC_RC2MEN_bm;
	// Espero que este estable
	do {} while ( (OSC.STATUS & OSC_RC2MRDY_bm) == 0 );

	// Seteo el clock para que use el oscilador de 2Mhz.
	// Uso la funcion CCPWrite porque hay que se cuidadoso al tocar estos
	// registros que son protegidos.
	CCPWrite(&CLK.CTRL, CLK_SCLKSEL_RC2M_gc);
	//
	// El prescaler A ( CLK.PSCCTRL ), B y C ( PSBCDIV ) los dejo en 0 de modo que no
	// hago division y con esto tengo un clk_per = clk_sys. ( 2 Mhz ).
	//
#endif

//#ifdef configUSE_TICKLESS_IDLE
	// Para el modo TICKLESS
	// Configuro el RTC con el osc externo de 32Khz
	// Pongo como fuente el xtal externo de 32768 contando a 32Khz.
	//CLK.RTCCTRL = CLK_RTCSRC_TOSC32_gc | CLK_RTCEN_bm;
	//do {} while ( ( RTC.STATUS & RTC_SYNCBUSY_bm ) );

	// Disable RTC interrupt.
	// RTC.INTCTRL = 0x00;
	//
	// Si uso el RTC32, habilito el oscilador para 1ms.

	RTC32_ToscEnable(true);
//#endif

	// Lockeo la configuracion.
	CCPWrite( &CLK.LOCK, CLK_LOCK_bm );

}
//------------------------------------------------------------------------------------
void u_configure_RTC32(void)
{
	// El RTC32 lo utilizo para desperarme en el modo tickless.
	// V-bat needs to be reset, and activated
	VBAT.CTRL |= VBAT_ACCEN_bm;
	// Este registro esta protegido de escritura con CCP.
	CCPWrite(&VBAT.CTRL, VBAT_RESET_bm);

	// Pongo el reloj en 1.024Khz.
	VBAT.CTRL |=  VBAT_XOSCSEL_bm | VBAT_XOSCFDEN_bm ;

	// wait for 200us see AVR1321 Application note page 8
	_delay_us(200);

	// Turn on 32.768kHz crystal oscillator
	VBAT.CTRL |= VBAT_XOSCEN_bm;

	// Wait for stable oscillator
	while(!(VBAT.STATUS & VBAT_XOSCRDY_bm));

	// Disable RTC32 module before setting counter values
	RTC32.CTRL = 0;

	// Wait for sync
	do { } while ( RTC32.SYNCCTRL & RTC32_SYNCBUSY_bm );

	// EL RTC corre a 1024 hz y quiero generar un tick de 10ms,
	RTC32.PER = 1024;
	RTC32.CNT = 0;

	// Interrupt: on Overflow
	RTC32.INTCTRL = RTC32_OVFINTLVL_LO_gc;

	// Enable RTC32 module
	RTC32.CTRL = RTC32_ENABLE_bm;

	/* Wait for sync */
	do { } while ( RTC32.SYNCCTRL & RTC32_SYNCBUSY_bm );
}
//------------------------------------------------------------------------------------
void RTC32_ToscEnable( bool use1khz )
{
	/* Enable 32 kHz XTAL oscillator, with 1 kHz or 1 Hz output. */
	if (use1khz)
		VBAT.CTRL |= ( VBAT_XOSCEN_bm | VBAT_XOSCSEL_bm );
	else
		VBAT.CTRL |= ( VBAT_XOSCEN_bm );

	RTC32.PER = 10;
	RTC32.CNT = 0;

	/* Wait for oscillator to stabilize before returning. */
//	do { } while ( RTC32_ToscBusy() );
}
//------------------------------------------------------------------------------------
uint8_t u_control_string( char *s_name )
{
	// Controlo que el string terminado en \0 tenga solo letras o digitos.
	// Es porque si en un nombre de canal se cuela un caracter extranio, me
	// despelota los logs.
	// Si encuentro un caracter extraño, lo sustituyo por un \0 y salgo

uint8_t max_length = PARAMNAME_LENGTH;
char *p = NULL;
uint8_t cChar = 0;
uint8_t length = 0;

	p = (char *)s_name;
	while (*p && (max_length-- > 0) ) {

		cChar = (uint8_t)*p;
		if (  ! isalnum(cChar) )	{
			*p = '\0';
			return (length);
		}
		p++;
		length++;
	}

	return (length);
}
//------------------------------------------------------------------------------------
void u_convert_str_to_time_t ( char *time_str, st_time_t *time_struct )
{

	// Convierte un string hhmm en una estructura time_type que tiene
	// un campo hora y otro minuto

uint16_t time_num = 0;

	time_num = atol(time_str);
	time_struct->hour = (uint8_t) (time_num / 100);
	time_struct->min = (uint8_t)(time_num % 100);

//	xprintf_P( PSTR("DEBUG: u_convert_str_to_time_t (hh=%d,mm=%d)\r\n\0"), time_struct->hour,time_struct->min );

}
//------------------------------------------------------------------------------------
void u_convert_int_to_time_t ( int int16time, st_time_t *time_struct )
{

	// Convierte un int16  hhmm en una estructura time_type que tiene
	// un campo hora y otro minuto

	time_struct->hour = (uint8_t) (int16time / 100);
	time_struct->min = (uint8_t)(int16time % 100);

//	xprintf_P( PSTR("DEBUG: u_convert_str_to_time_t (hh=%d,mm=%d)\r\n\0"), time_struct->hour,time_struct->min );

}
//------------------------------------------------------------------------------------
void u_load_defaults( char *opt )
{
	// Carga la configuracion por defecto.

	systemVars.debug = DEBUG_NONE;

	if ( spx_io_board == SPX_IO8CH ) {
		systemVars.timerPoll = 60;
	} else {
		systemVars.timerPoll = 300;
	}

	counters_config_defaults();
	dinputs_config_defaults();
	ainputs_config_defaults();
	range_config_defaults();
	doutputs_config_defaults(  opt );
	u_gprs_load_defaults( opt );

	psensor_config_defaults();


}
//------------------------------------------------------------------------------------
uint8_t u_save_params_in_NVMEE(void)
{
	// Calculo el checksum del systemVars.
	// Considero el systemVars como un array de chars.

uint8_t *p = NULL;
uint8_t checksum = 0;
uint16_t data_length = 0;
uint16_t i = 0;

	// Calculo el checksum del systemVars.
	systemVars.checksum = 0;
	data_length = sizeof(systemVars);
	p = (uint8_t *)&systemVars;
	checksum = 0;
	// Recorro todo el systemVars considerando c/byte como un char, hasta
	// llegar al ultimo ( checksum ) que no lo incluyo !!!.
	for ( i = 0; i < (data_length - 1) ; i++) {
		checksum += p[i];
	}
	checksum = ~checksum;
	systemVars.checksum = checksum;

	// Guardo systemVars en la EE
	nvm_eeprom_erase_and_write_buffer(0x00, &systemVars, sizeof(systemVars));

	return(checksum);

}
//------------------------------------------------------------------------------------
bool u_load_params_from_NVMEE(void)
{
	// Leo el systemVars desde la EE.
	// Calculo el checksum. Si no coincide es que hubo algun
	// error por lo que cargo el default.


uint8_t *p = NULL;
uint8_t stored_checksum = 0;
uint8_t checksum = 0;
uint16_t data_length = 0;
uint16_t i = 0;

	// Leo de la EE es systemVars.
	nvm_eeprom_read_buffer(0x00, (char *)&systemVars, sizeof(systemVars));

	// Guardo el checksum que lei.
	stored_checksum = systemVars.checksum;

	// Calculo el checksum del systemVars leido
	systemVars.checksum = 0;
	data_length = sizeof(systemVars);
	p = ( uint8_t *)&systemVars;	// Recorro el systemVars como si fuese un array de int8.
	checksum = 0;
	for ( i = 0; i < ( data_length - 1 ); i++) {
		checksum += p[i];
	}
	checksum = ~checksum;

	if ( stored_checksum != checksum ) {
		return(false);
	}

	return(true);
}
//------------------------------------------------------------------------------------
void u_config_timerpoll ( char *s_timerpoll )
{
	// Configura el tiempo de poleo.
	// Se utiliza desde el modo comando como desde el modo online
	// El tiempo de poleo debe estar entre 15s y 3600s

	systemVars.timerPoll = atoi(s_timerpoll);

	if ( systemVars.timerPoll < 15 )
		systemVars.timerPoll = 15;

	if ( systemVars.timerPoll > 3600 )
		systemVars.timerPoll = 300;

	u_gprs_redial();

	return;
}
//------------------------------------------------------------------------------------
void u_df_print_range( dataframe_s *df )
{
	// Range
	if ( ( spx_io_board == SPX_IO5CH ) && ( systemVars.rangeMeter_enabled ) ) {
		xprintf_P(PSTR(",RANGE=%d"), df->range );
	}
}
//------------------------------------------------------------------------------------
void u_df_print_psensor( dataframe_s *df )
{
	// Range
	if ( ! strcmp ( systemVars.psensor_conf.name, "X" ) )
		return;

	xprintf_P(PSTR(",%s=%.01f"), systemVars.psensor_conf.name, df->psensor );

}
//------------------------------------------------------------------------------------
void u_df_print_temp( dataframe_s *df )
{
	// Range
	if ( ! strcmp ( systemVars.psensor_conf.name, "X" ) )
		return;

	xprintf_P(PSTR(",TEMP=%.01f"), df->temp );

}
//------------------------------------------------------------------------------------

void u_format_memory(void)
{
	// Nadie debe usar la memoria !!!
	ctl_watchdog_kick(WDG_CMD, 0x8000 );

	vTaskSuspend( xHandle_tkData );
	ctl_watchdog_kick(WDG_DAT, 0x8000 );

	vTaskSuspend( xHandle_tkCounter0 );
	ctl_watchdog_kick(WDG_COUNT0, 0x8000 );

	vTaskSuspend( xHandle_tkCounter1 );
	ctl_watchdog_kick(WDG_COUNT1, 0x8000 );

	vTaskSuspend( xHandle_tkDoutputs );
	ctl_watchdog_kick(WDG_DOUT, 0x8000 );

	vTaskSuspend( xHandle_tkGprsTx );
	ctl_watchdog_kick(WDG_GPRSRX, 0x8000 );

	vTaskSuspend( xHandle_tkDinputs );
	ctl_watchdog_kick(WDG_DINPUTS, 0x8000 );

	// Formateo
	FF_format(true);

	// Reset
	CCPWrite( &RST.CTRL, RST_SWRST_bm );   /* Issue a Software Reset to initilize the CPU */

}
 //------------------------------------------------------------------------------------
bool u_check_more_Rcds4Del ( FAT_t *fat )
{
	// Devuelve si aun quedan registros para borrar del FS

	memset ( fat, '\0', sizeof( FAT_t));

	FAT_read(fat);

	if ( fat->rcds4del > 0 ) {
		return(true);
	} else {
		return(false);
	}

}
//------------------------------------------------------------------------------------
bool u_check_more_Rcds4Tx(void)
{

	/* Veo si hay datos en memoria para trasmitir
	 * Memoria vacia: rcds4wr = MAX, rcds4del = 0;
	 * Memoria llena: rcds4wr = 0, rcds4del = MAX;
	 * Memoria toda leida: rcds4rd = 0;
	 */

//	gprs_fat.wrPTR,gprs_fat.rdPTR, gprs_fat.delPTR,gprs_fat.rcds4wr,gprs_fat.rcds4rd,gprs_fat.rcds4del );

bool retS = false;
FAT_t *fat;

	memset( fat, '\0', sizeof ( FAT_t));
	FAT_read(&fat);

	// Si hay registros para leer
	if ( fat->rcds4rd > 0) {
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
