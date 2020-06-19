/**
 * Marlin 3D Printer Firmware
 * Copyright (c) 2020 MarlinFirmware [https://github.com/MarlinFirmware/Marlin]
 *
 * Based on Sprinter and grbl.
 * Copyright (c) 2011 Camiel Gubbels / Erik van der Zalm
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */
#pragma once

/**
 * Busy wait delay cycles routines:
 *
 *  DELAY_CYCLES(count): Delay execution in cycles
 *  DELAY_NS(count): Delay execution in nanoseconds
 *  DELAY_US(count): Delay execution in microseconds
 */

#include "../../core/macros.h"

#define  DWT_CR      *(volatile uint32_t *)0xE0001000
#define  DWT_CYCCNT  *(volatile uint32_t *)0xE0001004
#define  DEM_CR      *(volatile uint32_t *)0xE000EDFC
#define  DWT_LAR     *(volatile uint32_t *)0xE0001FB0
#define  DEM_CR_TRCENA                   (1 << 24)
#define  DWT_CR_CYCCNTENA                (1 <<  0)

#if defined(__arm__) || defined(__thumb__)

  //#if __CORTEX_M == 7
  #if 1  

    // Cortex-M3 through M7 can use the cycle counter of the DWT unit
    // http://www.anthonyvh.com/2017/05/18/cortex_m-cycle_counter/
    
    FORCE_INLINE static void enableCycleCounter() {
        DEM_CR         |=  DEM_CR_TRCENA;
      #if __CORTEX_M == 7
        DWT_LAR = 0xC5ACCE55; // Unlock DWT on the M7
      #endif
        DWT_CYCCNT      = 0;
        DWT_CR         |= DWT_CR_CYCCNTENA;
    }

    FORCE_INLINE volatile uint32_t getCycleCount() { return DWT_CYCCNT; }

    FORCE_INLINE static void DELAY_CYCLES(const uint32_t x) {
      DWT_CYCCNT      = 0;
      while(getCycleCount() < x);        //等到计数到所需延时值的cpu时钟数值
    }

  #else

    // https://blueprints.launchpad.net/gcc-arm-embedded/+spec/delay-cycles

    #define nop() __asm__ __volatile__("nop;\n\t":::)

    FORCE_INLINE static void __delay_4cycles(uint32_t cy) { // +1 cycle
      #if ARCH_PIPELINE_RELOAD_CYCLES < 2
        #define EXTRA_NOP_CYCLES A("nop")
      #else
        #define EXTRA_NOP_CYCLES ""
      #endif

      __asm__ __volatile__(
        A(".syntax unified") // is to prevent CM0,CM1 non-unified syntax
        L("1")
        A("subs %[cnt],#1")
        EXTRA_NOP_CYCLES
        A("bne 1b")
        : [cnt]"+r"(cy)   // output: +r means input+output
        :                 // input:
        : "cc"            // clobbers:
      );
    }

    // Delay in cycles
    FORCE_INLINE static void DELAY_CYCLES(uint32_t x) {

      if (__builtin_constant_p(x)) {
        #define MAXNOPS 4

        if (x <= (MAXNOPS)) {
          switch (x) { case 4: nop(); case 3: nop(); case 2: nop(); case 1: nop(); }
        }
        else { // because of +1 cycle inside delay_4cycles
          const uint32_t rem = (x - 1) % (MAXNOPS);
          switch (rem) { case 3: nop(); case 2: nop(); case 1: nop(); }
          if ((x = (x - 1) / (MAXNOPS)))
            __delay_4cycles(x); // if need more then 4 nop loop is more optimal
        }
        #undef MAXNOPS
      }
      else if ((x >>= 2))
        __delay_4cycles(x);
    }
    #undef nop

  #endif

#elif defined(__AVR__)

  #define nop() __asm__ __volatile__("nop;\n\t":::)

  FORCE_INLINE static void __delay_4cycles(uint8_t cy) {
    __asm__ __volatile__(
      L("1")
      A("dec %[cnt]")
      A("nop")
      A("brne 1b")
      : [cnt] "+r"(cy)  // output: +r means input+output
      :                 // input:
      : "cc"            // clobbers:
    );
  }

  // Delay in cycles
  FORCE_INLINE static void DELAY_CYCLES(uint16_t x) {

    if (__builtin_constant_p(x)) {
      #define MAXNOPS 4

      if (x <= (MAXNOPS)) {
        switch (x) { case 4: nop(); case 3: nop(); case 2: nop(); case 1: nop(); }
      }
      else {
        const uint32_t rem = (x) % (MAXNOPS);
        switch (rem) { case 3: nop(); case 2: nop(); case 1: nop(); }
        if ((x = (x) / (MAXNOPS)))
          __delay_4cycles(x); // if need more then 4 nop loop is more optimal
      }

      #undef MAXNOPS
    }
    else if ((x >>= 2))
      __delay_4cycles(x);
  }
  #undef nop

#elif defined(__PLAT_LINUX__) || defined(ESP32)

  // specified inside platform

#else

  #error "Unsupported MCU architecture"

#endif

// Delay in nanoseconds
#define DELAY_NS(x) DELAY_CYCLES( (x) * (F_CPU / 1000000UL) / 1000UL )

// Delay in microseconds
#define DELAY_US(x) DELAY_CYCLES( (x) * (F_CPU / 1000000UL) )
