// ===================================================================================
// Project:   USB-RTC - Demo
// Version:   v1.1
// Year:      2021
// Author:    Stefan Wagner
// Github:    https://github.com/wagiminator
// EasyEDA:   https://easyeda.com/wagiminator
// License:   http://creativecommons.org/licenses/by-sa/3.0/
// ===================================================================================
//
// Description:
// ------------
// This demo implements the basic functions of the ATtiny-based USB real-time
// clock. When the firmware is uploaded, the time is set to the compilation
// time. The time is then kept up to date by the 32.768 kHz crystal. 
// If connected to the PC, the current date and time is transferred serially
// via USB every second. The backup battery allows the clock to continue 
// running even without a USB connection.
//
// Before compiling and uploading the firmware, install the buffer battery
// and set the selector switch to UPDI. After uploading, set the switch to
// UART. Select 9600 BAUD in the serial monitor.
//
// References:
// -----------
// https://ww1.microchip.com/downloads/en/DeviceDoc/ATtiny417-814-816-817-DataSheet-DS40002288A.pdf
// https://ww1.microchip.com/downloads/en/Appnotes/TB3213-Getting-Started-with-RTC-DS90003213.pdf
// https://ww1.microchip.com/downloads/en/Appnotes/TB3216-Getting-Started-with-USART-DS90003216.pdf
// https://ww1.microchip.com/downloads/en/Appnotes/AN2543-Temperature-Logger-with-ATtiny817-and-SD-Card-v2-00002543C.pdf
// https://ww1.microchip.com/downloads/en/DeviceDoc/ADC-Power-Optimization-with-tinyAVR-0-1-series-and-megaAVR-0-series.pdf
// https://ww1.microchip.com/downloads/en/Appnotes/00002447A.pdf
// https://en.wikipedia.org/wiki/Determination_of_the_day_of_the_week#Sakamoto's_methods
//
// Wiring:
// -------
//                              +-\/-+
//                        Vcc  1|°   |14  GND
//         ------- (AIN4) PA4  2|    |13  PA3 (AIN3) -------- 
//         ------- (AIN5) PA5  3|    |12  PA2 (AIN2) -------- RXD
//         --- DAC (AIN6) PA6  4|    |11  PA1 (AIN1) -------- TXD
//         ------- (AIN7) PA7  5|    |10  PA0 (AIN0) -------- UPDI
// CRYSTAL -------------- PB3  6|    |9   PB0 (AIN11) SCL --- 
// CRYSTAL -------------- PB2  7|    |8   PB1 (AIN10) SDA --- 
//                              +----+
//
// Compilation Settings:
// ---------------------
// Core:          megaTinyCore (https://github.com/SpenceKonde/megaTinyCore)
// Board:         ATtiny1614/1604/814/804/414/404/214/204
// Chip:          ATtiny1614 or ATtiny814 or ATtiny414 or ATtiny214
// Clock:         5 MHz internal
//
// Leave the rest on default settings. Don't forget to "Burn bootloader"!
// Compile and upload the code.
//
// No Arduino core functions or libraries are used. To compile and upload without
// Arduino IDE download AVR 8-bit toolchain at:
// https://www.microchip.com/mplab/avr-support/avr-and-arm-toolchains-c-compilers
// and extract to tools/avr-gcc. Use the makefile to compile and upload.
//
// Fuse Settings: 0:0x00 1:0x00 2:0x02 4:0x00 5:0xC5 6:0x04 7:0x00 8:0x00


// ===================================================================================
// Libraries and Definitions
// ===================================================================================

// Libraries
#include <avr/io.h>           // for GPIO
#include <avr/sleep.h>        // for sleep functions
#include <avr/interrupt.h>    // for interrupts

// ===================================================================================
// Time and Date Functions (refer to AN2543)
// ===================================================================================

// Variables
typedef struct {
  uint8_t  second;
  uint8_t  minute;
  uint8_t  hour;
  uint8_t  day;
  uint8_t  month;
  uint16_t year;
} time;

time t;

// Convert string into integer (2 digits)
uint8_t str2dec(const char *p) {
  return( (*p == ' ') ? (*(++p) - '0') : ((*p++ - '0') * 10 + (*p - '0')) );
}

// Get compile date and time to use as initial time (refer to AN2543)
void TIME_init(void) {
  char *ptr = __DATE__;                       // format "Feb 12 1996"

  // Month
  if(*ptr == 'J') {
    ptr++;
    if(*ptr == 'a') {
      t.month = 1;
      ptr += 3;
    } else if(*ptr == 'u') {
      ptr++;
      if(*ptr == 'n') {
        t.month = 6;
      } else if(*ptr == 'l') {
        t.month = 7;
      }
      ptr += 2;
    }
  } else if(*ptr == 'F') {
    t.month = 2;
    ptr += 4;
  } else if(*ptr == 'M') {
    ptr += 2;
    if(*ptr == 'r') {
      t.month = 3;
    } else if(*ptr == 'y') {
      t.month = 5;
    }
    ptr += 2;
  } else if(*ptr == 'A') {
    ptr++;
    if(*ptr == 'p') {
      t.month = 5;
    } else if(*ptr == 'u') {
      t.month = 8;
    }
    ptr += 3;
  } else if(*ptr == 'S') {
    t.month = 9;
    ptr += 4;
  } else if(*ptr == 'O') {
    t.month = 10;
    ptr += 4;
  } else if(*ptr == 'N') {
    t.month = 11;
    ptr += 4;
  } else if(*ptr == 'D') {
    t.month = 12;
    ptr += 4;
  }

  // Day
  t.day = str2dec(ptr);
  ptr += 3;
  
  // Year
  t.year = (uint16_t) str2dec(ptr) * 100;
  ptr += 2;
  t.year += str2dec(ptr);

  ptr      = __TIME__;                        // format "23:59:01"
  t.hour   = str2dec(ptr); ptr += 3;          // hour
  t.minute = str2dec(ptr); ptr += 3;          // minute
  t.second = str2dec(ptr);                    // second
}

// Returns TRUE if year is NOT a leap year (refer to AN2543)
uint8_t TIME_notLeapYear(void) {
  if(!(t.year % 100)) return(t.year % 400);
  return(t.year % 4);
}

// Returns day of the week (Sakamoto's method, refer to wikipedia)
uint8_t TIME_dayOfTheWeek(void) {
  static uint8_t td[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  uint16_t y = t.year;
  if(t.month < 3) y -= 1;
  return((y + y/4 - y/100 + y/400 + td[t.month - 1] + t.day) % 7);
}

// Increase time and date by one second (refer to AN2543)
void TIME_update(void) {
  if(++t.second >= 60) {
    t.second -= 60;
    if(++t.minute == 60) {
      t.minute = 0;
      if(++t.hour == 24) {
        t.hour = 0;
        if(++t.day == 32) {
          t.month++;
          t.day = 1;
        } else if(t.day == 31) {
          if((t.month == 4) || (t.month == 6) || (t.month == 9) || (t.month == 11)) {
            t.month++;
            t.day = 1;
          }
        } else if(t.day == 30) {
          if(t.month == 2) {
            t.month++;
            t.day = 1;
          }
        } else if(t.day == 29) {
          if((t.month == 2) && (TIME_notLeapYear())) {
            t.month++;
            t.day = 1;
          }
        }
        if(t.month == 13) {
          t.year++;
          t.month = 1;
        }
      }
    }
  }
}

// ===================================================================================
// RTC Functions (refer to TB3213)
// ===================================================================================

// Setup external 32.768 kHz crystal and periodic interrupt timer (PIT)
void RTC_init(void) {
  _PROTECTED_WRITE(CLKCTRL_XOSC32KCTRLA, CLKCTRL_ENABLE_bm); // enable crystal
  RTC.CLKSEL      = RTC_CLKSEL_TOSC32K_gc;    // select external 32K crystal
  RTC.PITINTCTRL  = RTC_PI_bm;                // enable periodic interrupt
  RTC.PITCTRLA    = RTC_PERIOD_CYC32768_gc    // set period to 1 second
                  | RTC_PITEN_bm;             // enable PIT
}

// Interrupt service routine for PIT (wake up from sleep every second)
ISR(RTC_PIT_vect) {
  RTC.PITINTFLAGS = RTC_PI_bm;                // clear interrupt flag
}

// ===================================================================================
// ADC Supply Voltage Measurement (refer to AN2447)
// ===================================================================================

// ADC init for VCC measurements
void ADC_init(void) {
  VREF.CTRLA  = VREF_ADC0REFSEL_1V1_gc;       // select 1.1V reference
  ADC0.MUXPOS = ADC_MUXPOS_INTREF_gc;         // set internal reference as ADC input
  ADC0.CTRLC  = ADC_REFSEL_VDDREF_gc          // set VCC as reference
              | ADC_PRESC_DIV4_gc;            // set prescaler for 1.25 MHz ADC clock
  ADC0.CTRLD  = ADC_INITDLY_DLY64_gc;         // delay to settle internal reference
  ADC0.CTRLA  = ADC_RESSEL_bm                 // select 8-bit resolution
              | ADC_ENABLE_bm;                // enable ADC, single shot
}

// ===================================================================================
// UART Implementation (refer to TB3216)
// ===================================================================================

#define UART_BAUD       9600
#define UART_BAUD_RATE  4.0 * F_CPU / UART_BAUD + 0.5
#define UART_flushed()  (USART0.STATUS & USART_TXCIF_bm)

// UART init
void UART_init(void) {
  PORTMUX.CTRLB = PORTMUX_USART0_bm;          // select alternative pins for USART0
  USART0.BAUD   = UART_BAUD_RATE;             // set BAUD
  USART0.CTRLB |= USART_TXEN_bm;              // enable TX
}

// UART transmit data byte
void UART_write(uint8_t data) {
  while(!(USART0.STATUS & USART_DREIF_bm));   // wait until ready for next data
  USART0.STATUS |= USART_TXCIF_bm;            // clear USART TX complete flag
  USART0.TXDATAL = data;                      // send data byte
}

// UART print string
void UART_print(const char *str) {
  while(*str) UART_write(*str++);             // write characters of string
}

// UART print 2-digit integer value via UART
void UART_printVal(uint8_t value) {
  UART_write((value / 10) + '0');
  UART_write((value % 10) + '0');
}

// ===================================================================================
// Send Date and Time
// ===================================================================================

// Day of the week strings
const char *TIME_days[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };

// Send time stamp via UART
void sendTime(void) {
  VPORTA.DIR |= PIN1_bm;                      // set TX pin as output  

  // Send time stamp
  UART_printVal(t.hour);   UART_write(':');
  UART_printVal(t.minute); UART_write(':');
  UART_printVal(t.second); UART_print(" - ");
  UART_print(TIME_days[TIME_dayOfTheWeek()]); UART_print(", ");
  UART_printVal(t.day);   UART_write('.');
  UART_printVal(t.month);  UART_write('.');
  UART_printVal(t.year / 100);
  UART_printVal(t.year % 100); UART_write('\n');
  
  while(!UART_flushed());                     // wait for UART TX to complete
  VPORTA.DIR &= ~PIN1_bm;                     // set TX pin as input to save power
}

// ===================================================================================
// Main Function
// ===================================================================================

int main(void) {
  // Setup
  _PROTECTED_WRITE(CLKCTRL.MCLKCTRLB, 3);     // set clock frequency to 5 MHz
  TIME_init();                                // set time to compile time
  UART_init();                                // init UART
  ADC_init();                                 // init ADC
  RTC_init();                                 // init RTC
  sei();                                      // enable global interrupts

  // Disable unused pins to save power
  for(uint8_t pin=7; pin; pin--) (&PORTA.PIN0CTRL)[pin] = PORT_ISC_INPUT_DISABLE_gc;
  PORTB.PIN0CTRL = PORT_ISC_INPUT_DISABLE_gc;
  PORTB.PIN1CTRL = PORT_ISC_INPUT_DISABLE_gc;

  // Prepare sleep mode
  SLPCTRL.CTRLA |= SLPCTRL_SMODE_PDOWN_gc;    // set sleep mode to power down
  SLPCTRL.CTRLA |= SLPCTRL_SEN_bm;            // enable sleep mode
  
  // Loop
  while(1) {                                  // loop until forever                         
    sleep_cpu();                              // sleep until next second
    ADC0.COMMAND = ADC_STCONV_bm;             // start sampling supply voltage
    TIME_update();                            // meanwhile increase time and date by one second
    while(ADC0.COMMAND & ADC_STCONV_bm);      // wait for ADC sampling to complete
    if(ADC0.RESL < 65) sendTime();            // send time if VCC > 4.3V (65=256*1.1V/4.3V)
  }
}
