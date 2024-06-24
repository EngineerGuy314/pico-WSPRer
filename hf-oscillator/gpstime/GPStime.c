/////////////////////////////////////////////////////////////////////////////
//  Majority of code forked from work by
//  Roman Piksaykin [piksaykin@gmail.com], R2BDY
//  https://www.qrz.com/db/r2bdy
//  PROJECT PAGE
//  https://github.com/RPiks/pico-WSPR-tx
///////////////////////////////////////////////////////////////////////////////
#include "GPStime.h"
#include "timer_PIO.pio.h"

static GPStimeContext *spGPStimeContext = NULL;
static GPStimeData *spGPStimeData = NULL;
static uint16_t byte_count;
int    use_software_TCXO;
int64_t error_ppb_temp_vs_gps;
int64_t GPS_based_freq_shift_ppb;

static 	PIO timer_PIO;  /*state machine used for high resolution timing of duration between PPS pulses
						at 115Mhz clock speed each instruction in PIO takes 1/115M = 8.69565E-09 seconds
						the majority of the PIO loop executes 2 instructions each count cycle, which is 2 X 8.69565E-09 = 1.73913E-08 secs
						so the resolution of the timer is about 17.39130435 nanoSeconds per tick
						*/
static int sm;
static uint offset;
static int32_t PIO_counts_per_PPS;
static int32_t	elapsed_PIO_ticks_FILTERED= 57500000; //preload ideal value to reduce initial filtering lock time. 
/// @brief Initializes GPS time module Context.
/// @param uart_id UART id to which GPS receiver is connected, 0 OR 1.
/// @param uart_baud UART baudrate, 115200 max.
/// @param pps_gpio GPIO pin of PPS (second pulse) from GPS receiver.
/// @return the new GPS time Context.
GPStimeContext *GPStimeInit(int uart_id, int uart_baud, int pps_gpio)
{
  
	timer_PIO = pio1;    //instantiate pio1 for the timer, pio0 already used for WSPR generation
	sm = 0;			     //each of the two PIOs has 4 state machines (sm) available. use the 1st one 
    offset = pio_add_program( timer_PIO, &timer_PIO_program);
    timer_PIO_program_init(timer_PIO, sm, offset);

	ASSERT_(0 == uart_id || 1 == uart_id);
    ASSERT_(uart_baud <= 115200);
    ASSERT_(pps_gpio < 29);
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

/// @brief The PPS interrupt service subroutine. Once a second it recalculates the frequency compensation needed based on time seen between PPS from GPS
/// @param  gpio The GPIO pin of Pico which is connected to PPS output of GPS rec.

void RAM (GPStimePPScallback)(uint gpio, uint32_t events)
{   
        if (pio_sm_get_rx_fifo_level(timer_PIO, sm) >= 2) 	//make sure at least two values are waiting to be read from the PIO           
			PIO_counts_per_PPS = pio_sm_get(timer_PIO, sm)+ pio_sm_get(timer_PIO, sm);   //read and add 2 values from the PIO's output FIFO, representing tick count of ON and OFF pulse duration. each tick takes ~17.39nS

		if (PIO_counts_per_PPS>10000000) //make sure data is somewhat reasoable
		{
		elapsed_PIO_ticks_FILTERED=0.5*elapsed_PIO_ticks_FILTERED + 0.5*PIO_counts_per_PPS; 			    //a mild IIR lowpass filter to smooth the tick count from PIO
		spGPStimeData->_i32_freq_shift_ppb=(elapsed_PIO_ticks_FILTERED-(int64_t)57500000)*(int64_t)17391;  //57500000 is the ideal (exact) number of ticks in one second and 17391 comes from nano_secs_per_tick = 17.39130435 (scaled by a 1000 because of reasons. ask Roman. because we need parts per *billion*? for scaling reasons elsewhere?)
		}
		
		if ((spGPStimeContext->verbosity>=3)&&(spGPStimeContext->user_setup_menu_active==0)) printf(" elapsed PIO tick  %d and FILTERED %d   FRQ correction ppb:  %lli  \n",PIO_counts_per_PPS,elapsed_PIO_ticks_FILTERED,spGPStimeData->_i32_freq_shift_ppb);
		if ((spGPStimeContext->verbosity>=6)&&(spGPStimeContext->user_setup_menu_active==0)) GPStimeDump(spGPStimeData);
		if ((spGPStimeContext->verbosity>=6)&&(spGPStimeContext->user_setup_menu_active==0 )) printf("PPS went on at: %.3f secs\n",((uint32_t)(to_us_since_boot(get_absolute_time()) / 1000ULL)/1000.0f ));	
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
    if((spGPStimeContext))
    {
		uart_inst_t *puart_id = spGPStimeContext->_uart_id ? uart1 : uart0;
        while (uart_is_readable(puart_id))
        {
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
			spGPStimeContext->_u8_ixw = 0;     
														if ((spGPStimeContext->verbosity>=8)&&(spGPStimeContext->user_setup_menu_active==0 ))  printf("dump ALL RAW FIFO: %s",(char *)spGPStimeContext->_pbytebuff);           
            spGPStimeContext->_is_sentence_ready =0;
			spGPStimeContext->_i32_error_count -= GPStimeProcNMEAsentence(spGPStimeContext);
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
{                                               //"$GxGGA has time, locations, altitude and sat count! unlike $prmc it does NOT have date, but so what
    assert_(pg);
    uint8_t *prmc = (uint8_t *)strnstr((char *)pg->_pbytebuff, "$GPGGA,", sizeof(pg->_pbytebuff));
    uint8_t *nrmc = (uint8_t *)strnstr((char *)pg->_pbytebuff, "$GNGGA,", sizeof(pg->_pbytebuff));
    if(nrmc) prmc=nrmc;
	
	if(prmc)
    {
        ++pg->_time_data._u32_nmea_gprmc_count;   
		if ((spGPStimeContext->verbosity>=7)&&(spGPStimeContext->user_setup_menu_active==0 )) 	printf("Found GxGGA len: %d  full buff: %s",sizeof(pg->_pbytebuff),(char *)pg->_pbytebuff);// printf("prmc found: %s\n",(char *)prmc);

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
		pg->_time_data._u8_last_digit_hour= *(prmc + u8ixcollector[0] + 1);		
        pg->_time_data._u8_is_solution_active = (prmc[u8ixcollector[5]]>48);   //numeric 0 for no fix, 1 2 or 3 for various fix types //printf("char is: %c\n",prmc[u8ixcollector[5]]);
		pg->_time_data.sat_count = atoi((const char *)prmc + u8ixcollector[6]); 
		
		if ((spGPStimeContext->verbosity>=6)&&(spGPStimeContext->user_setup_menu_active==0 )) printf("sat count: %d\n",pg->_time_data.sat_count);

        if(pg->_time_data._u8_is_solution_active)
        {											 

			char firstTwo[3]; // Array to hold the first two characters
			strncpy(firstTwo, (const char *)prmc + u8ixcollector[1], 2);
			firstTwo[2] = '\0'; // Null terminate the string
			int dd_lat= atoi(firstTwo);
			pg->_time_data._i64_lat_100k = (int64_t)(.5f + 1e5 * ( (100*dd_lat) + atof((const char *)prmc + u8ixcollector[1]+2)/0.6)          ); 
            if('N' == prmc[u8ixcollector[2]]) { }
            else if('S' == prmc[u8ixcollector[3]])
            {
                INVERSE(pg->_time_data._i64_lat_100k);
            }
            else
            {
                return -2;
            }
			
			char firstThree[4]; // Array to hold the first two characters
			strncpy(firstThree, (const char *)prmc + u8ixcollector[3], 3);
			firstThree[3] = '\0'; // Null terminate the string
			int dd_lon= atoi(firstThree);									   
            pg->_time_data._i64_lon_100k = (int64_t)(.5f + 1e5 * ( (100*dd_lon) + atof((const char *)prmc + u8ixcollector[3]+3)/0.6)  );
            if('E' == prmc[u8ixcollector[4]]) { }
            else if('W' == prmc[u8ixcollector[4]])
            {
                INVERSE(pg->_time_data._i64_lon_100k);
            }
            else
            {
                return -3;
            }
			pg->_time_data._u64_sysclk_nmea_last = tm_fix;
			

			float f;
			f = (float)atof((char *)prmc+u8ixcollector[8]);  
			pg->_altitude=f;
    	
			//pg->_altitude=12500;     //FORCING A SPECIFIC ALTITUDE for debugging
		
		}
    }


    return 0;
}

///////////////////////////////////////////////////////////////////////////////////////

/// @brief Converts GPS time and date strings to unix time.
/// @param pdate Date string, 6 chars in work.
/// @param ptime Time string, 6 chars in work.
/// @return Unix timestamp (epoch). 0 if bad imput format.
uint32_t GPStime2UNIX(const char *pdate, const char *ptime)
{

    assert_(ptime);

    if(strlen(pdate) == 6 && strlen(ptime) > 5)
    {
        struct tm ltm = {0};

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
    printf("GxGGA count:%lu\n", pd->_u32_nmea_gprmc_count);
    printf("GPS Latitude:%lld Longtitude:%lld\n", pd->_i64_lat_100k, pd->_i64_lon_100k);
 //   printf("PPS sysclock last:%llu\n", pd->_u64_sysclk_pps_last);
 //   printf("PPS period *1e6:%llu\n", (pd->_u64_pps_period_1M + (eSlidingLen>>1)) / eSlidingLen);
    printf(" FRQ correction ppb:%lld  ", pd->_i32_freq_shift_ppb);
}
