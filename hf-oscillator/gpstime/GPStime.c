/////////////////////////////////////////////////////////////////////////////
//  Majority of code forked from work by
//  Roman Piksaykin [piksaykin@gmail.com], R2BDY
//  https://www.qrz.com/db/r2bdy
//  PROJECT PAGE
//  https://github.com/RPiks/pico-WSPR-tx
///////////////////////////////////////////////////////////////////////////////
#include "GPStime.h"

static GPStimeContext *spGPStimeContext = NULL;
static GPStimeData *spGPStimeData = NULL;
static uint16_t byte_count;

/// @brief Initializes GPS time module Context.
/// @param uart_id UART id to which GPS receiver is connected, 0 OR 1.
/// @param uart_baud UART baudrate, 115200 max.
/// @param pps_gpio GPIO pin of PPS (second pulse) from GPS receiver.
/// @return the new GPS time Context.
GPStimeContext *GPStimeInit(int uart_id, int uart_baud, int pps_gpio)
{
    ASSERT_(0 == uart_id || 1 == uart_id);
    ASSERT_(uart_baud <= 115200);
    ASSERT_(pps_gpio < 29);
	printf(" GPS time init was called ");
    // Set up our UART with the required speed & assign pins.
    uart_init(uart_id ? uart1 : uart0, uart_baud);
    gpio_set_function(uart_id ? 8 : 0, GPIO_FUNC_UART);
    gpio_set_function(uart_id ? 9 : 1, GPIO_FUNC_UART);
    
    GPStimeContext *pgt = calloc(1, sizeof(GPStimeContext));
    ASSERT_(pgt);

    pgt->_uart_id = uart_id;
    pgt->_uart_baudrate = uart_baud;
    pgt->_pps_gpio = pps_gpio;

    spGPStimeContext = pgt;
    spGPStimeData = &pgt->_time_data;

    gpio_init(pps_gpio);
    gpio_set_dir(pps_gpio, GPIO_IN);
    gpio_set_irq_enabled_with_callback(pps_gpio, GPIO_IRQ_EDGE_RISE, true, &GPStimePPScallback);

    uart_set_hw_flow(uart_id ? uart1 : uart0, false, false);
    uart_set_format(uart_id ? uart1 : uart0, 8, 1, UART_PARITY_NONE);
    uart_set_fifo_enabled(uart_id ? uart1 : uart0, false);  //this turns off the internal FIFO and makes chas come in one at a time
    irq_set_exclusive_handler(uart_id ? UART1_IRQ : UART0_IRQ, GPStimeUartRxIsr);
    irq_set_enabled(uart_id ? UART1_IRQ : UART0_IRQ, true);
    uart_set_irq_enables(uart_id ? uart1 : uart0, true, false);

    return pgt;
}

/// @brief Deinits the GPS module and destroys allocated resources.
/// @param pp Ptr to Ptr of the Context.
void GPStimeDestroy(GPStimeContext **pp)
{
    ASSERT_(pp);
    ASSERT_(*pp);

    spGPStimeContext = NULL;    /* Detach global context Ptr. */
    spGPStimeData = NULL;

    uart_deinit((*pp)->_uart_id ? uart1 : uart0);
    free(*pp);
    *pp = NULL;
}

/// @brief The PPS interrupt service subroutine.
/// @param  gpio The GPIO pin of Pico which is connected to PPS output of GPS rec.
void RAM (GPStimePPScallback)(uint gpio, uint32_t events)
{   
    const uint64_t tm64 = GetUptime64();
    if(spGPStimeData)
    {
        spGPStimeData->_u64_sysclk_pps_last = tm64;   //set pps_last = tm64 = uptime 
        ++spGPStimeData->_ix_last;                    //increment ix_last
        spGPStimeData->_ix_last %= eSlidingLen;       //rollover ix_last to zero at 32 (SlidingLen)

        const int64_t dt_per_window = tm64 - spGPStimeData->_pu64_sliding_pps_tm[spGPStimeData->_ix_last]; //sets dt_per_window to elapsed time since 32 cycles ago
        spGPStimeData->_pu64_sliding_pps_tm[spGPStimeData->_ix_last] = tm64;
    
	//	printf("Error value: %llu\n",(ABS((dt_per_window - eCLKperTimeMark * eSlidingLen)-eMaxCLKdevPPM * eSlidingLen)));    
		if(ABS(dt_per_window - eCLKperTimeMark * eSlidingLen) < eMaxCLKdevPPM * eSlidingLen)   // only if dt_per_window within +/-250 ppm of 32 seconds
        {      
			if(spGPStimeData->_u64_pps_period_1M)   //only if pps_per_1m != 0
            {
                spGPStimeData->_u64_pps_period_1M += iSAR64((int64_t)eDtUpscale * dt_per_window   //pp_period incremented by error (of last 32sec period), but also add 2 and divide by4 (via bitshift)      pps_per_1m +=  1mill*dt_per_window - pps_per_1m+2 , divided by 4 (bit shift 2 to the right with iSAR64)
														    - spGPStimeData->_u64_pps_period_1M + 2, 2);          // - spGPStimeData->_u64_pps_period_1M + 2, 2);
                spGPStimeData->_i32_freq_shift_ppb = (spGPStimeData->_u64_pps_period_1M           //set the frequency compensation value here, pretty much from pps_period (lots zeroes and nums that cancel out in this calc)
                                                      - (int64_t)eDtUpscale * eCLKperTimeMark * eSlidingLen
                                                      + (eSlidingLen >> 1)) / eSlidingLen;
			}
            else
            {
                spGPStimeData->_u64_pps_period_1M = (int64_t)eDtUpscale * dt_per_window;  //if pps_per_1m was zero, initialize it to ~ 32 secs
            }
        }

	if (spGPStimeContext->forced_XMIT_on)   //show some data for debugging
		{
        const int64_t dt_1M = (dt_per_window + (eSlidingLen >> 1)) / eSlidingLen;
        const uint64_t tmp = (spGPStimeData->_u64_pps_period_1M + (eSlidingLen >> 1)) / eSlidingLen;
        printf("tempr: %d \n",spGPStimeContext->temp_in_Celsius);
	    GPStimeDump(spGPStimeData);
	}
    }
	if (spGPStimeContext->enable_debug_messages) printf("PPS went on at: %.3f secs\n",((uint32_t)(to_us_since_boot(get_absolute_time()) / 1000ULL)/1000.0f ));
}

/// @brief Calculates current unixtime using data available.
/// @param pg Ptr to the context.
/// @param u32_tmdst Ptr to destination unixtime val.
/// @return 0 if OK.
/// @return -1 There was NO historical GPS fixes.
/// @return -2 The fix was expired (24hrs or more time ago).
int GPStimeGetTime(const GPStimeContext *pg, uint32_t *u32_tmdst)
{
    assert_(pg);
    assert(u32_tmdst);

    /* If there has been no fix, it's no way to get any time data... */
    if(!pg->_time_data._u32_utime_nmea_last)
    {
        return -1;
    }

    const uint64_t tm64 = GetUptime64();
    const uint64_t dt = tm64 - pg->_time_data._u64_sysclk_nmea_last;
    const uint32_t dt_sec = PicoU64timeToSeconds(dt);

    /* If expired. */
    if(dt_sec > 86400)
    {
        return -2;
    }

    *u32_tmdst = pg->_time_data._u32_utime_nmea_last + dt_sec;

    return 0;
}

/// @brief UART FIFO ISR. Processes another N chars received from GPS receiver
void RAM (GPStimeUartRxIsr)()
{
    if(spGPStimeContext)
    {
		uart_inst_t *puart_id = spGPStimeContext->_uart_id ? uart1 : uart0;
        while (uart_is_readable(puart_id))
        {
            //gpio_put(PICO_DEFAULT_LED_PIN, 1);
            uint8_t chr = uart_getc(puart_id);
            spGPStimeContext->_pbytebuff[spGPStimeContext->_u8_ixw++] = chr;
            if ('\n' == chr)
			{
				spGPStimeContext->_pbytebuff[spGPStimeContext->_u8_ixw]=0;//null terminates
				spGPStimeContext->_is_sentence_ready =1;
				break;
			}            
        }
		
	   if(spGPStimeContext->_is_sentence_ready)
        {
			spGPStimeContext->_u8_ixw = 0;     // printf("\n dump RAW FIFO: %s\n\n",(char *)spGPStimeContext->_pbytebuff);           
            spGPStimeContext->_is_sentence_ready =0;
			spGPStimeContext->_i32_error_count -= GPStimeProcNMEAsentence(spGPStimeContext);
			extract_altitude(spGPStimeContext);
        }
    }
}

/// @brief Processes a NMEA sentence GPRMC.
/// @param pg Ptr to Context.
/// @return 0 OK.
/// @return -2 Error: bad lat format.
/// @return -3 Error: bad lon format.
/// @return -4 Error: no final '*' char ere checksum value.
/// @attention Checksum validation is not implemented so far. !FIXME!
int GPStimeProcNMEAsentence(GPStimeContext *pg)
{                                                             //"$GNRMC, for new modules!
    assert_(pg);
    uint8_t *prmc = (uint8_t *)strnstr((char *)pg->_pbytebuff, "$GPRMC,", sizeof(pg->_pbytebuff));
    uint8_t *nrmc = (uint8_t *)strnstr((char *)pg->_pbytebuff, "$GNRMC,", sizeof(pg->_pbytebuff));
    if(nrmc) prmc=nrmc;
	
	if(prmc)
    {
        ++pg->_time_data._u32_nmea_gprmc_count;   
		if (pg->enable_debug_messages)	printf("Found GxRMC len: %d  full buff: %s",sizeof(pg->_pbytebuff),(char *)pg->_pbytebuff);// printf("prmc found: %s\n",(char *)prmc);

        uint64_t tm_fix = GetUptime64();
        uint8_t u8ixcollector[16] = {0};   //collects locations of commas
        uint8_t chksum = 0;
        for(uint8_t u8ix = 0, i = 0; u8ix != strlen(prmc); ++u8ix)
        {
            uint8_t *p = prmc + u8ix;
            chksum ^= *p;
            if(',' == *p)
            {
                *p = 0;
                u8ixcollector[i++] = u8ix + 1;
                if('*' == *p || 12 == i)
                {
                    break;
                }
            }
        }
		
		pg->_time_data._u8_last_digit_minutes= *(prmc + u8ixcollector[0] + 3);
        pg->_time_data._u8_is_solution_active = 'A' == prmc[u8ixcollector[1]];  // printf("char is: %c\n",(char *)prmc[u8ixcollector[1]]);

        if(pg->_time_data._u8_is_solution_active)
        {											 

			char firstTwo[3]; // Array to hold the first two characters
			strncpy(firstTwo, (const char *)prmc + u8ixcollector[2], 2);
			firstTwo[2] = '\0'; // Null terminate the string
			int dd_lat= atoi(firstTwo);
			pg->_time_data._i64_lat_100k = (int64_t)(.5f + 1e5 * ( (100*dd_lat) + atof((const char *)prmc + u8ixcollector[2]+2)/0.6)          ); 
            if('N' == prmc[u8ixcollector[3]]) { }
            else if('S' == prmc[u8ixcollector[3]])
            {
                INVERSE(pg->_time_data._i64_lat_100k);
            }
            else
            {
                return -2;
            }
			
			char firstThree[4]; // Array to hold the first two characters
			strncpy(firstThree, (const char *)prmc + u8ixcollector[4], 3);
			firstThree[3] = '\0'; // Null terminate the string
			int dd_lon= atoi(firstThree);									   
            pg->_time_data._i64_lon_100k = (int64_t)(.5f + 1e5 * ( (100*dd_lon) + atof((const char *)prmc + u8ixcollector[4]+3)/0.6)  );
            if('E' == prmc[u8ixcollector[5]]) { }
            else if('W' == prmc[u8ixcollector[5]])
            {
                INVERSE(pg->_time_data._i64_lon_100k);
            }
            else
            {
                return -3;
            }

            if('*' != prmc[u8ixcollector[11] + 1])
            {
                return -4;
            }

            pg->_time_data._u32_utime_nmea_last = GPStime2UNIX(prmc + u8ixcollector[8], prmc + u8ixcollector[0]);
            pg->_time_data._u64_sysclk_nmea_last = tm_fix;
			
		}
    }

    return 0;
}
/////////////////////////////////////////////////////////////////////////////////////

void extract_altitude(GPStimeContext *pg)
{
    assert_(pg);
    uint8_t *GnGGA = (uint8_t *)strnstr((char *)pg->_pbytebuff, "$GNGGA,", sizeof(pg->_pbytebuff));
    uint8_t *GxGGA = (uint8_t *)strnstr((char *)pg->_pbytebuff, "$GPGGA,", sizeof(pg->_pbytebuff));
    if(GnGGA) GxGGA=GnGGA; 
	if(GxGGA)
    {
        if (pg->enable_debug_messages) printf("Found GxGGA len: %d  full buff: %s",sizeof(pg->_pbytebuff),(char *)pg->_pbytebuff);
       
        uint8_t u8ixcollector[16] = {0};   //collects locations of commas
        uint8_t chksum = 0;
        for(uint8_t u8ix = 0, i = 0; u8ix != strlen(GxGGA); ++u8ix)
        {
            uint8_t *p = GxGGA + u8ix;
            chksum ^= *p;
            if(',' == *p)
            {
                *p = 0;
                u8ixcollector[i++] = u8ix + 1;
                if('*' == *p || 12 == i)
                {
                    break;
                }
            }
        }
           
			//printf("altitude: %s\n",(char *)GxGGA+u8ixcollector[8]);
        float f;
		f = (float)atof((char *)GxGGA+u8ixcollector[8]);  //printf("floating version of altitude: %f\n",f); 
			pg->_power_altitude=f;
											 
    }
}

///////////////////////////////////////////////////////////////////////////////////////

/// @brief Converts GPS time and date strings to unix time.
/// @param pdate Date string, 6 chars in work.
/// @param ptime Time string, 6 chars in work.
/// @return Unix timestamp (epoch). 0 if bad imput format.
uint32_t GPStime2UNIX(const char *pdate, const char *ptime)
{
    assert_(pdate);
    assert_(ptime);

    if(strlen(pdate) == 6 && strlen(ptime) > 5)
    {
        struct tm ltm = {0};

        ltm.tm_year = 100 + DecimalStr2ToNumber(pdate + 4);
        ltm.tm_mon  = DecimalStr2ToNumber(pdate + 2) - 1;
        ltm.tm_mday = DecimalStr2ToNumber(pdate);

        ltm.tm_hour = DecimalStr2ToNumber(ptime);
        ltm.tm_min = DecimalStr2ToNumber(ptime + 2);
        ltm.tm_sec = DecimalStr2ToNumber(ptime + 4);

        return mktime(&ltm);
    }

    return 0;
}

/// @brief Dumps the GPS data struct to stdio.
/// @param pd Ptr to Context.
void GPStimeDump(const GPStimeData *pd)
{
    assert_(pd);

    printf("\nGPS solution is active:%u\n", pd->_u8_is_solution_active);
    printf("GPRMC count:%lu\n", pd->_u32_nmea_gprmc_count);
    printf("GPS Latitude:%lld Longtitude:%lld\n", pd->_i64_lat_100k, pd->_i64_lon_100k);
    printf("PPS sysclock last:%llu\n", pd->_u64_sysclk_pps_last);
    printf("PPS period *1e6:%llu\n", (pd->_u64_pps_period_1M + (eSlidingLen>>1)) / eSlidingLen);
    printf("FRQ correction ppb:%lld\n\n", pd->_i32_freq_shift_ppb);
}
