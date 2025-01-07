/*
  pico_fractional_pll.h
  Pseudo Fractional PLL for RP2040
  Copyright 2024 by Kazuhisa "Kazu." Terasaki
  https://github.com/kaduhi/pico-fractional-pll
*/

#ifndef _PICO_FRACTIONAL_PLL_H
#define _PICO_FRACTIONAL_PLL_H

#include "pico/stdlib.h"
#include "hardware/pll.h"

typedef struct pico_fractional_pll_instance_t {
  // do not change this block, referred from the assembly code
  PLL pll;
  uint32_t acc_increment;
  uint32_t fbdiv_low;

  uint gpio;
  uint gpclk;
  uint srcclk;
  enum gpio_drive_strength drive_strength;
  enum gpio_slew_rate slew_rate;
  uint32_t freq_low;
  uint32_t freq_high;
  uint32_t fbdiv_high;
  uint32_t div;
  uint32_t postdiv1;
  uint32_t postdiv2;
  uint32_t clkdiv;
  uint32_t freq_delta;
} pico_fractional_pll_instance_t;

int pico_fractional_pll_init(PLL pll, uint gpio, uint32_t freq_range_min, uint32_t freq_range_max, enum gpio_drive_strength drive_strength, enum gpio_slew_rate slew_rate);

int pico_fractional_pll_deinit(void);

void pico_fractional_pll_enable_output(bool enable);

void pico_fractional_pll_set_drive_strength(enum gpio_drive_strength drive_strength);

void pico_fractional_pll_set_slew_rate(enum gpio_slew_rate slew_rate);

void pico_fractional_pll_set_freq_u32(uint32_t freq);

// fixed point: 28bit integer + 4bit fraction
void pico_fractional_pll_set_freq_28p4(uint32_t freq_28p4);

void pico_fractional_pll_set_freq_d(double freq);

void pico_fractional_pll_set_freq_f(float freq);

#endif // _PICO_FRACTIONAL_PLL_H
