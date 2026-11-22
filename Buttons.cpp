// avr-libc library includes
#include <avr/interrupt.h>
#include <Arduino.h> 

#define InternalVref     1100   // 1,1V

void InitButtons()
 {
// Init digital inputs
  MCUCR = 0x00;       // Pull-up enabled

  DDRB &= 0xF0;  // PB0-PB3 - input
  PORTB |= 0x0F; // PB0-PB3 Pull-up enable

  DDRD &= 0x03;  // PD2-PD7 - input
  PORTD |= 0xFC; // PD2-PD7 Pull-up enable

  DDRC &= 0xF8;  // PC0 (button), PC1 (A1 battery) - input
  PORTC |= 0x01; // PC0 Pull-up enable
  PORTC &= 0xFD; // PC1 (A1) Pull-up disable
  DIDR0 = 0x02;  // Disable digital input PC1 (A1)

// INIT ADC
  ADCSRA = 0x97; // ADC Enable, F/128
  ADMUX = 0x41;  // Vref = Vcc, Input = PC1 (A1)

// INIT Timer2
  TCNT2 = 0x00;
  TCCR2A = 0x00;
  TCCR2B = 0x06; // 0000.0110 FrqTMR2=CLK/256=16000000/256=62500Hz T_TMR2ovf=4ms
  TIMSK2 = 0x01; // enable interrupt overflow TMR2
 }

uint8_t ReadVcc()
// U_A1 = (ADC_A1 * InternalVref) / ADC_1,1V
  {
    uint8_t tmp;
    uint16_t V11;
    uint32_t tmp32;

// Analog Read Vin = PC1 (A1)
    ADMUX = 0x41;               // Asegura lectura en A1
    delay(1);
    ADCSRA|=0x40;               // Start the AD conversion
    while ((ADCSRA & 0x10)==0); // Wait for the AD conversion to complete
    ADCSRA|=0x10;
    tmp = ADCL;
    V11 = ADCH;
    ADMUX = 0x4E;               // Vref = Vcc, Input = Internal 1,1V
    V11 <<= 8;
    V11 += tmp;                       // 10 bit Vin = PC1 (A1)
    tmp32 = (long)InternalVref * V11; // 1100 * VinA1
    delay(1);
// Analog Read Vin = 1,1V
    ADCSRA|=0x40;               // Start the AD conversion
    while ((ADCSRA & 0x10)==0); // Wait for the AD conversion to complete
    ADCSRA|=0x10;
    tmp = ADCL;
    V11 = ADCH;
    V11 <<= 8;
    V11 += tmp;                       // 10 bit Vin = Internal 1,1V
    tmp32 /= V11;

    // Calibrado para tu bateria: 3.1V (1550mV en A1) = 0% | 4.0V (2000mV en A1) = 100%
    if (tmp32 < 1550) tmp32 = 1550;
    else if (tmp32 > 2000) tmp32 = 2000;

    tmp32 -= 1550;                    // Rango de 0 a 450 mV
    tmp = (tmp32 * 100) / 450;        // Convierte exactamente a 0...100%

    ADMUX = 0x41;                     // Vref = Vcc, Input = A1
    return(tmp);                      // Vcc = 0...100 (%)
  }

 ISR(TIMER2_OVF_vect)
  {
    extern uint16_t StateBut;
    extern uint16_t IntTimePush;

    static uint8_t cnt0, cnt1;
    uint16_t delta, sample, toggle;

    sample = ~(PINC) & 0x01;   // read button PC0 4 ms
    sample <<= 8;
    sample |= ~(PIND) & 0xF0;  // add buttons PD4-PD7      
    sample |= ~(PINB) & 0x0F;  // add buttons PB0-PB3
    
// Debouncing buttons with vertical counter 4ms
         delta = sample ^ StateBut;
         cnt1 = (cnt1 ^ cnt0) & delta;
         cnt0 = ~cnt0 & delta;
         toggle = delta & ~(cnt0 | cnt1);
         StateBut ^= toggle;                // debouncing buttons

         if (StateBut && (toggle == 0))     // Push button Time counter
            {
              IntTimePush++;
              if (IntTimePush > 0xFFF0) IntTimePush = 0xFFF0;
            }
         else IntTimePush = 0;
  }
