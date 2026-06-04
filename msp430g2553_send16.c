#include <msp430.h>
#include <stdint.h>

#define DATA_PIN   BIT7   // P1.7
#define CLOCK_PIN  BIT7   // P2.7

#define BIT_TICKS  104

void send16bit_bitbang(uint16_t val)
{
  // LSB first (anpassbar)
  int i;
  for(i=0;i<16;i++)
  {
    // set DATA
    if(val & 1) 
      P1OUT |= DATA_PIN; 
    else
      P1OUT &= ~DATA_PIN;
    //end else
    // small setup time
    __delay_cycles(2);
    // toggle CLOCK high
    P2OUT |= CLOCK_PIN;
    
    //__delay_cycles(BIT_TICKS - 4); // adjust für overhead
    __delay_cycles(2);
    
    // toggle CLOCK low
    P2OUT &= ~CLOCK_PIN;
    __delay_cycles(2);
    val >>= 1;
  }
  // line idle low
  P1OUT &= ~DATA_PIN;
}


void send16bit_manchester(uint16_t val)
{
    int i;
    // Empfehlung: Interrupts aus, Ports als Ausgang konfiguriert
    for(i = 0; i < 16; ++i)
    {
        // LSB first
        int bit = val & 1;

        // --- erste Halbperiode ---
        // Konvention: für Bit==1 zuerst LOW, dann HIGH (Mitte: LOW->HIGH)
        if(bit)
            P1OUT &= ~DATA_PIN; // LOW
        else
            P1OUT |= DATA_PIN;  // HIGH

        __delay_cycles(2); // Setup

        // optionaler Clock-Pulse (sichtbar zur Debug/Sync)
        //P2OUT |= CLOCK_PIN;
        __delay_cycles(BIT_TICKS - 6);

        // --- zweite Halbperiode (invertiert) ---
        if(bit)
            P1OUT |= DATA_PIN;  // HIGH
        else
            P1OUT &= ~DATA_PIN; // LOW

        __delay_cycles(2);
        //P2OUT &= ~CLOCK_PIN;
        __delay_cycles(BIT_TICKS - 2);

        val >>= 1;
    }

    // Idle low
    P1OUT &= ~DATA_PIN;
}


volatile unsigned int iCounter = 0;
volatile uint16_t adc_value = 0;
volatile int send_flag = 0;

int main(void)
{
  WDTCTL = WDTPW + WDTHOLD;                 // Stop watchdog timer
  P1DIR |= 0x01;                            // Set P1.0 to output direction

  // SMCLK läuft standardmäßig vom DCO (~1MHz). Wenn nötig, kalibrieren.
  // ADC10 konfigurieren: Kanal A0 (P1.0)
  ADC10CTL1 = INCH_0 + ADC10SSEL_0; // A0, ADC10OSC (oder SMCLK)
  ADC10CTL0 = SREF_0 + ADC10SHT_2 + ADC10ON + ADC10IE; // Vcc ref, sample time, int
  ADC10AE0 |= BIT1; // P1.1? analog enable

  // Timer0_A für 1s Interrupt: SMCLK / 1 -> CCR0 = 1_000_000-1
  // Wenn SMCLK ≈1MHz:
  TA0CCTL0 = CCIE;
  TA0CCR0 = 1000000 - 1;
  TA0CTL = TASSEL_2 | MC_1 | TACLR; // SMCLK, up mode
  __enable_interrupt();

 // P1.7 output (DATA)
  P1DIR |= DATA_PIN;
  P1OUT &= ~DATA_PIN;
  // P2.7 output (CLOCK)
  P2SEL = 0;
  P2DIR |= CLOCK_PIN;
  P2OUT &= ~CLOCK_PIN;
  
  for (;;)
  {
    volatile unsigned int i;

    while (send_flag==0)
    {
      ; //wait...
    };
    send_flag = 0;
    P1OUT ^= 0x01;                          // Toggle P1.0 using exclusive-OR

    send16bit_bitbang(iCounter++);
    send16bit_bitbang(adc_value);

    //send16bit_manchester(0xFFFF);
    //send16bit_manchester(iCounter);
    //send16bit_manchester(0);

    i = 50000;                              // Delay
    do (i--);
    while (i != 0);
  }
}


#pragma vector=TIMER0_A0_VECTOR
__interrupt void Timer0_A0_ISR(void){
  // jede Sekunde: starte ADC-Wandlung
  ADC10CTL0 |= ENC + ADC10SC;
}

#pragma vector=ADC10_VECTOR
__interrupt void ADC10_ISR(void){
  adc_value = ADC10MEM; // 10-bit in low bits
  send_flag = 1;
}
