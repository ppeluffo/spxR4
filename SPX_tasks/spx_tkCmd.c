/*
 * sp5K_tkCmd.c
 *
 *  Created on: 27/12/2013
 *      Author: root
 */

#include "spx.h"
#include "gprs.h"

//----------------------------------------------------------------------------------------
// FUNCIONES DE USO PRIVADO
//----------------------------------------------------------------------------------------
static void pv_snprintfP_OK(void );
static void pv_snprintfP_ERR(void);
static void pv_cmd_read_fuses(void);

static void pv_cmd_print_stack_watermarks(void);
static void pv_cmd_read_battery(void);
static void pv_cmd_read_analog_channel(void);
static void pv_cmd_read_digital_channels(void);
static void pv_cmd_read_memory(void);
static void pv_cmd_rwGPRS(uint8_t cmd_mode );
static void pv_cmd_rwMCP(uint8_t cmd_mode );
static void pv_cmd_I2Cscan(void);

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
static void cmdPeekFunction(void);
static void cmdPokeFunction(void);

#define WR_CMD 0
#define RD_CMD 1

#define WDG_CMD_TIMEOUT	30

static usuario_t tipo_usuario;
RtcTimeType_t rtc;

st_dataRecord_t dr;
dataframe_s df;

//------------------------------------------------------------------------------------
void tkCmd(void * pvParameters)
{

uint8_t c = 0;
uint8_t ticks = 0;

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
	FRTOS_CMD_register( "peek\0", cmdPeekFunction );
	FRTOS_CMD_register( "poke\0", cmdPokeFunction );

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

	//	PORTF.OUTTGL = 0x02;	// Toggle F1 Led Comms

		// Si no tengo terminal conectada, duermo 25s lo que me permite entrar en tickless.
		while ( ! ctl_terminal_connected() ) {
			ctl_watchdog_kick(WDG_CMD, WDG_CMD_TIMEOUT);
			vTaskDelay( ( TickType_t)( 25000 / portTICK_RATE_MS ) );
		}

		c = '\0';	// Lo borro para que luego del un CR no resetee siempre el timer.
		// el read se bloquea 50ms. lo que genera la espera.
		//while ( CMD_read( (char *)&c, 1 ) == 1 ) {
		while ( frtos_read( fdTERM, (char *)&c, 1 ) == 1 ) {
			FRTOS_CMD_process(c);
		}

	}
}
//------------------------------------------------------------------------------------
static void cmdStatusFunction(void)
{

FAT_t l_fat;
uint8_t channel = 0;
uint8_t olatb = 0 ;
uint8_t VA_cnt, VB_cnt, VA_status, VB_status;

	memset( &l_fat, '\0', sizeof(FAT_t));

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

	xprintf_P( PSTR("sVars Size: %d\r\n\0"), sizeof(systemVars) );
	xprintf_P( PSTR("dr Size: %d\r\n\0"), sizeof(st_dataRecord_t) );

	RTC_read_time();

	// DlgId
	xprintf_P( PSTR("dlgid: %s\r\n\0"), systemVars.gprs_conf.dlgId );

	// Memoria
	FAT_read(&l_fat);
	xprintf_P( PSTR("memory: rcdSize=%d, wrPtr=%d,rdPtr=%d,delPtr=%d,r4wr=%d,r4rd=%d,r4del=%d \r\n\0"), sizeof(st_dataRecord_t), l_fat.wrPTR,l_fat.rdPTR, l_fat.delPTR,l_fat.rcds4wr,l_fat.rcds4rd,l_fat.rcds4del );

	// SERVER
	xprintf_P( PSTR(">Server:\r\n\0"));
	xprintf_P( PSTR("  apn: %s\r\n\0"), systemVars.gprs_conf.apn );
	xprintf_P( PSTR("  server ip:port: %s:%s\r\n\0"), systemVars.gprs_conf.server_ip_address, systemVars.gprs_conf.server_tcp_port );
	xprintf_P( PSTR("  server script: %s\r\n\0"), systemVars.gprs_conf.serverScript );
	xprintf_P( PSTR("  simpwd: %s\r\n\0"), systemVars.gprs_conf.simpwd );

	// MODEM
	xprintf_P( PSTR(">Modem:\r\n\0"));
	xprintf_P( PSTR("  signalQ: csq=%d, dBm=%d\r\n\0"), GPRS_stateVars.csq, GPRS_stateVars.dbm );
	xprintf_P( PSTR("  ip address: %s\r\n\0"), GPRS_stateVars.dlg_ip_address);

	// GPRS STATE
	switch (GPRS_stateVars.state) {
	case G_ESPERA_APAGADO:
		xprintf_P( PSTR("  state: await_off\r\n"));
		break;
	case G_PRENDER:
		xprintf_P( PSTR("  state: prendiendo\r\n"));
		break;
	case G_CONFIGURAR:
		xprintf_P( PSTR("  state: configurando\r\n"));
		break;
	case G_MON_SQE:
		xprintf_P( PSTR("  state: mon_sqe\r\n"));
		break;
	case G_SCAN_APN:
		xprintf_P( PSTR("  state: scan apn\r\n"));
		break;
	case G_GET_IP:
		xprintf_P( PSTR("  state: ip\r\n"));
		break;
	case G_INIT_FRAME:
		xprintf_P( PSTR("  state: init frame\r\n"));
		break;
	case G_DATA:
		xprintf_P( PSTR("  state: data\r\n"));
		break;
	default:
		xprintf_P( PSTR("  state: ERROR\r\n"));
		break;
	}

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
	case DEBUG_DATA:
		xprintf_P( PSTR("  debug: data\r\n\0") );
		break;
	case DEBUG_GPRS:
		xprintf_P( PSTR("  debug: gprs\r\n\0") );
		break;
	case DEBUG_PILOTO:
		xprintf_P( PSTR("  debug: piloto\r\n\0") );
		break;
	default:
		xprintf_P( PSTR("  debug: ???\r\n\0") );
		break;
	}

	// Timerpoll
	xprintf_P( PSTR("  timerPoll: [%d s]/ %d\r\n\0"),systemVars.timerPoll, ctl_readTimeToNextPoll() );

	// Timerdial
	xprintf_P( PSTR("  timerDial: [%d s]/\0"), systemVars.gprs_conf.timerDial );
	xprintf_P( PSTR(" %d\r\n\0"), u_gprs_read_timeToNextDial() );

	// Sensor Pwr Time
	xprintf_P( PSTR("  timerPwrSensor: [%d s]\r\n\0"), systemVars.ainputs_conf.pwr_settle_time );

	if ( spx_io_board == SPX_IO5CH ) {

		// PWR SAVE:
		if ( systemVars.gprs_conf.pwrSave.pwrs_enabled ==  false ) {
			xprintf_P(  PSTR("  pwrsave: OFF\r\n\0"));
		} else {
			xprintf_P(  PSTR("  pwrsave: ON, start[%02d:%02d], end[%02d:%02d]\r\n\0"), systemVars.gprs_conf.pwrSave.hora_start.hour, systemVars.gprs_conf.pwrSave.hora_start.min, systemVars.gprs_conf.pwrSave.hora_fin.hour, systemVars.gprs_conf.pwrSave.hora_fin.min);
		}

		// RangeMeter: PULSE WIDTH
		if ( systemVars.rangeMeter_enabled ) {
			xprintf_P( PSTR("  rangeMeter: ON\r\n"));
		} else {
			xprintf_P( PSTR("  rangeMeter: OFF\r\n"));
		}

		// Psensor
		// Psensor
		if ( strcmp ( systemVars.psensor_conf.name, "X" ) == 0 ) {
			xprintf_P( PSTR("  psensor: X\r\n\0"));
		} else {
			xprintf_P( PSTR("  psensor: %s (%d-%d / %.01f-%.01f)[offset=%0.02f]\r\n\0"), systemVars.psensor_conf.name, systemVars.psensor_conf.count_min, systemVars.psensor_conf.count_max, systemVars.psensor_conf.pmin, systemVars.psensor_conf.pmax, systemVars.psensor_conf.offset );
		}

	}

	// doutputs
	switch( systemVars.doutputs_conf.modo) {
	case OFF:
		xprintf_P( PSTR("  doutputs modo: OFF\r\n"));
		break;
	case CONSIGNA:
		// Consignas
		xprintf_P( PSTR("  doutputs modo: CONSIGNA\r\n"));
		if ( systemVars.doutputs_conf.consigna.c_aplicada == CONSIGNA_DIURNA ) {
			xprintf_P( PSTR("  consignas: (DIURNA) (c_dia=%02d:%02d, c_noche=%02d:%02d)\r\n"), systemVars.doutputs_conf.consigna.hhmm_c_diurna.hour, systemVars.doutputs_conf.consigna.hhmm_c_diurna.min, systemVars.doutputs_conf.consigna.hhmm_c_nocturna.hour, systemVars.doutputs_conf.consigna.hhmm_c_nocturna.min );
		} else {
			xprintf_P( PSTR("  consignas: (NOCTURNA) (c_dia=%02d:%02d, c_noche=%02d:%02d)\r\n"), systemVars.doutputs_conf.consigna.hhmm_c_diurna.hour, systemVars.doutputs_conf.consigna.hhmm_c_diurna.min, systemVars.doutputs_conf.consigna.hhmm_c_nocturna.hour, systemVars.doutputs_conf.consigna.hhmm_c_nocturna.min );
		}
		break;
	case PERFORACIONES:
		xprintf_P( PSTR("  doutputs modo: PERFORACION\r\n"));
		MCP_read( 0x15, (char *)&olatb, 1 );
		xprintf_P( PSTR("  out_value=%d(0x%02x)[[%c%c%c%c%c%c%c%c](olatb=0x%02x)\r\n\0"), systemVars.doutputs_conf.perforacion.outs, systemVars.doutputs_conf.perforacion.outs, BYTE_TO_BINARY( systemVars.doutputs_conf.perforacion.outs ), olatb );
		if ( systemVars.doutputs_conf.perforacion.control == CTL_BOYA ) {
			xprintf_P( PSTR("  out_control=BOYA, timer=%d\r\n\0"), perforaciones_read_clt_timer() );
		} else {
			xprintf_P( PSTR("  out_control=SISTEMA, timer=%d\r\n\0"), perforaciones_read_clt_timer() );
		}
		break;
	case PILOTOS:
		pilotos_readCounters(&VA_cnt, &VB_cnt, &VA_status, &VB_status );
		xprintf_P( PSTR("  doutputs modo: PILOTO\r\n"));
		//xprintf_P( PSTR("  pout_ref=%.02f\r\n\0"), systemVars.doutputs_conf.piloto.pout );
		xprintf_P( PSTR("  pband=%.02f\r\n\0"), systemVars.doutputs_conf.piloto.band );
		xprintf_P( PSTR("  max_steps=%d, VAp=%d, VBp=%d\r\n\0"), systemVars.doutputs_conf.piloto.max_steps, VA_cnt, VB_cnt );
		switch(systemVars.doutputs_conf.piloto.tipo_valvula) {
		case VR_CHICA:
			xprintf_P( PSTR("  VReg: chica\r\n"));
			break;
		case VR_MEDIA:
			xprintf_P( PSTR("  VReg: media\r\n"));
			break;
		case VR_GRANDE:
			xprintf_P( PSTR("  VReg: grande\r\n"));
			break;
		}
		xprintf_P( PSTR("  Slots: "));
		for ( channel = 0; channel < MAX_PILOTO_PSLOTS; channel++ ) {
			xprintf_P( PSTR("[%d]%02d:%02d->%.02f "), channel, systemVars.doutputs_conf.piloto.pSlots[channel].hhmm.hour, systemVars.doutputs_conf.piloto.pSlots[channel].hhmm.min, systemVars.doutputs_conf.piloto.pSlots[channel].pout );
		}
		xprintf_P( PSTR("\r\n\0"));
		break;
	}

	// aninputs
	for ( channel = 0; channel < NRO_ANINPUTS; channel++) {
		xprintf_P( PSTR("  a%d [%d-%d mA/ %.02f,%.02f | %04d | %.02f | %.03f | %.03f | %s]\r\n\0"),channel, systemVars.ainputs_conf.imin[channel], systemVars.ainputs_conf.imax[channel], systemVars.ainputs_conf.mmin[channel], systemVars.ainputs_conf.mmax[channel], systemVars.ainputs_conf.inaspan[channel], systemVars.ainputs_conf.offset[channel] , systemVars.ainputs_conf.ieq_min[channel] , systemVars.ainputs_conf.ieq_max[channel], systemVars.ainputs_conf.name[channel] );
	}

	// dinputs
	for ( channel = 0; channel <  NRO_DINPUTS; channel++) {
		xprintf_P( PSTR("  d%d [ %s | %d ]\r\n\0"),channel, systemVars.dinputs_conf.name[channel], systemVars.dinputs_conf.tpoll[channel] );
	}

	// contadores( Solo hay 2 )
	// Counter0
	xprintf_P( PSTR("  c0 [ %s | %.03f ] pw=%d,T=%d\r\n\0"), systemVars.counters_conf.name[0], systemVars.counters_conf.magpp[0], systemVars.counters_conf.pwidth[0], systemVars.counters_conf.period[0] );
	// Counter1
	if ( systemVars.counters_conf.speed[1] == CNT_LOW_SPEED ) {
			xprintf_P( PSTR("  c1 [ %s | %.03f ] pw=%d,T=%d (LS)\r\n\0"),systemVars.counters_conf.name[1], systemVars.counters_conf.magpp[1], systemVars.counters_conf.pwidth[1], systemVars.counters_conf.period[1] );
	} else {
		xprintf_P( PSTR("  c1 [ %s | %.03f ] pw=%d,T=%d (HS)\r\n\0"),systemVars.counters_conf.name[1], systemVars.counters_conf.magpp[1], systemVars.counters_conf.pwidth[1], systemVars.counters_conf.period[1] );
	}

	// Muestro los datos sin polear
	data_read_frame ( false );
}
//-----------------------------------------------------------------------------------
static void cmdResetFunction(void)
{
	// Resetea al micro prendiendo el watchdog

	FRTOS_CMD_makeArgv();

	// Reset memory ??
	if (!strcmp_P( strupr(argv[1]), PSTR("MEMORY\0"))) {

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

	// ANALOG
	// write analog wakeup/sleep
	if (!strcmp_P( strupr(argv[1]), PSTR("ANALOG\0")) && ( tipo_usuario == USER_TECNICO) ) {
		if (!strcmp_P( strupr(argv[2]), PSTR("WAKEUP\0")) ) {
			ainputs_awake();
			return;
		}
		if (!strcmp_P( strupr(argv[2]), PSTR(" SLEEP\0")) ) {
			ainputs_sleep();
			return;
		}
		return;
	}

	// MCP
	// write mcp regAddr data
	if (!strcmp_P( strupr(argv[1]), PSTR("MCP\0")) && ( tipo_usuario == USER_TECNICO) ) {
		pv_cmd_rwMCP(WR_CMD);
		return;
	}

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
		NVMEE_test_write ( argv[2], argv[3] );
		pv_snprintfP_OK();
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

	// OUTPUT
	// write output {0..7} {set | clear}
	if (!strcmp_P( strupr(argv[1]), PSTR("OUTPUT\0")) && ( tipo_usuario == USER_TECNICO) ) {
		doutputs_cmd_write_outputs( argv[2], argv[3] ) ?  pv_snprintfP_OK() : 	pv_snprintfP_ERR();
		return;
	}

	// DOUT
	// write dout VAL
	if (!strcmp_P( strupr(argv[1]), PSTR("DOUT\0")) && ( tipo_usuario == USER_TECNICO) ) {
		perforaciones_set_douts( atoi(argv[2]) );
		pv_snprintfP_OK();
		return;
	}


	// RANGE
	// write range {run | stop}
	if (!strcmp_P( strupr(argv[1]), PSTR("RANGE\0")) && ( tipo_usuario == USER_TECNICO) ) {
		if (!strcmp_P( strupr(argv[2]), PSTR("RUN\0")) ) {
			RMETER_start(); pv_snprintfP_OK(); return;
		}

		if (!strcmp_P( strupr(argv[2]), PSTR("STOP\0")) ) {
			RMETER_stop(); pv_snprintfP_OK();	return;
		}

		xprintf_P( PSTR("cmd ERROR: ( write range {run|stop} )\r\n\0"));
		return;
	}

	// CONSIGNA
	// write consigna (diurna|nocturna)
	if (!strcmp_P( strupr(argv[1]), PSTR("CONSIGNA\0")) && ( tipo_usuario == USER_TECNICO) ) {
		consigna_write( argv[2] ) ?  pv_snprintfP_OK() : 	pv_snprintfP_ERR();
		return;
	}

	// VALVE
	// write valve (enable|disable),(set|reset),(sleep|awake),(ph01|ph10) } {A/B}
	//             (open|close) (A|B) (ms)
	//              power {on|off}
	if (!strcmp_P( strupr(argv[1]), PSTR("VALVE\0")) && ( tipo_usuario == USER_TECNICO) ) {
		doutputs_cmd_write_valve( argv[2], argv[3] ) ?  pv_snprintfP_OK() : pv_snprintfP_ERR();
		return;
	}

	// GPRS
	// write gprs pwr|sw|rts {on|off}
	// write gprs cmd {atcmd}
	if (!strcmp_P( strupr(argv[1]), PSTR("GPRS\0")) && ( tipo_usuario == USER_TECNICO) ) {
		pv_cmd_rwGPRS(WR_CMD);
		return;
	}

	// CMD NOT FOUND
	xprintf_P( PSTR("ERROR\r\nCMD NOT DEFINED\r\n\0"));
	return;
}
//------------------------------------------------------------------------------------
static void cmdReadFunction(void)
{

int16_t range = 0;

	FRTOS_CMD_makeArgv();


	// TEMP
	// read temp
	if (!strcmp_P( strupr(argv[1]), PSTR("TEMP\0")) && ( tipo_usuario == USER_TECNICO) ) {
		tempsensor_test_read();
		return;
	}

	// PSENSOR
	// read psensor
	if (!strcmp_P( strupr(argv[1]), PSTR("PSENSOR\0")) && ( tipo_usuario == USER_TECNICO) ) {
		psensor_test_read();
		return;
	}

	// I2Cscan
	// read i2cscan busaddr
	if (!strcmp_P( strupr(argv[1]), PSTR("I2CSCAN\0")) && ( tipo_usuario == USER_TECNICO) ) {
		pv_cmd_I2Cscan();
		return;
	}

	// MCP
	// read mcp regAddr
	if (!strcmp_P( strupr(argv[1]), PSTR("MCP\0")) && ( tipo_usuario == USER_TECNICO) ) {
		pv_cmd_rwMCP(RD_CMD);
		return;
	}

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
		ainputs_awake();
		INA_test_read ( argv[2], argv[3] );
		ainputs_sleep();
		return;
	}

	// FRAME
	// read frame
	if (!strcmp_P( strupr(argv[1]), PSTR("FRAME\0")) ) {
		// Muestro los datos poleandolos y sin sacarlos por xbee.
		data_read_frame ( true );
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

	// RANGE
	// read range
	if ( ( spx_io_board == SPX_IO5CH ) && (!strcmp_P( strupr(argv[1]), PSTR("RANGE\0"))) && ( tipo_usuario == USER_TECNICO) ) {
		RMETER_ping( &range, false );
		xprintf_P( PSTR("RANGE=%d\r\n\0"),range);
		pv_snprintfP_OK();
		return;
	}

	// GPRS
	// read gprs (rsp,cts,dcd,ri)
	if (!strcmp_P( strupr(argv[1]), PSTR("GPRS\0")) && ( tipo_usuario == USER_TECNICO) ) {
		pv_cmd_rwGPRS(RD_CMD);
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

	// OUTMODE
	// outmode {none|cons | perf | plt
	if (!strcmp_P( strupr(argv[1]), PSTR("OUTMODE\0")) ) {
		retS = doutputs_config_mode( argv[2]);
		retS ? pv_snprintfP_OK() : pv_snprintfP_ERR();
		return;
	}

	// CONSIGNA
	// config consigna {hhmm1} {hhmm2}
	if (!strcmp_P( strupr(argv[1]), PSTR("CONSIGNA\0")) ) {
		retS = consigna_config( argv[2], argv[3]);
		retS ? pv_snprintfP_OK() : pv_snprintfP_ERR();
		return;
	}

	// PILOTO
	// config piloto { reg { CHICA | MEDIA | GRANDE },pout {pout},pband {pband},steps {steps},slot {idx} {hhmm} {pout} }\r\n\0"));
	if (!strcmp_P( strupr(argv[1]), PSTR("PILOTO\0")) ) {
		retS = piloto_config( argv[2], argv[3], argv[4], argv[5]);
		retS ? pv_snprintfP_OK() : pv_snprintfP_ERR();
		return;
	}

	// COUNTERS
	// config counter {0..1} cname magPP pulseWidth period speed
	if (!strcmp_P( strupr(argv[1]), PSTR("COUNTER\0")) ) {
		retS = counters_config_channel( atoi(argv[2]), argv[3], argv[4], argv[5], argv[6], argv[7] );
		retS ? pv_snprintfP_OK() : pv_snprintfP_ERR();
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
		} else if (!strcmp_P( strupr(argv[2]), PSTR("GPRS\0"))) {
			systemVars.debug = DEBUG_GPRS;
			retS = true;
		} else if (!strcmp_P( strupr(argv[2]), PSTR("OUTPUTS\0"))) {
			systemVars.debug = DEBUG_OUTPUTS;
			retS = true;
		} else if (!strcmp_P( strupr(argv[2]), PSTR("PILOTO\0"))) {
			systemVars.debug = DEBUG_PILOTO;
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
		u_load_defaults( strupr(argv[2]) );
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
		ainputs_config_offset( argv[2], argv[3] ) ? pv_snprintfP_OK() : pv_snprintfP_ERR();
		return;
	}

	// AUTOCAL
	// config autocal {ch,PSENSOR} {mag}
	if (!strcmp_P( strupr(argv[1]), PSTR("AUTOCAL\0")) ) {
		if (!strcmp_P( strupr(argv[2]), PSTR("PSENSOR\0")) ) {
			psensor_config_autocalibrar( argv[3] ) ? pv_snprintfP_OK() : pv_snprintfP_ERR();
			return;
		}
		ainputs_config_autocalibrar( argv[2], argv[3] ) ? pv_snprintfP_OK() : pv_snprintfP_ERR();
		return;
	}

	// ICAL
	// config ical {ch} {4 | 20}
	if (!strcmp_P( strupr(argv[1]), PSTR("ICAL\0")) ) {
		ainputs_config_ical( argv[2], argv[3] ) ? pv_snprintfP_OK() : pv_snprintfP_ERR();
		return;
	}

	// TIMRPWRSENSOR
	// config sensortime
	if (!strcmp_P( strupr(argv[1]), PSTR("TIMEPWRSENSOR\0")) ) {
		ainputs_config_timepwrsensor( argv[2] );
		pv_snprintfP_OK();
		return;
	}

	// INASPAN
	// config inaspan
	if (!strcmp_P( strupr(argv[1]), PSTR("INASPAN\0"))) {
		ainputs_config_span( argv[2], argv[3] );
		pv_snprintfP_OK();
		return;
	}

	// ANALOG
	// config analog {0..n} aname imin imax mmin mmax
	if (!strcmp_P( strupr(argv[1]), PSTR("ANALOG\0")) ) {
		ainputs_config_channel( atoi(argv[2]), argv[3], argv[4], argv[5], argv[6], argv[7] ) ? pv_snprintfP_OK() : pv_snprintfP_ERR();
		return;
	}

	// DIGITAL
	// config digital {0..N} dname tpoll
	if (!strcmp_P( strupr(argv[1]), PSTR("DIGITAL\0")) ) {
		dinputs_config_channel( atoi(argv[2]), argv[3], argv[4]) ? pv_snprintfP_OK() : pv_snprintfP_ERR();
		return;
	}

	// PSENSOR
	// config psensor name countMin countMax pmin pmax offset
	if (!strcmp_P( strupr(argv[1]), PSTR("PSENSOR\0")) ) {
		psensor_config( argv[2], argv[3], argv[4], argv[5], argv[6], argv[7] ) ? pv_snprintfP_OK() : pv_snprintfP_ERR();
		return;
	}

	// RANGEMETER
	// config rangemeter {on|off}
	if ( !strcmp_P( strupr(argv[1]), PSTR("RANGEMETER\0"))) {

		if ( range_config(argv[2]) ) {
			pv_snprintfP_OK();
			return;
		} else {
			pv_snprintfP_ERR();
			return;
		}

		pv_snprintfP_ERR();
		return;
	}


	// TIMERDIAL
	// config timerdial
	if ( ( spx_io_board == SPX_IO5CH ) && (!strcmp_P( strupr(argv[1]), PSTR("TIMERDIAL\0"))) ) {
		u_gprs_config_timerdial( argv[2] );
		pv_snprintfP_OK();
		return;
	}

	// PWRSAVE
	if (!strcmp_P( strupr(argv[1]), PSTR("PWRSAVE\0"))) {
		u_gprs_configPwrSave ( argv[2], argv[3], argv[4] );
		pv_snprintfP_OK();
		return;
	}

	// APN
	if (!strcmp_P( strupr(argv[1]), PSTR("APN\0"))) {
		if ( argv[2] == NULL ) {
			retS = false;
		} else {
			memset(systemVars.gprs_conf.apn, '\0', sizeof(systemVars.gprs_conf.apn));
			memcpy(systemVars.gprs_conf.apn, argv[2], sizeof(systemVars.gprs_conf.apn));
			systemVars.gprs_conf.apn[APN_LENGTH - 1] = '\0';
			retS = true;
		}
		retS ? pv_snprintfP_OK() : 	pv_snprintfP_ERR();
		return;
	}

	// PORT ( SERVER IP PORT)
	if (!strcmp_P( strupr(argv[1]), PSTR("PORT\0"))) {
		if ( argv[2] == NULL ) {
			retS = false;
		} else {
			memset(systemVars.gprs_conf.server_tcp_port, '\0', sizeof(systemVars.gprs_conf.server_tcp_port));
			memcpy(systemVars.gprs_conf.server_tcp_port, argv[2], sizeof(systemVars.gprs_conf.server_tcp_port));
			systemVars.gprs_conf.server_tcp_port[PORT_LENGTH - 1] = '\0';
			retS = true;
		}
		retS ? pv_snprintfP_OK() : 	pv_snprintfP_ERR();
		return;
	}

	// IP (SERVER IP ADDRESS)
	if (!strcmp_P( strupr(argv[1]), PSTR("IP\0"))) {
		if ( argv[2] == NULL ) {
			retS = false;
		} else {
			memset(systemVars.gprs_conf.server_ip_address, '\0', sizeof(systemVars.gprs_conf.server_ip_address));
			memcpy(systemVars.gprs_conf.server_ip_address, argv[2], sizeof(systemVars.gprs_conf.server_ip_address));
			systemVars.gprs_conf.server_ip_address[IP_LENGTH - 1] = '\0';
			retS = true;
		}
		retS ? pv_snprintfP_OK() : 	pv_snprintfP_ERR();
		return;
	}

	// SCRIPT ( SERVER SCRIPT SERVICE )
	if (!strcmp_P( strupr(argv[1]), PSTR("SCRIPT\0"))) {
		if ( argv[2] == NULL ) {
			retS = false;
		} else {
			memset(systemVars.gprs_conf.serverScript, '\0', sizeof(systemVars.gprs_conf.serverScript));
			memcpy(systemVars.gprs_conf.serverScript, argv[2], sizeof(systemVars.gprs_conf.serverScript));
			systemVars.gprs_conf.serverScript[SCRIPT_LENGTH - 1] = '\0';
			retS = true;
		}
		retS ? pv_snprintfP_OK() : 	pv_snprintfP_ERR();
		return;
	}

	// SIMPWD
	if (!strcmp_P( strupr(argv[1]), PSTR("SIMPWD\0"))) {
		if ( argv[2] == NULL ) {
			retS = false;
		} else {
			memset(systemVars.gprs_conf.simpwd, '\0', sizeof(systemVars.gprs_conf.simpwd));
			memcpy(systemVars.gprs_conf.simpwd, argv[2], sizeof(systemVars.gprs_conf.simpwd));
			systemVars.gprs_conf.simpwd[PASSWD_LENGTH - 1] = '\0';
			retS = true;
		}
		retS ? pv_snprintfP_OK() : 	pv_snprintfP_ERR();
		return;
	}

	// DLGID
	if (!strcmp_P( strupr(argv[1]), PSTR("DLGID\0"))) {
		if ( argv[2] == NULL ) {
			retS = false;
			} else {
				memset(systemVars.gprs_conf.dlgId,'\0', sizeof(systemVars.gprs_conf.dlgId) );
				memcpy(systemVars.gprs_conf.dlgId, argv[2], sizeof(systemVars.gprs_conf.dlgId));
				systemVars.gprs_conf.dlgId[DLGID_LENGTH - 1] = '\0';
				retS = true;
			}
		retS ? pv_snprintfP_OK() : 	pv_snprintfP_ERR();
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
			xprintf_P( PSTR("  analog (wakeup | sleep )\r\n\0"));

			if ( spx_io_board == SPX_IO8CH ) {
				xprintf_P( PSTR("  mcp {regAddr} {data}, mcpinit\r\n\0"));
				xprintf_P( PSTR("  dout VAL\r\n\0"));
			}

			if ( systemVars.rangeMeter_enabled == true ) {
				xprintf_P( PSTR("  range {run | stop}\r\n\0"));
			}

			if ( spx_io_board == SPX_IO5CH ) {
				xprintf_P( PSTR("  consigna (diurna|nocturna)\r\n\0"));
				xprintf_P( PSTR("  valve (enable|disable),(set|reset),(sleep|awake),(ph01|ph10) } {A/B}\r\n\0"));
				xprintf_P( PSTR("        (open|close) (A|B)\r\n\0"));
				xprintf_P( PSTR("        pulse (A|B) (secs) \r\n\0"));
				xprintf_P( PSTR("        power {on|off}\r\n\0"));
			}

			if ( spx_io_board == SPX_IO8CH ) {
				xprintf_P( PSTR("  output {0..7} {set | clear}\r\n\0"));
			}

			xprintf_P( PSTR("  gprs (pwr|sw|cts|dtr) {on|off}\r\n\0"));
			xprintf_P( PSTR("       cmd {atcmd}, redial\r\n\0"));

		}
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
			xprintf_P( PSTR("  i2cscan {busaddr}\r\n\0"));
			xprintf_P( PSTR("  temp,psensor\r\n\0"));
			if ( spx_io_board == SPX_IO8CH ) {
				xprintf_P( PSTR("  mcp {regAddr}\r\n\0"));
			}
			xprintf_P( PSTR("  memory {full}\r\n\0"));
			xprintf_P( PSTR("  din\r\n\0"));

			if ( spx_io_board == SPX_IO5CH ) {
				xprintf_P( PSTR("  ach {0..4}, battery\r\n\0"));
				xprintf_P( PSTR("  range\r\n\0"));
			} else if ( spx_io_board == SPX_IO8CH ) {
				xprintf_P( PSTR("  ach {0..7}\r\n\0"));
			}

			xprintf_P( PSTR("  gprs (rsp,rts,dcd,ri)\r\n\0"));
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
		xprintf_P( PSTR("  dlgid, apn, port, ip, script, simpasswd\r\n\0"));

		if ( spx_io_board == SPX_IO5CH ) {
			xprintf_P( PSTR("  pwrsave {on|off} {hhmm1}, {hhmm2}\r\n\0"));
			xprintf_P( PSTR("  timerpoll {val}, timerdial {val}, timepwrsensor {val}\r\n\0"));
		}

		if ( spx_io_board == SPX_IO8CH ) {
			xprintf_P( PSTR("  timerpoll {val}, sensortime {val}\r\n\0"));
		}

		xprintf_P( PSTR("  debug {none,counter,data, gprs, outputs, piloto }\r\n\0"));
		xprintf_P( PSTR("  analog {0..%d} aname imin imax mmin mmax\r\n\0"),( NRO_ANINPUTS - 1 ) );
		xprintf_P( PSTR("  offset {ch} {mag}, inaspan {ch} {mag}\r\n\0"));
		xprintf_P( PSTR("  autocal {ch,PSENSOR} {mag}\r\n\0"));
		xprintf_P( PSTR("  ical {ch} {imin | imax}\r\n\0"));
		xprintf_P( PSTR("  digital {0..%d} dname tpoll\r\n\0"), ( NRO_DINPUTS - 1 ) );
		xprintf_P( PSTR("  counter {0..%d} cname magPP pw(ms) period(ms) speed(LS/HS)\r\n\0"), ( NRO_COUNTERS - 1 ) );

		if ( spx_io_board == SPX_IO5CH ) {
			xprintf_P( PSTR("  rangemeter {on|off}\r\n\0"));
			xprintf_P( PSTR("  psensor name countMin countMax pmin pmax offset\r\n\0"));
		}

		xprintf_P( PSTR("  outmode { off | perf | plt | cons }\r\n\0"));
		xprintf_P( PSTR("  consigna {hhmm1} {hhmm2}\r\n\0"));
		xprintf_P( PSTR("  piloto { reg{CHICA|MEDIA|GRANDE}, pband {pband},steps {steps},slot {idx} {hhmm} {pout} }\r\n\0"));
		xprintf_P( PSTR("  default {SPY|OSE|UTE}\r\n\0"));
		xprintf_P( PSTR("  save\r\n\0"));
	}

	// HELP KILL
	else if (!strcmp_P( strupr(argv[1]), PSTR("KILL\0")) && ( tipo_usuario == USER_TECNICO) ) {
		xprintf_P( PSTR("-kill {counter, data, dinputs, doutputs, gprstx, gprsrx }\r\n\0"));
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
		vTaskSuspend( xHandle_tkCounter0 );
		vTaskSuspend( xHandle_tkCounter1 );
		ctl_watchdog_kick(WDG_COUNT0, 0x8000 );
		ctl_watchdog_kick(WDG_COUNT1, 0x8000 );
		pv_snprintfP_OK();
		return;
	}

	// KILL DATA
	if (!strcmp_P( strupr(argv[1]), PSTR("DATA\0"))) {
		vTaskSuspend( xHandle_tkData );
		ctl_watchdog_kick(WDG_DAT, 0x8000 );
		pv_snprintfP_OK();
		return;
	}

	// KILL DOUTPUTS
	if (!strcmp_P( strupr(argv[1]), PSTR("DOUTPUTS\0"))) {
		vTaskSuspend( xHandle_tkDoutputs );
		ctl_watchdog_kick(WDG_DOUT, 0x8000 );
		pv_snprintfP_OK();
		return;
	}

	// KILL GPRSTX
	if (!strcmp_P( strupr(argv[1]), PSTR("GPRSTX\0"))) {
		vTaskSuspend( xHandle_tkGprsTx );
		ctl_watchdog_kick(WDG_GPRSTX, 0x8000 );
		// Dejo la flag de modem prendido para poder leer comandos
		GPRS_stateVars.modem_prendido = true;
		pv_snprintfP_OK();
		return;
	}

	// KILL GPRSRX
	if (!strcmp_P( strupr(argv[1]), PSTR("GPRSRX\0"))) {
		vTaskSuspend( xHandle_tkGprsRx );
		ctl_watchdog_kick(WDG_GPRSRX, 0x8000 );
		pv_snprintfP_OK();
		return;
	}

	// KILL DINPUTS
	if (!strcmp_P( strupr(argv[1]), PSTR("DINPUTS\0"))) {
		vTaskSuspend( xHandle_tkDinputs);
		ctl_watchdog_kick(WDG_DINPUTS, 0x8000 );
		pv_snprintfP_OK();
		return;
	}

	pv_snprintfP_ERR();
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

uint8_t fuse0 = 0;
uint8_t fuse1 = 0;
uint8_t fuse2 = 0;
uint8_t fuse4 = 0;
uint8_t fuse5 = 0;

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
static void pv_cmd_print_stack_watermarks(void)
{

UBaseType_t uxHighWaterMark;

	// tkIdle
	uxHighWaterMark = uxTaskGetStackHighWaterMark( xHandle_idle );
	xprintf_P( PSTR("IDLE:%03d,%03d,[%03d]\r\n\0"),configMINIMAL_STACK_SIZE,uxHighWaterMark,(configMINIMAL_STACK_SIZE - uxHighWaterMark)) ;

	// tkControl
	uxHighWaterMark = uxTaskGetStackHighWaterMark( xHandle_tkCtl );
	xprintf_P( PSTR("CTL: %03d,%03d,[%03d]\r\n\0"),tkCtl_STACK_SIZE,uxHighWaterMark, (tkCtl_STACK_SIZE - uxHighWaterMark));

	// tkCmd
	uxHighWaterMark = uxTaskGetStackHighWaterMark( xHandle_tkCmd );
	xprintf_P( PSTR("CMD: %03d,%03d,[%03d]\r\n\0"),tkCmd_STACK_SIZE,uxHighWaterMark,(tkCmd_STACK_SIZE - uxHighWaterMark)) ;

	// tkCounters
	uxHighWaterMark = uxTaskGetStackHighWaterMark( xHandle_tkCounter0 );
	xprintf_P( PSTR("CNT0: %03d,%03d,[%03d]\r\n\0"),tkCounter_STACK_SIZE,uxHighWaterMark, (tkCounter_STACK_SIZE - uxHighWaterMark));
	uxHighWaterMark = uxTaskGetStackHighWaterMark( xHandle_tkCounter1 );
	xprintf_P( PSTR("CNT1: %03d,%03d,[%03d]\r\n\0"),tkCounter_STACK_SIZE,uxHighWaterMark, (tkCounter_STACK_SIZE - uxHighWaterMark));

	// tkData
	uxHighWaterMark = uxTaskGetStackHighWaterMark( xHandle_tkData );
	xprintf_P( PSTR("DAT: %03d,%03d,[%03d]\r\n\0"),tkData_STACK_SIZE,uxHighWaterMark, (tkData_STACK_SIZE - uxHighWaterMark));

	// tkDout
	uxHighWaterMark = uxTaskGetStackHighWaterMark( xHandle_tkDoutputs );
	xprintf_P( PSTR("DOUT: %03d,%03d,[%03d]\r\n\0"), tkDoutputs_STACK_SIZE,uxHighWaterMark, ( tkDoutputs_STACK_SIZE - uxHighWaterMark));

	// tkGprsTX
	uxHighWaterMark = uxTaskGetStackHighWaterMark( xHandle_tkGprsTx );
	xprintf_P( PSTR("TX: %03d,%03d,[%03d]\r\n\0"), tkGprs_tx_STACK_SIZE ,uxHighWaterMark, ( tkGprs_tx_STACK_SIZE - uxHighWaterMark));

	// tkGprsRX
	uxHighWaterMark = uxTaskGetStackHighWaterMark( xHandle_tkGprsRx );
	xprintf_P( PSTR("RX: %03d,%03d,[%03d]\r\n\0"), tkGprs_rx_STACK_SIZE ,uxHighWaterMark, ( tkGprs_rx_STACK_SIZE - uxHighWaterMark));

	// tkDigital
	uxHighWaterMark = uxTaskGetStackHighWaterMark( xHandle_tkDinputs );
	xprintf_P( PSTR("RX: %03d,%03d,[%03d]\r\n\0"), tkDinputs_STACK_SIZE ,uxHighWaterMark, ( tkDinputs_STACK_SIZE - uxHighWaterMark));


}
//------------------------------------------------------------------------------------
static void pv_cmd_read_battery(void)
{

float battery = 0.0;

	ainputs_prender_12V_sensors();
	ainputs_awake();
	vTaskDelay( ( TickType_t)( ( 1000 * systemVars.ainputs_conf.pwr_settle_time ) / portTICK_RATE_MS ) );

	battery = ainputs_read_battery();

	ainputs_sleep();
	ainputs_apagar_12Vsensors();

	xprintf_P( PSTR("Battery=%.02f\r\n\0"), battery );

}
//------------------------------------------------------------------------------------
static void pv_cmd_read_analog_channel(void)
{

float mag_val = 0.0;


	if ( atoi(argv[2]) <  NRO_ANINPUTS ) {

		ainputs_prender_12V_sensors();
		ainputs_awake();
		vTaskDelay( ( TickType_t)( ( 1000 * systemVars.ainputs_conf.pwr_settle_time ) / portTICK_RATE_MS ) );

		mag_val = ainputs_read_channel( atoi(argv[2]) );
		xprintf_P( PSTR("anCH[%02d]=%.02f\r\n\0"),atoi(argv[2]),mag_val );

		ainputs_sleep();
		ainputs_apagar_12Vsensors();

	} else {
		pv_snprintfP_ERR();
	}
}
//------------------------------------------------------------------------------------
static void pv_cmd_read_digital_channels(void)
{

	// Leo e imprimo todos los canales digitales a la vez.

uint8_t dinputsA[MAX_DINPUTS_CHANNELS] = { 0,0,0,0,0,0,0,0 };
uint8_t i = 0;

	// Leo
	for ( i = 0; i < NRO_DINPUTS; i++ ) {
		dinputsA[i] = dinputs_read_channel(i);
	}

	// Imprimo al reves (MSB LSB)
	xprintf_P( PSTR("Dinputs: "));
	for ( i = 0; i < NRO_DINPUTS; i++ ) {
		//xprintf_P( PSTR(" %d"), dinputsA[ NRO_DINPUTS - 1 - i] );
		xprintf_P( PSTR(" %d"), dinputsA[ i] );
	}
	xprintf_P( PSTR("\r\n\0"));

}
//------------------------------------------------------------------------------------
static void pv_cmd_read_memory(void)
{
	// Si hay datos en memoria los lee todos y los muestra en pantalla
	// Leemos la memoria e imprimo los datos.
	// El problema es que si hay muchos datos puede excederse el tiempo de watchdog y
	// resetearse el dlg.
	// Para esto, cada 32 registros pateo el watchdog.
	// El proceso es similar a tkGprs.transmitir_dataframe


FAT_t l_fat;
size_t bRead = 0;
uint16_t rcds = 0;
bool detail = false;

	memset( &l_fat, '\0', sizeof(FAT_t));

	if (!strcmp_P( strupr(argv[2]), PSTR("FULL\0"))) {
		detail = true;
	}

	FF_rewind();
	while(1) {

		bRead = FF_readRcd( &dr, sizeof(st_dataRecord_t));
		FAT_read(&l_fat);

		if ( bRead == 0) {
			xprintf_P(PSTR( "MEM Empty\r\n"));
			break;
		}

		if ( ( rcds++ % 32) == 0 ) {
			ctl_watchdog_kick(WDG_CMD, WDG_CMD_TIMEOUT);
		}

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
			memcpy( &df.counters, &dr.df.io8.counters, ( NRO_COUNTERS * sizeof(float)));
			memcpy( &df.rtc, &dr.rtc, sizeof(RtcTimeType_t) );
			break;
		default:
			return;
		}

		// imprimo
		if ( detail ) {
			xprintf_P( PSTR("memory: wrPtr=%d,rdPtr=%d,delPtr=%d,r4wr=%d,r4rd=%d,r4del=%d \r\n\0"), l_fat.wrPTR,l_fat.rdPTR, l_fat.delPTR,l_fat.rcds4wr,l_fat.rcds4rd,l_fat.rcds4del );
		}

		// Imprimo el registro
		xprintf_P( PSTR("CTL=%d LINE=%04d%02d%02d,%02d%02d%02d\0"), l_fat.rdPTR, df.rtc.year, df.rtc.month, df.rtc.day, df.rtc.hour, df.rtc.min, df.rtc.sec );

		ainputs_df_print( &df );
		dinputs_df_print( &df );
	    counters_df_print( &df );
		u_df_print_range( &df );
		u_df_print_psensor( &df );
		u_df_print_temp( &df );
		ainputs_df_print_battery( &df );

		xprintf_P(PSTR( "\r\n"));
	}

}
//------------------------------------------------------------------------------------
static void pv_cmd_rwGPRS(uint8_t cmd_mode )
{

uint8_t pin = 0;
//char *p;

	if ( cmd_mode == WR_CMD ) {

		// write gprs (pwr|sw|rts|dtr) {on|off}

		if (!strcmp_P( strupr(argv[2]), PSTR("PWR\0")) ) {
			if (!strcmp_P( strupr(argv[3]), PSTR("ON\0")) ) {
				IO_set_GPRS_PWR(); pv_snprintfP_OK(); return;
			}
			if (!strcmp_P( strupr(argv[3]), PSTR("OFF\0")) ) {
				IO_clr_GPRS_PWR(); pv_snprintfP_OK(); return;
			}
			pv_snprintfP_ERR();
			return;
		}

		if (!strcmp_P( strupr(argv[2]), PSTR("SW\0")) ) {
			if (!strcmp_P( strupr(argv[3]), PSTR("ON\0")) ) {
				IO_set_GPRS_SW();
				pv_snprintfP_OK(); return;
			}
			if (!strcmp_P( strupr(argv[3]), PSTR("OFF\0")) ) {
				IO_clr_GPRS_SW(); pv_snprintfP_OK(); return;
			}
			pv_snprintfP_ERR();
			return;
		}

		if (!strcmp_P( strupr(argv[2]), PSTR("CTS\0")) ) {
			if (!strcmp_P( strupr(argv[3]), PSTR("ON\0")) ) {
				IO_set_GPRS_CTS(); pv_snprintfP_OK(); return;
			}
			if (!strcmp_P( strupr(argv[3]), PSTR("OFF\0")) ) {
				IO_clr_GPRS_CTS(); pv_snprintfP_OK(); return;
			}
			pv_snprintfP_ERR();
			return;
		}

		// Por ahora cableo DTR a CTS.

		if (!strcmp_P( strupr(argv[2]), PSTR("DTR\0")) ) {
			if (!strcmp_P( strupr(argv[3]), PSTR("ON\0")) ) {
				IO_set_GPRS_CTS(); pv_snprintfP_OK(); return;
			}
			if (!strcmp_P( strupr(argv[3]), PSTR("OFF\0")) ) {
				IO_clr_GPRS_CTS(); pv_snprintfP_OK(); return;
			}
			pv_snprintfP_ERR();
			return;
		}

		// write gprs redial
		if (!strcmp_P( strupr(argv[2]), PSTR("REDIAL\0")) ) {
			u_gprs_redial();
			return;
		}
		// ATCMD
		// // write gprs cmd {atcmd}
		if (!strcmp_P(strupr(argv[2]), PSTR("CMD\0"))) {
			xprintf_P( PSTR("%s\r\0"),argv[3] );

			u_gprs_flush_RX_buffer();
			xCom_printf_P( fdGPRS,PSTR("%s\r\0"),argv[3] );

			xprintf_P( PSTR("sent->%s\r\n\0"),argv[3] );
			return;
		}

		return;
	}

	if ( cmd_mode == RD_CMD ) {
		// read gprs (rsp,cts,dcd,ri)

			// ATCMD
			// read gprs rsp
			if (!strcmp_P(strupr(argv[2]), PSTR("RSP\0"))) {
				u_gprs_print_RX_Buffer();
				//p = pub_gprs_rxbuffer_getPtr();
				//xprintf_P( PSTR("rx->%s\r\n\0"),p );
				return;
			}

			// DCD
			if (!strcmp_P( strupr(argv[2]), PSTR("DCD\0")) ) {
				pin = IO_read_DCD();
				xprintf_P( PSTR("DCD=%d\r\n\0"),pin);
				pv_snprintfP_OK();
				return;
			}

			// RI
			if (!strcmp_P( strupr(argv[2]), PSTR("RI\0")) ) {
				pin = IO_read_RI();
				xprintf_P( PSTR("RI=%d\r\n\0"),pin);
				pv_snprintfP_OK();
				return;
			}

			// RTS
			if (!strcmp_P( strupr(argv[2]), PSTR("RTS\0")) ) {
				pin = IO_read_RTS();
				xprintf_P( PSTR("RTS=%d\r\n\0"),pin);
				pv_snprintfP_OK();
				return;
			}


			pv_snprintfP_ERR();
			return;
	}

}
//------------------------------------------------------------------------------------
static void pv_cmd_rwMCP(uint8_t cmd_mode )
{

int xBytes = 0;
uint8_t data = 0;

	if ( spx_io_board != SPX_IO8CH ) {
		xprintf_P(PSTR("ERROR: IOboard NOT spx8CH !!\r\n\0"));
		return;
	}

	// read mcp {regAddr}
	if ( cmd_mode == RD_CMD ) {
		xBytes = MCP_read( (uint32_t)(atoi(argv[2])), (char *)&data, 1 );
		if ( xBytes == -1 )
			xprintf_P(PSTR("ERROR: I2C:MCP:pv_cmd_rwMCP\r\n\0"));

		if ( xBytes > 0 ) {
			xprintf_P( PSTR( "MCP ADDR=0x%x, VALUE=0x%x\r\n\0"),atoi(argv[2]), data);
		}
		( xBytes > 0 ) ? pv_snprintfP_OK() : 	pv_snprintfP_ERR();
		return;
	}

	// write mcp regAddr value
	if ( cmd_mode == WR_CMD ) {
		data = atoi(argv[3]);

		xBytes = MCP_write( (uint32_t)(atoi(argv[2])), (char *)&data , 1 );
		if ( xBytes == -1 )
			xprintf_P(PSTR("ERROR: I2C:MCP:pv_cmd_rwMCP\r\n\0"));

		// Si estoy escribiendo el registro OLATB que refleja las salidas
		// debo dejar actualizado el valor que puse
		if ( atoi( argv[2]) == MCP_OLATB ) {
			perforaciones_set_douts(data);
		}

		( xBytes > 0 ) ? pv_snprintfP_OK() : 	pv_snprintfP_ERR();
		return;
	}


}
//------------------------------------------------------------------------------------
static void cmdPeekFunction(void)
{
	// Comando utilizado para recuperar variables.
	// peek varName
	// Return: datos de configuracion de la variable.

uint8_t ch = 0;

	FRTOS_CMD_makeArgv();

	if (!strcmp_P( strupr(argv[1]), PSTR("VERSION\0")) ) {
		xprintf_P( PSTR("VERSION=%s,%s,%s,%s,%d,%d\r\n\0"), SPX_HW_MODELO, SPX_FTROS_VERSION, SPX_FW_REV, SPX_FW_DATE, SYSMAINCLK, configTICK_RATE_HZ);

	} else if (!strcmp_P( strupr(argv[1]), PSTR("IOBOARD\0")) ) {
		xprintf_P( PSTR("IOBOARD=%d\r\n\0"), spx_io_board );

	} else if (!strcmp_P( strupr(argv[1]), PSTR("UID\0")) ) {
		xprintf_P( PSTR("UID=%s\r\n\0"), NVMEE_readID() );

	} else if (!strcmp_P( strupr(argv[1]), PSTR("DLGID\0")) ) {
		xprintf_P( PSTR("DLGID=%s\r\n\0"), systemVars.gprs_conf.dlgId );

	} else if  (!strcmp_P( strupr(argv[1]), PSTR("APN\0")) ) {
		xprintf_P( PSTR("APN=%s\r\n\0"), systemVars.gprs_conf.apn );

	} else if  (!strcmp_P( strupr(argv[1]), PSTR("PORT\0")) ) {
		xprintf_P( PSTR("PORT=%s\r\n\0"), systemVars.gprs_conf.server_tcp_port );

	} else if  (!strcmp_P( strupr(argv[1]), PSTR("IP\0")) ) {
		xprintf_P( PSTR("IP=%s\r\n\0"), systemVars.gprs_conf.server_ip_address );

	} else if  (!strcmp_P( strupr(argv[1]), PSTR("SCRIPT\0")) ) {
		xprintf_P( PSTR("SCRIPT=%s\r\n\0"), systemVars.gprs_conf.serverScript );

	} else if  (!strcmp_P( strupr(argv[1]), PSTR("SIMPWD\0")) ) {
		xprintf_P( PSTR("SIMPWD=%s\r\n\0"), systemVars.gprs_conf.simpwd );

	} else if  (!strcmp_P( strupr(argv[1]), PSTR("TIMERPOLL\0")) ) {
		xprintf_P( PSTR("TIMERPOLL=%d\r\n\0"), systemVars.timerPoll );

	} else if  (!strcmp_P( strupr(argv[1]), PSTR("TIMERDIAL\0")) ) {
		xprintf_P( PSTR("TIMERDIAL=%d\r\n\0"), systemVars.gprs_conf.timerDial );

	} else if  (!strcmp_P( strupr(argv[1]), PSTR("timepwrsensor\0")) ) {
		xprintf_P( PSTR("TIMEPWRSENSOR=%d\r\n\0"), systemVars.ainputs_conf.pwr_settle_time );

	} else if  (!strcmp_P( strupr(argv[1]), PSTR("DEBUG\0")) ) {
		xprintf_P( PSTR("DEBUG=%d\r\n\0"), systemVars.debug );

	} else if  (!strcmp_P( strupr(argv[1]), PSTR("RANGEMETER\0")) ) {
		xprintf_P( PSTR("RANGEMETER=%d\r\n\0"), systemVars.rangeMeter_enabled );

	} else if  (!strcmp_P( strupr(argv[1]), PSTR("OUTMODE\0")) ) {
		xprintf_P( PSTR("OUTMODE=%d\r\n\0"), systemVars.doutputs_conf.modo );

	} else if  (!strcmp_P( strupr(argv[1]), PSTR("PWRSAVE\0")) ) {
		xprintf_P( PSTR("PWRSAVE=%d,%d,%d\r\n\0"), systemVars.gprs_conf.pwrSave.pwrs_enabled, systemVars.gprs_conf.pwrSave.hora_start, systemVars.gprs_conf.pwrSave.hora_fin  );

	} else if  (!strcmp_P( strupr(argv[1]), PSTR("CONSIGNA\0")) ) {
		xprintf_P( PSTR("CONSIGNA=%d,%d\r\n\0"), systemVars.doutputs_conf.consigna.hhmm_c_diurna,systemVars.doutputs_conf.consigna.hhmm_c_nocturna  );

	} else if  (!strcmp_P( strupr(argv[1]), PSTR("ANALOG\0")) && ( argv[2] != NULL )) {
		ch = atoi( argv[2]);
		xprintf_P( PSTR("ANALOG=%d,%s,%d,%d,%.02f,%.02f\r\n\0"), ch, systemVars.ainputs_conf.name[ch], systemVars.ainputs_conf.imin[ch], systemVars.ainputs_conf.imax[ch], systemVars.ainputs_conf.mmin[ch], systemVars.ainputs_conf.mmax[ch] );

	} else if  (!strcmp_P( strupr(argv[1]), PSTR("OFFSET\0")) && ( argv[2] != NULL )) {
		ch = atoi( argv[2]);
		xprintf_P( PSTR("OFFSET=%d,%.02f\r\n\0"), ch,systemVars.ainputs_conf.offset[ch] );

	} else if  (!strcmp_P( strupr(argv[1]), PSTR("INASPAN\0")) && ( argv[2] != NULL )) {
		ch = atoi( argv[2]);
		xprintf_P( PSTR("INASPAN=%d,%d\r\n\0"), ch,systemVars.ainputs_conf.inaspan[ch] );

	} else if  (!strcmp_P( strupr(argv[1]), PSTR("DIGITAL\0")) && ( argv[2] != NULL )) {
		ch = atoi( argv[2]);
		xprintf_P( PSTR("DIGITAL=%d,%s\r\n\0"), ch,systemVars.dinputs_conf.name[ch] );

	} else if  (!strcmp_P( strupr(argv[1]), PSTR("COUNTER\0")) && ( argv[2] != NULL )) {
		ch = atoi( argv[2]);
		xprintf_P( PSTR("COUNTER=%d,%s,%.03f,%d,%d,%s\r\n\0"), ch,systemVars.counters_conf.name[ch],systemVars.counters_conf.magpp[ch],systemVars.counters_conf.pwidth[ch],systemVars.counters_conf.period[ch],systemVars.counters_conf.speed[ch] );

	} else {
		xprintf_P( PSTR("ERROR\r\n\0"));
	}
	return;

}
//------------------------------------------------------------------------------------
static void cmdPokeFunction(void)
{
	// COmando para configurar variables.

	FRTOS_CMD_makeArgv();


	if (!strcmp_P( strupr(argv[1]), PSTR("DLGID\0")) ) {

		memset(systemVars.gprs_conf.dlgId,'\0', sizeof(systemVars.gprs_conf.dlgId) );
		memcpy(systemVars.gprs_conf.dlgId, argv[2], sizeof(systemVars.gprs_conf.dlgId));
		systemVars.gprs_conf.dlgId[DLGID_LENGTH - 1] = '\0';

	} else if  (!strcmp_P( strupr(argv[1]), PSTR("APN\0")) ) {
		memset(systemVars.gprs_conf.apn, '\0', sizeof(systemVars.gprs_conf.apn));
		memcpy(systemVars.gprs_conf.apn, argv[2], sizeof(systemVars.gprs_conf.apn));
		systemVars.gprs_conf.apn[APN_LENGTH - 1] = '\0';

	} else if  (!strcmp_P( strupr(argv[1]), PSTR("PORT\0")) ) {
		memset(systemVars.gprs_conf.server_tcp_port, '\0', sizeof(systemVars.gprs_conf.server_tcp_port));
		memcpy(systemVars.gprs_conf.server_tcp_port, argv[2], sizeof(systemVars.gprs_conf.server_tcp_port));
		systemVars.gprs_conf.server_tcp_port[PORT_LENGTH - 1] = '\0';

	} else if  (!strcmp_P( strupr(argv[1]), PSTR("IP\0")) ) {
		memset(systemVars.gprs_conf.server_ip_address, '\0', sizeof(systemVars.gprs_conf.server_ip_address));
		memcpy(systemVars.gprs_conf.server_ip_address, argv[2], sizeof(systemVars.gprs_conf.server_ip_address));
		systemVars.gprs_conf.server_ip_address[IP_LENGTH - 1] = '\0';

	} else if  (!strcmp_P( strupr(argv[1]), PSTR("SCRIPT\0")) ) {
		memset(systemVars.gprs_conf.serverScript, '\0', sizeof(systemVars.gprs_conf.serverScript));
		memcpy(systemVars.gprs_conf.serverScript, argv[2], sizeof(systemVars.gprs_conf.serverScript));
		systemVars.gprs_conf.serverScript[SCRIPT_LENGTH - 1] = '\0';

	} else if  (!strcmp_P( strupr(argv[1]), PSTR("SIMPWD\0")) ) {
		memset(systemVars.gprs_conf.simpwd, '\0', sizeof(systemVars.gprs_conf.simpwd));
		memcpy(systemVars.gprs_conf.simpwd, argv[2], sizeof(systemVars.gprs_conf.simpwd));
		systemVars.gprs_conf.simpwd[PASSWD_LENGTH - 1] = '\0';

	} else if  (!strcmp_P( strupr(argv[1]), PSTR("TIMERPOLL\0")) ) {
		u_config_timerpoll( argv[2] );

	} else if  (!strcmp_P( strupr(argv[1]), PSTR("TIMERDIAL\0")) ) {

		if ( spx_io_board == SPX_IO5CH ) {
			u_gprs_config_timerdial( argv[2] );
		}

	} else if  (!strcmp_P( strupr(argv[1]), PSTR("timepwrsensor\0")) ) {
		ainputs_config_timepwrsensor( argv[2] );

	} else if  (!strcmp_P( strupr(argv[1]), PSTR("DEBUG\0")) ) {
		systemVars.debug = atoi(argv[2]);

	} else if  (!strcmp_P( strupr(argv[1]), PSTR("RANGEMETER\0")) ) {
		range_config(argv[2]);

	} else if  (!strcmp_P( strupr(argv[1]), PSTR("OUTMODE\0")) ) {
		doutputs_config_mode( argv[2] );

	} else if  (!strcmp_P( strupr(argv[1]), PSTR("PWRSAVE\0")) ) {
		u_gprs_configPwrSave ( argv[2], argv[3], argv[4] );

	} else if  (!strcmp_P( strupr(argv[1]), PSTR("COUNTER\0")) ) {
		counters_config_channel( atoi(argv[2]), argv[3], argv[4], argv[5], argv[6], argv[7] );

	} else if  (!strcmp_P( strupr(argv[1]), PSTR("OFFSET\0")) ) {
		ainputs_config_offset( argv[2], argv[3] );

	} else if  (!strcmp_P( strupr(argv[1]), PSTR("INASPAN\0")) ) {
		ainputs_config_span( argv[2], argv[3] );

	} else if  (!strcmp_P( strupr(argv[1]), PSTR("DIGITAL\0")) ) {
		dinputs_config_channel( atoi(argv[2]), argv[3], argv[4] );

	} else if  (!strcmp_P( strupr(argv[1]), PSTR("ANALOG\0")) ) {
		ainputs_config_channel( atoi(argv[2]), argv[3], argv[4], argv[5], argv[6], argv[7] );
	}

}
//------------------------------------------------------------------------------------
static void pv_cmd_I2Cscan(void)
{

bool retS = false;
uint8_t i2c_address;

	i2c_address = atoi(argv[2]);
	retS = I2C_scan_device(i2c_address);
	if (retS) {
		xprintf_P( PSTR("I2C device found at 0x%02x\r\n\0"), i2c_address );
	} else {
		xprintf_P( PSTR("I2C device NOT found at 0x%02x\r\n\0"), i2c_address );
	}
	return;
}
//------------------------------------------------------------------------------------

