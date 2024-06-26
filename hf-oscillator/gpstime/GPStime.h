/////////////////////////////////////////////////////////////////////////////
//  Majority of code forked from work by
//  Roman Piksaykin [piksaykin@gmail.com], R2BDY
//  https://www.qrz.com/db/r2bdy
//  PROJECT PAGE
//  https://github.com/RPiks/pico-WSPR-tx
///////////////////////////////////////////////////////////////////////////////
#ifndef GPSTIME_H_
#define GPSTIME_H_

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "../defines.h"
#include "../lib/assert.h"
#include "../lib/utility.h"
#include "../lib/thirdparty/strnstr.h"

#define ASSERT_(x) assert_(x)


typedef struct
{
    uint8_t _u8_is_solution_active;             /* A navigation solution is valid. */
	uint8_t sat_count;
    char _u8_last_digit_minutes;                /* First digit of the minutes. Really, this is the only thing needed to sequence messages. */
    char _u8_last_digit_hour;  
    int64_t _i64_lat_100k, _i64_lon_100k;       /* The lat, lon, degrees, multiplied by 1e5. */
    uint32_t _u32_nmea_gprmc_count;             /* The count of $GPRMC sentences received */
    uint8_t _ix_last;                           /* An index of last write to sliding window. */
    int64_t _i32_freq_shift_ppb;                /* Calcd frequency shift, parts per billion. */
	
} GPStimeData;

typedef struct
{
    int _uart_id;
    int _uart_baudrate;
    int _pps_gpio;

    GPStimeData _time_data;

    uint8_t _pbytebuff[256];  
    uint8_t _u8_ixw;
    uint8_t _is_sentence_ready;
    int32_t _i32_error_count;
    float _altitude;   //altitude in metesr
	uint8_t user_setup_menu_active;
	uint8_t forced_XMIT_on;
	int8_t temp_in_Celsius;
	int8_t verbosity;

} GPStimeContext;

GPStimeContext *GPStimeInit(int uart_id, int uart_baud, int pps_gpio, uint32_t clock_speed);
void GPStimeDestroy(GPStimeContext **pp);
int parse_GPS_data(GPStimeContext *pg);
void RAM (GPStimePPScallback)(uint gpio, uint32_t events);
void RAM (GPStimeUartRxIsr)();
void GPStimeDump(const GPStimeData *pd);

#endif
