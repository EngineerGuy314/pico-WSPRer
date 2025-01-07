/*
  pico_fractional_pll.c
  Pseudo Fractional PLL for RP2040
  Copyright 2024 by Kazuhisa "Kazu." Terasaki
  https://github.com/kaduhi/pico-fractional-pll
*/

#include "pico_fractional_pll.h"

#include "pico/multicore.h"

#include "hardware/clocks.h"
#include "hardware/structs/pll.h"
#include "hardware/structs/clocks.h"
#include "hardware/structs/scb.h"
#include "hardware/exception.h"
#include "hardware/structs/systick.h"

#define ALARM_NUM   2
#define ALARM_IRQ   TIMER_IRQ_2

extern uint32_t ram_vector_table[48];
uint32_t __attribute__((aligned(256))) ram_vector_table2[48];

static const uint32_t xo_freq = 12000000;

// PLL VCO frequency range has to be 750MHz - 1600MHz
static const uint32_t vco_freq_max = 1600000000;  // 133
static const uint32_t vco_freq_min = 750000000;   // 63

static const uint32_t foutpostdiv_freq_max = 150000000; //133000000;

static const uint32_t fbdiv_max = vco_freq_max / xo_freq;
static const uint32_t fbdiv_min = (vco_freq_min + (xo_freq - 1)) / xo_freq;

static pico_fractional_pll_instance_t pico_fractional_pll_instance;

enum core1_state_t {
  core1_state_not_started = 0,
  core1_state_initialized,
  core1_state_running,
  core1_state_stopping = 8, // do not change this, used in assembly code
  core1_state_stopped,
};
// this needs to be uint32_t, because accessing in assembly code
static volatile uint32_t s_core1_state = core1_state_not_started;


extern void __unhandled_user_irq(void);

static bool is_div_possible(uint32_t vco_freq, uint32_t div, uint32_t *o_postdiv1, uint32_t *o_postdiv2, uint32_t *o_clkdiv);
static bool calculate_pll_divider(pico_fractional_pll_instance_t *instance, uint32_t freq_range_min, uint32_t freq_range_max);
static void launch_core1(void);


// public functions

int pico_fractional_pll_init(PLL pll, uint gpio, uint32_t freq_range_min, uint32_t freq_range_max, enum gpio_drive_strength drive_strength, enum gpio_slew_rate slew_rate)
{
  if (s_core1_state != core1_state_not_started) {
    return 0;
  }

  pico_fractional_pll_instance.pll = pll;
  pico_fractional_pll_instance.gpio = gpio;
  pico_fractional_pll_instance.drive_strength = drive_strength;
  pico_fractional_pll_instance.slew_rate = slew_rate;

  if (calculate_pll_divider(&pico_fractional_pll_instance, freq_range_min, freq_range_max) == false) {
    return -1;
  }

  uint gpclk;
  uint src;
  if (gpio == 21) {
    gpclk = clk_gpout0;
    src = (pll == pll_sys) ? CLOCKS_CLK_GPOUT0_CTRL_AUXSRC_VALUE_CLKSRC_PLL_SYS : CLOCKS_CLK_GPOUT0_CTRL_AUXSRC_VALUE_CLKSRC_PLL_USB;
  }
  else if (gpio == 23) {
    gpclk = clk_gpout1;
    src = (pll == pll_sys) ? CLOCKS_CLK_GPOUT1_CTRL_AUXSRC_VALUE_CLKSRC_PLL_SYS : CLOCKS_CLK_GPOUT1_CTRL_AUXSRC_VALUE_CLKSRC_PLL_USB;
  }
  else if (gpio == 24) {
    gpclk = clk_gpout2;
    src = (pll == pll_sys) ? CLOCKS_CLK_GPOUT2_CTRL_AUXSRC_VALUE_CLKSRC_PLL_SYS : CLOCKS_CLK_GPOUT2_CTRL_AUXSRC_VALUE_CLKSRC_PLL_USB;
  }
  else if (gpio == 25) {
    gpclk = clk_gpout3;
    src = (pll == pll_sys) ? CLOCKS_CLK_GPOUT3_CTRL_AUXSRC_VALUE_CLKSRC_PLL_SYS : CLOCKS_CLK_GPOUT3_CTRL_AUXSRC_VALUE_CLKSRC_PLL_USB;
  }
  else {
    return -2;
  }
  pico_fractional_pll_instance.gpclk = gpclk;
  pico_fractional_pll_instance.srcclk = src;

  // initially the output is disabled
  pico_fractional_pll_enable_output(false);

  pico_fractional_pll_instance.acc_increment = 0x80000000;
  pll_init(pll, 1, (xo_freq * pico_fractional_pll_instance.fbdiv_low), pico_fractional_pll_instance.postdiv1, pico_fractional_pll_instance.postdiv2);

  launch_core1();

  s_core1_state = core1_state_running;

  return 0;
}

int pico_fractional_pll_deinit(void)
{
  if (s_core1_state == core1_state_not_started) {
    return 0;
  }

  pico_fractional_pll_enable_output(false);

  pll_deinit(pico_fractional_pll_instance.pll);

  s_core1_state = core1_state_stopping;
  while (s_core1_state == core1_state_stopping) { }
  multicore_reset_core1();

  s_core1_state = core1_state_not_started;

  return 0;
}

void pico_fractional_pll_enable_output(bool enable)
{
  if (s_core1_state == core1_state_not_started) {
    return;
  }

  if (enable) {
    clock_gpio_init_int_frac(pico_fractional_pll_instance.gpio, pico_fractional_pll_instance.srcclk, pico_fractional_pll_instance.clkdiv, 0);
    if (pico_fractional_pll_instance.clkdiv & 1) {
      // enable duty cycle correction for odd divisor
      clocks_hw->clk[pico_fractional_pll_instance.gpclk].ctrl |= CLOCKS_CLK_GPOUT0_CTRL_DC50_BITS;
    }
    else {
      clocks_hw->clk[pico_fractional_pll_instance.gpclk].ctrl &= ~CLOCKS_CLK_GPOUT0_CTRL_DC50_BITS;
    }
    gpio_set_drive_strength(pico_fractional_pll_instance.gpio, pico_fractional_pll_instance.drive_strength);
    gpio_set_slew_rate(pico_fractional_pll_instance.gpio, pico_fractional_pll_instance.slew_rate);
  }
  else {
    gpio_set_function(pico_fractional_pll_instance.gpio, GPIO_FUNC_NULL);
    gpio_set_dir(pico_fractional_pll_instance.gpio, GPIO_IN);
    gpio_set_pulls(pico_fractional_pll_instance.gpio, false, false);
  }
}

void pico_fractional_pll_set_drive_strength(enum gpio_drive_strength drive_strength)
{
  pico_fractional_pll_instance.drive_strength = drive_strength;
  if (s_core1_state == core1_state_not_started) {
    return;
  }
  gpio_set_drive_strength(pico_fractional_pll_instance.gpio, pico_fractional_pll_instance.drive_strength);
}

void pico_fractional_pll_set_slew_rate(enum gpio_slew_rate slew_rate)
{
  pico_fractional_pll_instance.slew_rate = slew_rate;
  if (s_core1_state == core1_state_not_started) {
    return;
  }
  gpio_set_slew_rate(pico_fractional_pll_instance.gpio, pico_fractional_pll_instance.slew_rate);
}

void pico_fractional_pll_set_freq_u32(uint32_t freq)
{
  if (freq < pico_fractional_pll_instance.freq_low) {
    pico_fractional_pll_instance.acc_increment = 0;
  }
  else if (freq >= pico_fractional_pll_instance.freq_high) {
    pico_fractional_pll_instance.acc_increment = 0xffffffff;
  }
  else {
    pico_fractional_pll_instance.acc_increment = (uint32_t)((uint64_t)(freq - pico_fractional_pll_instance.freq_low) * 0x100000000 / pico_fractional_pll_instance.freq_delta);
  }
}

void pico_fractional_pll_set_freq_28p4(uint32_t freq_28p4)
{
  uint32_t freq = freq_28p4 >> 4;
  if (freq < pico_fractional_pll_instance.freq_low) {
    pico_fractional_pll_instance.acc_increment = 0;
  }
  else if (freq >= pico_fractional_pll_instance.freq_high) {
    pico_fractional_pll_instance.acc_increment = 0xffffffff;
  }
  else {
    pico_fractional_pll_instance.acc_increment = (uint32_t)(((uint64_t)(freq_28p4 - (pico_fractional_pll_instance.freq_low << 4)) << (32 - 4)) / pico_fractional_pll_instance.freq_delta);
  }
}

void pico_fractional_pll_set_freq_d(double freq)
{
  if (freq < pico_fractional_pll_instance.freq_low) {
    pico_fractional_pll_instance.acc_increment = 0;
  }
  else if (freq >= pico_fractional_pll_instance.freq_high) {
    pico_fractional_pll_instance.acc_increment = 0xffffffff;
  }
  else {
    pico_fractional_pll_instance.acc_increment = (uint32_t)((freq - (double)pico_fractional_pll_instance.freq_low) * (double)0x100000000 / (double)pico_fractional_pll_instance.freq_delta);
  }
}

void pico_fractional_pll_set_freq_f(float freq)
{
  if (freq < pico_fractional_pll_instance.freq_low) {
    pico_fractional_pll_instance.acc_increment = 0;
  }
  else if (freq >= pico_fractional_pll_instance.freq_high) {
    pico_fractional_pll_instance.acc_increment = 0xffffffff;
  }
  else {
    pico_fractional_pll_instance.acc_increment = (uint32_t)((freq - (float)pico_fractional_pll_instance.freq_low) * (float)0x100000000 / (float)pico_fractional_pll_instance.freq_delta);
  }
}


// static functions

static const uint32_t possible_postdiv_values[][2] = {
    1, 1, // 1
    2, 1, // 2
    3, 1, // 3
    4, 1, // 4
    5, 1, // 5
    6, 1, // 6
    7, 1, // 7
    4, 2, // 8
    3, 3, // 9
    5, 2, // 10
    6, 2, // 12
    7, 2, // 14
    5, 3, // 15
    4, 4, // 16
    6, 3, // 18
    5, 4, // 20
    7, 3, // 21
    6, 4, // 24
    5, 5, // 25
    7, 4, // 28
    6, 5, // 30
    7, 5, // 35
    6, 6, // 36
    7, 6, // 42
    7, 7, // 49
};

static bool is_div_possible(uint32_t vco_freq, uint32_t div, uint32_t *o_postdiv1, uint32_t *o_postdiv2, uint32_t *o_clkdiv) {
    for (int i = (sizeof(possible_postdiv_values) / 2 / sizeof(possible_postdiv_values[0][0])) - 1; i >= 0; i--) {
        uint32_t postdiv1 = possible_postdiv_values[i][0];
        uint32_t postdiv2 = possible_postdiv_values[i][1];
        uint32_t postdiv = postdiv1 * postdiv2;
        uint32_t clkdiv = div / postdiv;
        if ((div % postdiv) == 0 && (vco_freq / postdiv) <= foutpostdiv_freq_max) {
            *o_postdiv1 = postdiv1;
            *o_postdiv2 = postdiv2;
            *o_clkdiv = clkdiv;
            return true;
        }
    }
    return false;
}

static bool calculate_pll_divider(pico_fractional_pll_instance_t *instance, uint32_t freq_range_min, uint32_t freq_range_max)
{
  for (int pass = 0; pass < 2; pass++) {
    for (uint32_t fbdiv_high = fbdiv_max; fbdiv_high > fbdiv_min; fbdiv_high--) {
      uint32_t fbdiv_low = fbdiv_high - 1;

      uint32_t vco_freq_low = xo_freq * fbdiv_low;
      uint32_t vco_freq_high = xo_freq * fbdiv_high;

      uint32_t div_low = (vco_freq_low + (freq_range_min - 1)) / freq_range_min;
      uint32_t div_high = vco_freq_high / freq_range_max;

      uint32_t div_freq_low = vco_freq_low / div_low;
      uint32_t div_freq_high = vco_freq_high / div_high;

      if (div_low != div_high) {
        // only supports single div value for entire range
        continue;
      }

      uint32_t div = div_high;
      uint32_t postdiv1, postdiv2, clkdiv;
      if (is_div_possible(vco_freq_high, div, &postdiv1, &postdiv2, &clkdiv) == false) {
        // couldn't find any valid combination of postdiv1 and postdiv2
        continue;
      }

      div_freq_low = vco_freq_low / div;
      div_freq_high = vco_freq_high / div;

      if ((vco_freq_low % div) != 0 || (vco_freq_high % div) != 0) {
        // div_freq_low or div_freq_high is not integer number
        // in pass 0, continue find other possible combinations
        // in pass 1, just accept them
        if (pass == 0) {
          continue;
        }
      }

      // found the right combination
      instance->freq_low = div_freq_low;    //TODO: handle the not integer cases
      instance->freq_high = div_freq_high;  //TODO: handle the not integer cases
      instance->fbdiv_low = fbdiv_low;
      instance->fbdiv_high = fbdiv_high;
      instance->div = div;
      instance->postdiv1 = postdiv1;
      instance->postdiv2 = postdiv2;
      instance->clkdiv = clkdiv;
      instance->freq_delta = div_freq_high - div_freq_low;
      instance->acc_increment = 0;
      return true;
    }
  }

  return false;
}

void core1_systick_callback(void);

static inline void __not_in_flash_func(fractional_pll_core_logic)(void)
{
  // r0: temp
  // r1: #0
  // r2: pico_fractional_pll_instance
  // r3: s_core1_state
  // r4: acc
  // r5: acc_increment
  // r6: fbdiv_low
  // r7: pll
  __asm(" \n\
    push {r0,r1,r2,r3,r4,r5,r6,r7}                  \n\
    mov r1, #0                                      \n\
    ldr r2, =(pico_fractional_pll_instance)         \n\
    ldr r3, =(s_core1_state)                        \n\
    mov r4, #0                                      \n\
    ldr r5, [r2, #4]  // acc_increment              \n\
    ldr r6, [r2, #8]  // fbdiv_low                  \n\
    ldr r7, [r2, #0]  // pll                        \n\
    b loop_1                                        \n\
                                                    \n\
    core1_systick_callback:                         \n\
                                                    \n\
    // thank you for reading my source code.        \n\
    // seems like you are really trying to          \n\
    // understand how my code works...              \n\
    // and now you are going to be surprised.       \n\
                                                    \n\
    // below 4 lines are the real 'CORE LOGIC' of   \n\
    // my Pico Fractional PLL implemenation:        \n\
                                                    \n\
    add r4, r5                                      \n\
    adc r1, r6                                      \n\
    str r1, [r7, #8]  // pll->fbdiv_int             \n\
    bx  lr                                          \n\
                                                    \n\
    // yes, that's it!! very very simple.           \n\
    // maybe you still don't get why this 4 lines   \n\
    // of code does the job. that's totally fine.   \n\
                                                    \n\
    // but if you are able to see all my stories    \n\
    // behind above 4 lines of code, hey, you're    \n\
    // now my friend. let's talk!                   \n\
    // yes, i already know what your background is, \n\
    // since you're able to understand my code.     \n\
    //                   Kazuhisa 'Kazu.' Terasaki  \n\
                                                    \n\
    loop_1:                                         \n\
    wfi                                             \n\
    ldr r5, [r2, #4]  // acc_increment              \n\
    ldr r0, [r3]                                    \n\
    cmp r0, #8  // core1_state_stopping             \n\
    bne loop_1                                      \n\
                                                    \n\
    // disable systick                              \n\
    ldr r2, =(0xe000e010)                           \n\
    str r1, [r2, #0]  // systick_hw->csr            \n\
    pop {r0,r1,r2,r3,r4,r5,r6,r7}                   \n\
  ");
}

static void __not_in_flash_func(timer_alarm_callback)(void)
{
  timer_hw->intr = (1u << ALARM_NUM);
}

static void __not_in_flash_func(core1_main)(void)
{
  // disable all interrupts
  for (int i = 0; i < 48; i++) {
    if (i >= VTABLE_FIRST_IRQ) {
      irq_set_enabled(i - VTABLE_FIRST_IRQ, false);
    }
  }

  // set the code1 dedicated vector table
  scb_hw->vtor = (uintptr_t)ram_vector_table2;

  // since the SYS_PLL is running on 12MHz XO clock and Core1 is on 48MHz sys_clk,
  // the timing of writing to the pll->fbdiv_int may not in sync with the XO clock.
  // this is bad, so we use the timer alarm (on XO clock) to sync.

  // setup an alarm
  hw_clear_bits(&timer_hw->armed, 1u << ALARM_NUM);
  hw_clear_bits(&timer_hw->intr, 1u << ALARM_NUM);
  hw_set_bits(&timer_hw->inte, 1u << ALARM_NUM);
  irq_set_exclusive_handler(ALARM_IRQ, timer_alarm_callback);
  irq_set_enabled(ALARM_IRQ, true);
  timer_hw->alarm[ALARM_NUM] = timer_hw->timerawl + 2;

  // DO NOT CHANGE ANYTHING BELOW!!
  // if change, the number of "nop"s needs to be re-adjusted for precise timing
  __asm(" \n\
    wfi   \n\
    nop   \n\
    nop   \n\
    nop   \n\
  ");

  hw_clear_bits(&timer_hw->armed, 1u << ALARM_NUM);
  hw_clear_bits(&timer_hw->intr, 1u << ALARM_NUM);
  timer_hw->inte &= ~(1u << ALARM_NUM);
  irq_set_enabled(ALARM_IRQ, false);

  // setup exception handler then start core1 systick counter
  systick_hw->csr &= ~M0PLUS_SYST_CSR_ENABLE_BITS;
  exception_set_exclusive_handler(SYSTICK_EXCEPTION, (exception_handler_t)((uint32_t)core1_systick_callback + 1));
  systick_hw->csr |= M0PLUS_SYST_CSR_CLKSOURCE_BITS | M0PLUS_SYST_CSR_TICKINT_BITS;
  systick_hw->cvr = 0;  // document says: A write of any value clears the field to 0
  systick_hw->rvr = (48 - 1); // 48 = 1.0usec
  systick_hw->csr |= M0PLUS_SYST_CSR_ENABLE_BITS;
  // DO NOT CHANGE ANYTHING ABOVE!!

  s_core1_state = core1_state_initialized;

  fractional_pll_core_logic();

  systick_hw->csr &= ~M0PLUS_SYST_CSR_ENABLE_BITS;
  s_core1_state = core1_state_stopped;

  // wait for core1 reset
  for (;;) {
    __asm("wfi");
  }
}

static void launch_core1(void)
{
  static bool init_done = false;
  if (!init_done) {
    init_done = true;
    // copy the vector table
    __builtin_memcpy(ram_vector_table2, ram_vector_table, sizeof(ram_vector_table2));
    // clear the table
    for (int i = 0; i < 48; i++) {
      if (i >= VTABLE_FIRST_IRQ) {
        ram_vector_table2[i] = (uint32_t)__unhandled_user_irq;
      }
    }
  }

  s_core1_state = core1_state_not_started;

  multicore_launch_core1(core1_main);

  while (s_core1_state == core1_state_not_started) { }
}
