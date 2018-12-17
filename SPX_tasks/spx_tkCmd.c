/*
 * sp5K_tkCmd.c
 *
 *  Created on: 27/12/2013
 *      Author: root
 */

#include "spx.h"

//----------------------------------------------------------------------------------------
// FUNCIONES DE USO PRIVADO
//----------------------------------------------------------------------------------------
static void pv_snprintfP_OK(void );
static void pv_snprintfP_ERR(void);
static void pv_cmd_read_fuses(void);
static bool pv_cmd_config_offset( char *s_channel, char *s_offset );
static bool pv_cmd_autocalibrar( char *s_channel, char *s_mag_val );
static void pv_cmd_config_sensortime ( char *s_sensortime );
static void pv_cmd_config_inaspan ( uint8_t channel, char *s_span );
static void pv_cmd_config_counter_debounce_time( char *s_counter_debounce_time );
static void pv_cmd_print_stack_watermarks(void);
static void pv_cmd_read_battery(void);
static void pv_cmd_read_analog_channel(void);
static void pv_cmd_read_digital_channels(void);
static void pv_cmd_read_memory(void);

//----------------------------------------------------------------------------------------
// FUNCIONES DE CMDMODE
//----------------------------------------------------------------------------------------
static void cmdHelpFunction(void);
static void cmdClearScreen(void);
static void cmdResetFunction(void);
static void cmdWriteFunction(void);
static void cmdReadFunction(void);
static void cmdStatusFunction(void);
static void cmdConfigFunction(void);
static void cmdKillFunction(void);

#define WDG_CMD_TIMEOUT	30

static usuario_t tipo_usuario;
RtcTimeType_t rtc;

//------------------------------------------------------------------------------------
void tkCmd(void * pvParameters)
{

uint8_t c;
uint8_t ticks;

( void ) pvParameters;

	// Espero la notificacion para arrancar
	while ( !startTask )
		vTaskDelay( ( TickType_t)( 100 / portTICK_RATE_MS ) );

	vTaskDelay( ( TickType_t)( 500 / portTICK_RATE_MS ) );

	FRTOS_CMD_init();

	// Registro los comandos y los mapeo al cmd string.
	FRTOS_CMD_register( "cls\0", cmdClearScreen );
	FRTOS_CMD_register( "reset\0", cmdResetFunction);
	FRTOS_CMD_register( "write\0", cmdWriteFunction);
	FRTOS_CMD_register( "read\0", cmdReadFunction);
	FRTOS_CMD_register( "help\0", cmdHelpFunction );
	FRTOS_CMD_register( "status\0", cmdStatusFunction );
	FRTOS_CMD_register( "config\0", cmdConfigFunction );
	FRTOS_CMD_register( "kill\0", cmdKillFunction );

	// Fijo el timeout del READ
	ticks = 5;
	frtos_ioctl( fdTERM,ioctl_SET_TIMEOUT, &ticks );

	tipo_usuario = USER_TECNICO;

	xprintf_P( PSTR("starting tkCmd..\r\n\0") );

	//FRTOS_CMD_regtest();
	// loop
	for( ;; )
	{

		// Con la terminal desconectada paso c/5s plt 30s es suficiente.
		ctl_watchdog_kick(WDG_CMD, WDG_CMD_TIMEOUT);

		// Si no tengo terminal conectada, duermo 5s lo que me permite entrar en tickless.
//		if ( IO_read_TERMCTL_PIN() == 0 ) {
//			vTaskDelay( ( TickType_t)( 5000 / portTICK_RATE_MS ) );

//		} else {

			c = '\0';	// Lo borro para que luego del un CR no resetee siempre el timer.
			// el read se bloquea 50ms. lo que genera la espera.
			//while ( CMD_read( (char *)&c, 1 ) == 1 ) {
			while ( frtos_read( fdTERM, (char *)&c, 1 ) == 1 ) {
				FRTOS_CMD_process(c);
//			}
		}
	}
}
//------------------------------------------------------------------------------------
static void cmdStatusFunction(void)
{

FAT_t l_fat;
uint8_t channel;

	xprintf_P( PSTR("\r\nSpymovil %s %s %s %s \r\n\0"), SPX_HW_MODELO, SPX_FTROS_VERSION, SPX_FW_REV, SPX_FW_DATE);
	xprintf_P( PSTR("Clock %d Mhz, Tick %d Hz\r\n\0"),SYSMAINCLK, configTICK_RATE_HZ );
	if ( spx_io_board == SPX_IO5CH ) {
		xprintf_P( PSTR("IOboard SPX5CH\r\n\0") );
	} else if ( spx_io_board == SPX_IO8CH ) {
		xprintf_P( PSTR("IOboard SPX8CH\r\n\0") );
	}

	// SIGNATURE ID
	xprintf_P( PSTR("uID=%s\r\n\0"), NVMEE_readID() );

	// Last reset cause
	xprintf_P( PSTR("WRST=0x%02X\r\n\0") ,wdg_resetCause );

	RTC_read_time();

	// Memoria
	FAT_read(&l_fat);
	xprintf_P( PSTR("memory: rcdSize=%d, wrPtr=%d,rdPtr=%d,delPtr=%d,r4wr=%d,r4rd=%d,r4del=%d \r\n\0"), sizeof(st_dataRecord_t), l_fat.wrPTR,l_fat.rdPTR, l_fat.delPTR,l_fat.rcds4wr,l_fat.rcds4rd,l_fat.rcds4del );

	// CONFIG
	xprintf_P( PSTR(">Config:\r\n\0"));

	// debug
	switch(systemVars.debug) {
	case DEBUG_NONE:
		xprintf_P( PSTR("  debug: none\r\n\0") );
		break;
	case DEBUG_COUNTER:
		xprintf_P( PSTR("  debug: counter\r\n\0") );
		break;
	default:
		xprintf_P( PSTR("  debug: ???\r\n\0") );
		break;
	}

	// Timerpoll
	xprintf_P( PSTR("  timerPoll: [%d s]/%d\r\n\0"),systemVars.timerPoll, ctl_readTimeToNextPoll() );

	// Sensor Pwr Time
	xprintf_P( PSTR("  timerPwrSensor: [%d s]\r\n\0"),systemVars.pwr_settle_time );

	// Counters debounce time
	xprintf_P( PSTR("  timerDebounceCnt: [%d s]\r\n\0"),systemVars.counter_debounce_time );

	// RangeMeter: PULSE WIDTH
	if ( spx_io_board == SPX_IO5CH ) {
		if ( systemVars.rangeMeter_enabled ) {
			xprintf_P( PSTR("  rangeMeter: on\r\n"));
		} else {
			xprintf_P( PSTR("  rangeMeter: off\r\n"));
		}
	}

	// aninputs
	for ( channel = 0; channel < NRO_ANINPUTS; channel++) {
		xprintf_P( PSTR("  a%d( ) [%d-%d mA/ %.02f,%.02f | %.02f | %.02f | %s]\r\n\0"),channel, systemVars.ain_imin[channel],systemVars.ain_imax[channel],systemVars.ain_mmin[channel],systemVars.ain_mmax[channel], systemVars.ain_inaspan[channel], systemVars.ain_offset[channel], systemVars.ain_name[channel] );
	}

	// dinputs
	for ( channel = 0; channel < NRO_DINPUTS; channel++) {
		xprintf_P( PSTR("  d%d( ) [ %s ]\r\n\0"),channel, systemVars.din_name[channel] );
	}

	// contadores
	for ( channel = 0; channel < NRO_COUNTERS; channel++) {
		xprintf_P( PSTR("  c%d [ %s | %.02f ]\r\n\0"),channel, systemVars.counters_name[channel],systemVars.counters_magpp[channel] );
	}

	data_show_frame( false );
}
//-----------------------------------------------------------------------------------
static void cmdResetFunction(void)
{
	// Resetea al micro prendiendo el watchdog

	FRTOS_CMD_makeArgv();

	// Reset memory ??
	if (!strcmp_P( strupr(argv[1]), PSTR("MEMORY\0"))) {

		// Nadie debe usar la memoria !!!
		ctl_watchdog_kick(WDG_CMD, 0xFFFF);

		vTaskSuspend( xHandle_tkData );
		ctl_watchdog_kick(WDG_DAT, 0xFFFF);

		vTaskSuspend( xHandle_tkCounter );
		ctl_watchdog_kick(WDG_COUNT, 0xFFFF);

		if (!strcmp_P( strupr(argv[2]), PSTR("SOFT\0"))) {
			FF_format(false );
		} else if (!strcmp_P( strupr(argv[2]), PSTR("HARD\0"))) {
			FF_format(true);
		} else {
			xprintf_P( PSTR("ERROR\r\nUSO: reset memory {hard|soft}\r\n\0"));
			return;
		}
	}

	cmdClearScreen();

//	while(1)
//		;

	CCPWrite( &RST.CTRL, RST_SWRST_bm );   /* Issue a Software Reset to initilize the CPU */


}
//------------------------------------------------------------------------------------
static void cmdWriteFunction(void)
{

	FRTOS_CMD_makeArgv();

	// RTC
	// write rtc YYMMDDhhmm
	if (!strcmp_P( strupr(argv[1]), PSTR("RTC\0")) ) {
		( RTC_write_time( argv[2]) > 0)?  pv_snprintfP_OK() : 	pv_snprintfP_ERR();
		return;
	}

	// EE
	// write ee pos string
	if (!strcmp_P( strupr(argv[1]), PSTR("EE\0")) && ( tipo_usuario == USER_TECNICO) ) {
		( EE_test_write ( argv[2], argv[3] ) > 0)?  pv_snprintfP_OK() : 	pv_snprintfP_ERR();
		return;
	}

	// NVMEE
	// write nvmee pos string
	if (!strcmp_P( strupr(argv[1]), PSTR("NVMEE\0")) && ( tipo_usuario == USER_TECNICO) ) {
		( NVMEE_test_write ( argv[2], argv[3] ) > 0)?  pv_snprintfP_OK() : 	pv_snprintfP_ERR();
		return;
	}

	// RTC SRAM
	// write rtcram pos string
	if (!strcmp_P( strupr(argv[1]), PSTR("RTCRAM\0"))  && ( tipo_usuario == USER_TECNICO) ) {
		( RTCSRAM_test_write ( argv[2], argv[3] ) > 0)?  pv_snprintfP_OK() : 	pv_snprintfP_ERR();
		return;
	}

	// SENS12V
	// write sens12V {on|off}
	if (!strcmp_P( strupr(argv[1]), PSTR("SENS12V\0")) && ( tipo_usuario == USER_TECNICO) ) {

		if (!strcmp_P( strupr(argv[2]), PSTR("ON\0")) ) {
			IO_set_SENS_12V_CTL();
			pv_snprintfP_OK();
		} else if  (!strcmp_P( strupr(argv[2]), PSTR("OFF\0")) ) {
			IO_clr_SENS_12V_CTL();
			pv_snprintfP_OK();
		} else {
			xprintf_P( PSTR("cmd ERROR: ( write sens12V on{off} )\r\n\0"));
			pv_snprintfP_ERR();
		}
		return;
	}

	// INA
	// write ina id rconfValue
	// Solo escribimos el registro 0 de configuracion.
	if (!strcmp_P( strupr(argv[1]), PSTR("INA\0")) && ( tipo_usuario == USER_TECNICO) ) {
		( INA_test_write ( argv[2], argv[3] ) > 0)?  pv_snprintfP_OK() : 	pv_snprintfP_ERR();
		return;
	}


	// CMD NOT FOUND
	xprintf_P( PSTR("ERROR\r\nCMD NOT DEFINED\r\n\0"));
	return;
}
//------------------------------------------------------------------------------------
static void cmdReadFunction(void)
{

	FRTOS_CMD_makeArgv();

	// FUSES
 	if (!strcmp_P( strupr(argv[1]), PSTR("FUSES\0"))) {
 		pv_cmd_read_fuses();
 		return;
 	}

	// WMK
 	if (!strcmp_P( strupr(argv[1]), PSTR("WMK\0")) && ( tipo_usuario == USER_TECNICO) ) {
 		pv_cmd_print_stack_watermarks();
 		return;
 	}

 	// WDT
 	if (!strcmp_P( strupr(argv[1]), PSTR("WDT\0")) && ( tipo_usuario == USER_TECNICO) ) {
 		ctl_print_wdg_timers();
 		return;
 	}

	// RTC
	// read rtc
	if (!strcmp_P( strupr(argv[1]), PSTR("RTC\0")) ) {
		RTC_read_time();
		return;
	}

	// EE
	// read ee address length
	if (!strcmp_P( strupr(argv[1]), PSTR("EE\0")) && ( tipo_usuario == USER_TECNICO) ) {
		 EE_test_read ( argv[2], argv[3] );
		return;
	}

	// NVMEE
	// read nvmee address length
	if (!strcmp_P( strupr(argv[1]), PSTR("NVMEE\0")) && ( tipo_usuario == USER_TECNICO) ) {
		NVMEE_test_read ( argv[2], argv[3] );
		return;
	}

	// RTC SRAM
	// read rtcram address length
	if (!strcmp_P( strupr(argv[1]), PSTR("RTCRAM\0")) && ( tipo_usuario == USER_TECNICO) ) {
		RTCSRAM_test_read ( argv[2], argv[3] );
		return;
	}

	// INA
	// read ina id regName
	if (!strcmp_P( strupr(argv[1]), PSTR("INA\0")) && ( tipo_usuario == USER_TECNICO) ) {
		 INA_test_read ( argv[2], argv[3] );
		return;
	}

	// FRAME
	// read frame
	if (!strcmp_P( strupr(argv[1]), PSTR("FRAME\0")) ) {
		data_show_frame( true );
		return;
	}

	// BATTERY
	// read battery
	if (!strcmp_P( strupr(argv[1]), PSTR("BATTERY\0")) ) {
		pv_cmd_read_battery();
		return;
	}

	// ACH { 0..4}
	// read ach x
	if (!strcmp_P( strupr(argv[1]), PSTR("ACH\0")) && ( tipo_usuario == USER_TECNICO) ) {
		pv_cmd_read_analog_channel();
		return;
	}

	// DIN
	// read din
	if (!strcmp_P( strupr(argv[1]), PSTR("DIN\0")) && ( tipo_usuario == USER_TECNICO) ) {
		pv_cmd_read_digital_channels();
		return;
	}

	// MEMORY
	// read memory
	if (!strcmp_P( strupr(argv[1]), PSTR("MEMORY\0")) && ( tipo_usuario == USER_TECNICO) ) {
		pv_cmd_read_memory();
		return;
	}


	// CMD NOT FOUND
	xprintf_P( PSTR("ERROR\r\nCMD NOT DEFINED\r\n\0"));
	return;

}
//------------------------------------------------------------------------------------
static void cmdClearScreen(void)
{
	// ESC [ 2 J
	xprintf_P( PSTR("\x1B[2J\0"));
}
//------------------------------------------------------------------------------------
static void cmdConfigFunction(void)
{

bool retS = false;

	FRTOS_CMD_makeArgv();

	// COUNTERS
	// config counter {0..1} cname magPP
	if (!strcmp_P( strupr(argv[1]), PSTR("COUNTER\0")) ) {
		retS = u_config_counter_channel( atoi(argv[2]), argv[3], argv[4] );
		retS ? pv_snprintfP_OK() : pv_snprintfP_ERR();
		return;
	}

	// CDTIME ( counter_debounce_time )
	// config cdtime { val }
	if (!strcmp_P( strupr(argv[1]), PSTR("CDTIME\0")) ) {
		pv_cmd_config_counter_debounce_time( argv[2] );
		pv_snprintfP_OK();
		return;
	}

	// DEBUG
	// config debug
	if (!strcmp_P( strupr(argv[1]), PSTR("DEBUG\0"))) {
		if (!strcmp_P( strupr(argv[2]), PSTR("NONE\0"))) {
			systemVars.debug = DEBUG_NONE;
			retS = true;
		} else if (!strcmp_P( strupr(argv[2]), PSTR("COUNTER\0"))) {
			systemVars.debug = DEBUG_COUNTER;
			retS = true;
		} else if (!strcmp_P( strupr(argv[2]), PSTR("DATA\0"))) {
			systemVars.debug = DEBUG_DATA;
			retS = true;
		} else {
			retS = false;
		}
		retS ? pv_snprintfP_OK() : 	pv_snprintfP_ERR();
		return;
	}

	// DEFAULT
	// config default
	if (!strcmp_P( strupr(argv[1]), PSTR("DEFAULT\0"))) {
		u_load_defaults();
		pv_snprintfP_OK();
		return;
	}

	// SAVE
	// config save
	if (!strcmp_P( strupr(argv[1]), PSTR("SAVE\0"))) {
		u_save_params_in_NVMEE();
		pv_snprintfP_OK();
		return;
	}

	// TIMERPOLL
	// config timerpoll
	if (!strcmp_P( strupr(argv[1]), PSTR("TIMERPOLL\0")) ) {
		u_config_timerpoll( argv[2] );
		pv_snprintfP_OK();
		return;
	}

	// OFFSET
	// config offset {ch} {mag}
	if (!strcmp_P( strupr(argv[1]), PSTR("OFFSET\0")) ) {
		pv_cmd_config_offset( argv[2], argv[3] ) ? pv_snprintfP_OK() : pv_snprintfP_ERR();
		return;
	}

	// AUTOCAL
	// config autocal {ch} {mag}
	if (!strcmp_P( strupr(argv[1]), PSTR("AUTOCAL\0")) ) {
		pv_cmd_autocalibrar( argv[2], argv[3] ) ? pv_snprintfP_OK() : pv_snprintfP_ERR();
		return;
	}

	// SENSORTIME
	// config sensortime
	if (!strcmp_P( strupr(argv[1]), PSTR("SENSORTIME\0")) ) {
		pv_cmd_config_sensortime( argv[2] );
		pv_snprintfP_OK();
		return;
	}

	// INASPAN
	// config inaspan
	if (!strcmp_P( strupr(argv[1]), PSTR("INASPAN\0"))) {
		pv_cmd_config_inaspan( atoi(argv[2]), argv[3] );
		pv_snprintfP_OK();
		return;
	}

	// ANALOG
	// config analog {0..n} aname imin imax mmin mmax
	if (!strcmp_P( strupr(argv[1]), PSTR("ANALOG\0")) ) {
		u_config_analog_channel( atoi(argv[2]), argv[3], argv[4], argv[5], argv[6], argv[7] ) ? pv_snprintfP_OK() : pv_snprintfP_ERR();
		return;
	}

	// DIGITAL
	// config digital {0..1} dname
	if (!strcmp_P( strupr(argv[1]), PSTR("DIGITAL\0")) ) {
		u_config_digital_channel( atoi(argv[2]), argv[3]) ? pv_snprintfP_OK() : pv_snprintfP_ERR();
		return;
	}

	// RANGEMETER
	// rangemeter {on|off}
	if (!strcmp_P( strupr(argv[1]), PSTR("RANGEMETER\0"))) {

		if ( !strcmp_P( strupr(argv[2]), PSTR("ON\0"))) {
			systemVars.rangeMeter_enabled = true;
			pv_snprintfP_OK();
		} else if ( !strcmp_P( strupr(argv[2]), PSTR("OFF\0"))) {
			systemVars.rangeMeter_enabled = false;
			pv_snprintfP_OK();
		} else {
			pv_snprintfP_ERR();
		}
		return;
	}

	pv_snprintfP_ERR();
	return;
}
//------------------------------------------------------------------------------------
static void cmdHelpFunction(void)
{

	FRTOS_CMD_makeArgv();

	// HELP WRITE
	if (!strcmp_P( strupr(argv[1]), PSTR("WRITE\0"))) {
		xprintf_P( PSTR("-write\r\n\0"));
		xprintf_P( PSTR("  rtc YYMMDDhhmm\r\n\0"));
		if ( tipo_usuario == USER_TECNICO ) {
			xprintf_P( PSTR("  (ee,nvmee,rtcram) {pos} {string}\r\n\0"));
			xprintf_P( PSTR("  ina {id} {rconfValue}, sens12V {on|off}\r\n\0"));
		}
		return;

		return;
	}

	// HELP READ
	else if (!strcmp_P( strupr(argv[1]), PSTR("READ\0"))) {
		xprintf_P( PSTR("-read\r\n\0"));
		xprintf_P( PSTR("  rtc, frame, fuses\r\n\0"));
		if ( tipo_usuario == USER_TECNICO ) {
			xprintf_P( PSTR("  id\r\n\0"));
			xprintf_P( PSTR("  (ee,nvmee,rtcram) {pos} {lenght}\r\n\0"));
			xprintf_P( PSTR("  ina (id) {conf|chXshv|chXbusv|mfid|dieid}\r\n\0"));
			xprintf_P( PSTR("  ach {0..4}, battery\r\n\0"));
			xprintf_P( PSTR("  din\r\n\0"));
			xprintf_P( PSTR("  memory\r\n\0"));
		}
		return;

	}

	// HELP RESET
	else if (!strcmp_P( strupr(argv[1]), PSTR("RESET\0"))) {
		xprintf_P( PSTR("-reset\r\n\0"));
		xprintf_P( PSTR("  memory {soft|hard}\r\n\0"));
		return;

	}

	// HELP CONFIG
	else if (!strcmp_P( strupr(argv[1]), PSTR("CONFIG\0"))) {
		xprintf_P( PSTR("-config\r\n\0"));
		xprintf_P( PSTR("  user {normal|tecnico}\r\n\0"));
		xprintf_P( PSTR("  debug {none,counter,data}\r\n\0"));
		xprintf_P( PSTR("  counter {0..%d} cname magPP\r\n\0"), ( NRO_COUNTERS - 1 ) );
		xprintf_P( PSTR("  cdtime {val}\r\n\0"));
		xprintf_P( PSTR("  analog {0..%d} aname imin imax mmin mmax\r\n\0"),( NRO_ANINPUTS - 1 ) );
		xprintf_P( PSTR("  timerpoll {val}, sensortime {val}\r\n\0"));
		xprintf_P( PSTR("  offset {ch} {mag}, inaspan {ch} {mag}\r\n\0"));
		xprintf_P( PSTR("  autocal {ch} {mag}\r\n\0"));
		xprintf_P( PSTR("  digital {0..%d} dname\r\n\0"), ( NRO_DINPUTS - 1 ) );
		if ( spx_io_board == SPX_IO5CH ) {
			xprintf_P( PSTR("  rangemeter {on|off}\r\n\0"));
		}
		xprintf_P( PSTR("  default\r\n\0"));
		xprintf_P( PSTR("  save\r\n\0"));
	}

	// HELP KILL
	else if (!strcmp_P( strupr(argv[1]), PSTR("KILL\0")) && ( tipo_usuario == USER_TECNICO) ) {
		xprintf_P( PSTR("-kill {counter, data }\r\n\0"));
		return;

	} else {

		// HELP GENERAL
		xprintf_P( PSTR("\r\nSpymovil %s %s %s %s\r\n\0"), SPX_HW_MODELO, SPX_FTROS_VERSION, SPX_FW_REV, SPX_FW_DATE);
		xprintf_P( PSTR("Clock %d Mhz, Tick %d Hz\r\n\0"),SYSMAINCLK, configTICK_RATE_HZ );
		xprintf_P( PSTR("Available commands are:\r\n\0"));
		xprintf_P( PSTR("-cls\r\n\0"));
		xprintf_P( PSTR("-help\r\n\0"));
		xprintf_P( PSTR("-status\r\n\0"));
		xprintf_P( PSTR("-reset...\r\n\0"));
		xprintf_P( PSTR("-kill...\r\n\0"));
		xprintf_P( PSTR("-write...\r\n\0"));
		xprintf_P( PSTR("-read...\r\n\0"));
		xprintf_P( PSTR("-config...\r\n\0"));

	}

	xprintf_P( PSTR("\r\n\0"));

}
//------------------------------------------------------------------------------------
static void cmdKillFunction(void)
{

	FRTOS_CMD_makeArgv();

	// KILL COUNTER
	if (!strcmp_P( strupr(argv[1]), PSTR("COUNTER\0"))) {
		vTaskSuspend( xHandle_tkCounter );
		ctl_watchdog_kick(WDG_COUNT, 0xFFFF);
		return;
	}

	// KILL DATA
	if (!strcmp_P( strupr(argv[1]), PSTR("DATA\0"))) {
		vTaskSuspend( xHandle_tkData );
		ctl_watchdog_kick(WDG_DAT, 0xFFFF);
		return;
	}

	pv_snprintfP_OK();
	return;
}
//------------------------------------------------------------------------------------
static void pv_snprintfP_OK(void )
{
	xprintf_P( PSTR("ok\r\n\0"));
}
//------------------------------------------------------------------------------------
static void pv_snprintfP_ERR(void)
{
	xprintf_P( PSTR("error\r\n\0"));
}
//------------------------------------------------------------------------------------
static void pv_cmd_read_fuses(void)
{
	// Lee los fuses.

uint8_t fuse0,fuse1,fuse2,fuse4,fuse5;

	fuse0 = nvm_fuses_read(0x00);	// FUSE0
	xprintf_P( PSTR("FUSE0=0x%x\r\n\0"),fuse0);

	fuse1 = nvm_fuses_read(0x01);	// FUSE1
	xprintf_P( PSTR("FUSE1=0x%x\r\n\0"),fuse1);

	fuse2 = nvm_fuses_read(0x02);	// FUSE2
	xprintf_P( PSTR("FUSE2=0x%x\r\n\0"),fuse2);

	fuse4 = nvm_fuses_read(0x04);	// FUSE4
	xprintf_P( PSTR("FUSE4=0x%x\r\n\0"),fuse4);

	fuse5 = nvm_fuses_read(0x05);	// FUSE5
	xprintf_P( PSTR("FUSE5=0x%x\r\n\0"),fuse5);

	if ( (fuse0 != 0xFF) || ( fuse1 != 0xAA) || (fuse2 != 0xFD) || (fuse4 != 0xF5) || ( fuse5 != 0xD6) ) {
		xprintf_P( PSTR("FUSES ERROR !!!.\r\n\0"));
		xprintf_P( PSTR("Los valores deben ser: FUSE0=0xFF,FUSE1=0xAA,FUSE2=0xFD,FUSE4=0xF5,FUSE5=0xD6\r\n\0"));
		xprintf_P( PSTR("Deben reconfigurarse !!.\r\n\0"));
		pv_snprintfP_ERR();
		return;
	}
	pv_snprintfP_OK();
	return;
}
//------------------------------------------------------------------------------------
static bool pv_cmd_config_offset( char *s_channel, char *s_offset )
{
	// Configuro el parametro offset de un canal analogico.

uint8_t channel;
float offset;

	channel = atoi(s_channel);
	if ( ( channel >=  0) && ( channel < NRO_ANINPUTS ) ) {
		offset = atof(s_offset);
		systemVars.ain_offset[channel] = offset;
		return(true);
	}

	return(false);
}
//------------------------------------------------------------------------------------
static bool pv_cmd_autocalibrar( char *s_channel, char *s_mag_val )
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
	an_raw_val = AINPUTS_read_ina( spx_io_board, channel );
//	xprintf_P( PSTR("ANRAW=%d\r\n\0"), an_raw_val );

	// Convierto el raw_value a la magnitud
	I = (float)( an_raw_val) * 20 / ( systemVars.ain_inaspan[channel] + 1);
	P = 0;
	D = systemVars.ain_imax[channel] - systemVars.ain_imin[channel];

	an_mag_val = 0.0;
	if ( D != 0 ) {
		// Pendiente
		P = (float) ( systemVars.ain_mmax[channel]  -  systemVars.ain_mmin[channel] ) / D;
		// Magnitud
		M = (float) (systemVars.ain_mmin[channel] + ( I - systemVars.ain_imin[channel] ) * P);

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

	systemVars.ain_offset[channel] = offset;

	xprintf_P( PSTR("OFFSET=%.02f\r\n\0"), systemVars.ain_offset[channel] );

	return(true);

}
//------------------------------------------------------------------------------------
static void pv_cmd_config_sensortime ( char *s_sensortime )
{
	// Configura el tiempo de espera entre que prendo  la fuente de los sensores y comienzo el poleo.
	// Se utiliza solo desde el modo comando.
	// El tiempo de espera debe estar entre 1s y 15s

	while ( xSemaphoreTake( sem_SYSVars, ( TickType_t ) 5 ) != pdTRUE )
		taskYIELD();

	systemVars.pwr_settle_time = atoi(s_sensortime);

	if ( systemVars.pwr_settle_time < 1 )
		systemVars.pwr_settle_time = 1;

	if ( systemVars.pwr_settle_time > 15 )
		systemVars.pwr_settle_time = 15;

	xSemaphoreGive( sem_SYSVars );
	return;
}
//------------------------------------------------------------------------------------
static void pv_cmd_config_inaspan ( uint8_t channel, char *s_span )
{
	// Configura el factor de correccion del span de canales delos INA.
	// Esto es debido a que las resistencias presentan una tolerancia entonces con
	// esto ajustamos que con 20mA den la excursión correcta.
	// Solo de configura desde modo comando.

uint16_t span;

	while ( xSemaphoreTake( sem_SYSVars, ( TickType_t ) 5 ) != pdTRUE )
		taskYIELD();

	span = atoi(s_span);
	systemVars.ain_inaspan[channel] = span;

	xSemaphoreGive( sem_SYSVars );
	return;

}
//------------------------------------------------------------------------------------
static void pv_cmd_config_counter_debounce_time( char *s_counter_debounce_time )
{
	// Configura el tiempo de debounce del conteo de pulsos

	while ( xSemaphoreTake( sem_SYSVars, ( TickType_t ) 5 ) != pdTRUE )
		taskYIELD();

	systemVars.counter_debounce_time = atoi(s_counter_debounce_time);

	if ( systemVars.counter_debounce_time < 1 )
		systemVars.counter_debounce_time = 50;

	xSemaphoreGive( sem_SYSVars );
	return;
}
//------------------------------------------------------------------------------------
static void pv_cmd_print_stack_watermarks(void)
{

UBaseType_t uxHighWaterMark;

	// tkIdle
	uxHighWaterMark = uxTaskGetStackHighWaterMark( xHandle_idle );
	xprintf_P( PSTR("IDLE:%03d,%03d,[%03d]\r\n\0"),configMINIMAL_STACK_SIZE,uxHighWaterMark,(configMINIMAL_STACK_SIZE - uxHighWaterMark)) ;

	// tkCmd
	uxHighWaterMark = uxTaskGetStackHighWaterMark( xHandle_tkCmd );
	xprintf_P( PSTR("CMD: %03d,%03d,[%03d]\r\n\0"),tkCmd_STACK_SIZE,uxHighWaterMark,(tkCmd_STACK_SIZE - uxHighWaterMark)) ;

	// tkControl
	uxHighWaterMark = uxTaskGetStackHighWaterMark( xHandle_tkCtl );
	xprintf_P( PSTR("CTL: %03d,%03d,[%03d]\r\n\0"),tkCtl_STACK_SIZE,uxHighWaterMark, (tkCtl_STACK_SIZE - uxHighWaterMark));

}
//------------------------------------------------------------------------------------
static void pv_cmd_read_battery(void)
{

float battery;

	AINPUTS_prender_12V();
	INA_config(1, CONF_INAS_AVG128 );
	battery = 0.008 * AINPUTS_read_battery();
	INA_config(1, CONF_INAS_SLEEP );
	AINPUTS_apagar_12V();

	xprintf_P( PSTR("Battery=%.02f\r\n\0"), battery );

}
//------------------------------------------------------------------------------------
static void pv_cmd_read_analog_channel(void)
{

uint16_t raw_val;
float mag_val;


	if ( atoi(argv[2]) <  NRO_ANINPUTS ) {
		u_read_analog_channel( spx_io_board, atoi(argv[2]),&raw_val, &mag_val );
		xprintf_P( PSTR("anCH[%02d] raw=%d,mag=%.02f\r\n\0"),atoi(argv[2]),raw_val, mag_val );
	} else {
		pv_snprintfP_ERR();
	}
}
//------------------------------------------------------------------------------------
static void pv_cmd_read_digital_channels(void)
{

	// Leo e imprimo todos los canales digitales a la vez.

uint8_t din0, din1;

	if ( spx_io_board == SPX_IO5CH ) {
		// Leo los canales digitales.
		din0 = DIN_read_DIN0();
		din1 = DIN_read_DIN1();
		xprintf_P( PSTR("Din0: %d, Din1: %d\r\n\0"), din0, din1 );
		return;

	}
}
//------------------------------------------------------------------------------------
static void pv_cmd_read_memory(void)
{
	// Si hay datos en memoria los lee todos y los muestra en pantalla
	// Leemos la memoria e imprimo los datos.
	// El problema es que si hay muchos datos puede excederse el tiempo de watchdog y
	// resetearse el dlg.
	// Para esto, cada 32 registros pateo el watchdog.
/*
StatBuffer_t pxFFStatBuffer;
frameData_t Aframe;
size_t bRead;
uint16_t rcds = 0;

	FF_seek();
	while(1) {
		bRead = FF_fread( &Aframe, sizeof(Aframe));

		if ( bRead == 0) {
			break;
		}

		if ( ( rcds++ % 32) == 0 ) {
			pub_control_watchdog_kick(WDG_CMD, WDG_CMD_TIMEOUT);
		}

		// imprimo
		FF_stat(&pxFFStatBuffer);
		FRTOS_snprintf_P( cmd_printfBuff, sizeof(cmd_printfBuff),  PSTR("RD:[%d/%d/%d][%d/%d] "), pxFFStatBuffer.HEAD,pxFFStatBuffer.RD, pxFFStatBuffer.TAIL,pxFFStatBuffer.rcdsFree,pxFFStatBuffer.rcds4del);
		FreeRTOS_write( &pdUART1, cmd_printfBuff, sizeof(cmd_printfBuff) );

		pub_analog_print_frame(&Aframe);

	}
*/
}
//------------------------------------------------------------------------------------

