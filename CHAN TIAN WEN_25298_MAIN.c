//
//	CHAN TIAN WEN 25298 
//	LAB TEST
//

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "NUC1xx.h"
#include "SYS.h"
#include "GPIO.h"
#include "LCD.h"
#include "2DGraphics.h"

#define  PERIODIC 1   // counting and interrupt when reach TCMPR number, then counting from 0 again
#define X_MAX 127
#define RED_BLINK_RATE	46875
#define GREEN_BLINK_RATE	11719

volatile uint32_t ledState0 = 0;		//state of RED LED
volatile uint32_t ledState1 = 0;		//state of GREEN LED
int prev_VR2_state=0;							//keeps track of previous bar length. We give it a random initial number

//	---------TASK 1----------- 
//	Timer 0 for RED LED - blink rate of 1 seconds
//	Timer 1 for GREEN LED - blink rate of 0.25 seconds
  
void TMR0_IRQHandler()
{	
	ledState0 = ~ ledState0;  // changing ON/OFF state
	if(ledState0) DrvGPIO_ClrBit(E_GPA,14);		//GPA14 refers to RED LED
	else         	DrvGPIO_SetBit(E_GPA,14);
	
	TIMER0->TISR.TIF = 1;		// clear Interrupt flag
}

void TMR1_IRQHandler()
{
	ledState1 = ~ ledState1;  // changing ON/OFF state
	if(ledState1) DrvGPIO_ClrBit(E_GPA,13);		//GPA13 is GREEN LED
	else         	DrvGPIO_SetBit(E_GPA,13);
	TIMER1->TISR.TIF = 1;		// clear Interrupt flag
}

void InitTIMER0(void)
{
	/* Step 1. Enable and Select Timer clock source */          
	SYSCLK->CLKSEL1.TMR0_S = 0;	//Select 12Mhz for Timer0 clock source 
	SYSCLK->APBCLK.TMR0_EN = 1;	//Enable Timer0 clock source

	/* Step 2. Select Operation mode */	
	TIMER0->TCSR.MODE = PERIODIC; //Select operation mode

	/* Step 3. Select Time out period = (Period of timer clock input) * (8-bit Prescale + 1) * (24-bit TCMP)*/
	TIMER0->TCSR.PRESCALE = 255;	// Set Prescale [0~255]
	TIMER0->TCMPR = RED_BLINK_RATE;		    // Set TCMPR [0~16777215]
	//Timeout period = (1 / 12MHz) * ( 255 + 1 ) * RED_BLINK_RATE = sec

	/* Step 4. Enable interrupt */
	TIMER0->TCSR.IE = 1;
	TIMER0->TISR.TIF = 1;		//Write 1 to clear for safty		
	NVIC_EnableIRQ(TMR0_IRQn);	//Enable Timer0 Interrupt

	/* Step 5. Enable Timer module */
	TIMER0->TCSR.CRST = 1;	//Reset up counter
	TIMER0->TCSR.CEN = 1;		//Enable Timer0

//	TIMER0->TCSR.TDR_EN=1;		// Enable TDR function
}

void InitTIMER1(void)
{
	/* Step 1. Enable and Select Timer clock source */          
	SYSCLK->CLKSEL1.TMR1_S = 0;	//Select 12Mhz for Timer1 clock source 
	SYSCLK->APBCLK.TMR1_EN = 1;	//Enable Timer1 clock source

	/* Step 2. Select Operation mode */	
	TIMER1->TCSR.MODE = PERIODIC; //Select operation mode

	/* Step 3. Select Time out period = (Period of timer clock input) * (8-bit Prescale + 1) * (24-bit TCMP)*/
	TIMER1->TCSR.PRESCALE = 255;	// Set Prescale [0~255]
	TIMER1->TCMPR = GREEN_BLINK_RATE;		    // Set TCMPR [0~16777215]
	//Timeout period = (1 / 12MHz) * ( 255 + 1 ) * GREEN_BLINK_RATE = sec

	/* Step 4. Enable interrupt */
	TIMER1->TCSR.IE = 1;
	TIMER1->TISR.TIF = 1;		//Write 1 to clear for safty		
	NVIC_EnableIRQ(TMR1_IRQn);	//Enable Timer1 Interrupt

	/* Step 5. Enable Timer module */
	TIMER1->TCSR.CRST = 1;	//Reset up counter
	TIMER1->TCSR.CEN = 1;		//Enable Timer1
}


//	-----------TASK 2-------------
// 	Input from VR1 variable resistor change the number of LEDs to be displayed
//	Max value 4 LEDs, mid value 2 LEDs, min value 0 LEDs

void InitADC(void)
{
	/* Step 1. GPIO initial */ 
	GPIOA->OFFD|=0x00800000; 	//Disable digital input path
	SYS->GPAMFP.ADC7_SS21_AD6=1; 		//Set ADC function 
				
	/* Step 2. Enable and Select ADC clock source, and then enable ADC module */          
	SYSCLK->CLKSEL1.ADC_S = 2;	//Select 22Mhz for ADC
	SYSCLK->CLKDIV.ADC_N = 1;	//ADC clock source = 22Mhz/2 =11Mhz;
	SYSCLK->APBCLK.ADC_EN = 1;	//Enable clock source
	ADC->ADCR.ADEN = 1;			//Enable ADC module

	/* Step 3. Select Operation mode */
	ADC->ADCR.DIFFEN = 0;     	//single end input
	ADC->ADCR.ADMD   = 3;     	//continuous mode
		
	/* Step 4. Select ADC channel */
	// we switch on both ADC7 and ADC6
	ADC->ADCHER.CHEN = 0xC0;
	
	/* Step 5. Enable ADC interrupt */
	ADC->ADSR.ADF =1;     		//clear the A/D interrupt flags for safe 
	ADC->ADCR.ADIE = 1;
//	NVIC_EnableIRQ(ADC_IRQn);
	
	/* Step 6. Enable WDT module */
	ADC->ADCR.ADST=1;
}

void LED_display(float VR1) {	
	
	//we map the variable resistor values (0 to 4030) to the 4 LEDs to be on
	//we choose 4030, because during hardware testing we found that
	// a) max VR1 value is not always 4095.
	// b) mid point value is not 4095/2, but actually ~2015
	int led_num = (VR1/4030.00)*4;
	int i=0;
	
	//off all LEDs first
	for (i=12; i<16; i++) {
		DrvGPIO_SetBit(E_GPC,i);
	}
	
	//switch on correct LEDs
	for (i=0;i<=led_num;i++) {
		DrvGPIO_ClrBit(E_GPC,11+i);
	}
}


//	-----------TASK 3-------------
// 	Second variable resistor input will be used to change output on LCD display
//	Length of a bar fills up accordingly to the VR2 input

void LCD_display(float VR2) {	
	
	//during hardware testing, we find that min potientometer value is 5
	//since 5/4095 is ~0, we can leave it as it is. Otherwise, we would need to factor in this min position
	//to our calculations.
	//we map the potentiometer values (5 to 4095) to the length of the bar (0 to X_MAX)
	int bar_length = ((VR2/4095)*X_MAX);
	
	//due to the insufferable nature of hardware which INSISTS on fluctuating values despite the
	//potentiometer not being turned, we need to ensure that the redrawing of the bar
	//happens only when there is obvious manual turning, and not due to minor hardware fluctuations.
	//otherwise your LCD screen is glitchy.
	//therefore, we keep track of previous pot values, and redraw the bar only if the current pot value
	//is significantly bigger than previous pot value.
	int diff = abs(prev_VR2_state - VR2);
	
	if (diff > 2) {
		clear_LCD();
		RectangleFill(0, 2, bar_length, 6, FG_COLOR, BG_COLOR);
		prev_VR2_state = VR2;
	}
}

int32_t main(void)
{
	int i;
	char TEXT0[12] = "VR1:        ";
	char TEXT1[12] = "VR2:        ";
	
	UNLOCKREG();
	SYSCLK->PWRCON.XTL12M_EN = 1;//Enable 12MHz Crystal
  DrvSYS_Delay(5000); // waiting for 12MHz crystal stable
	SYSCLK->CLKSEL0.HCLK_S = 0;		
	LOCKREG();
	
	init_LCD();  // initialize LCD pannel
	clear_LCD();  // clear LCD panel 
	
	InitADC();		    // initialize ADC
	
	DrvGPIO_Open(E_GPA,14, E_IO_OUTPUT); // set GPA14 output for RED LED
	DrvGPIO_Open(E_GPA,13, E_IO_OUTPUT); // set GPA13 output for GREEN LED
	
	//Initialize our 4 LEDs
	for (i=12; i<=15; i++) {
		DrvGPIO_Open(E_GPC, i, E_IO_OUTPUT);
		DrvGPIO_SetBit(E_GPC, i);
	}
	
	InitTIMER0();                        // Set Timer Ticking
	InitTIMER1();                        // Set Timer Ticking
	
	while(1) {
		while(ADC->ADSR.ADF==0); // wait till conversion flag = 1, conversion is done
		ADC->ADSR.ADF=1;		     // write 1 to clear the flag
		LED_display(ADC->ADDR[7].RSLT);		//pass VR1 input to control 4 LEDs
		LCD_display(ADC->ADDR[6].RSLT);		//pass VR2 input to control bar length
		sprintf(TEXT0+5, "%4d", ADC->ADDR[7].RSLT);		//print out VR1 values
		sprintf(TEXT1+5, "%4d", ADC->ADDR[6].RSLT);		//printC outp16 VR2 values
		print_Line(2, TEXT0);	   // output TEXT to LCD
		print_Line(3, TEXT1);	   // output TEXT to LCD
		ADC->ADCR.ADST=1;		     // restart ADC sample
	}
}

