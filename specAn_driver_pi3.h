#ifndef SPECAN_DRIVER_3_H
#define SPECAN_DRIVER_3_H
/******************************************************************************************************
* Includes
******************************************************************************************************/

/******************************************************************************************************
* Defines
******************************************************************************************************/
//Driver Specific Default and Reference Values
#define DEFAULT_SAMPLES_PER_STEP	(1 << 13)
#define BITS				8
#define	AMP_BITS			3
#define DEFAULT_STEPS			10
#define DEFAULT_SAMPLES			(DEFAULT_SAMPLES_PER_STEP * DEFAULT_STEPS)
#define DRIVER_NAME			"specAn_driver_pi3"

#define BIT0			25
#define BIT1			24
#define	BIT2			23
#define BIT3			18
#define BIT4			15
#define BIT5			14
#define BIT6			3
#define	BIT7			2
#define AMPCTL0			22
#define AMPCTL1			17
#define AMPCTL2			27
#define CLOCK_PIN		4

#define UPDATE_STEPS		1
#define UPDATE_SAMPLES_PER_STEP (1 << 1)
#define UPDATE_DELAY		(1 << 2)
#define UPDATE_DAC_OCT		(1 << 3)
#define UPDATE_AMP_BITS		(1 << 4)

//BCM2835-6 Peripheral Register Information
#define BCM2836_PERI_BASE	0x3F000000
#define BCM2835_PERI_BASE	0x20000000
#define BCM2835_REG_SIZE	sizeof(uint32_t)

//GPIO Peripheral Register Information
#define BCM2835_PERI_GPIO	0x200000
#define GPIO_FN_SEL		0
#define GPIO_LVL_SET		7
#define GPIO_LVL_CLR		10
#define GPIO_LVL_GET		13

#define GPIO_INP		0
#define GPIO_OUT		1
#define GPIO_ALT0		4
#define GPIO_ALT1		5
#define GPIO_ALT2		6
#define GPIO_ALT3		7
#define GPIO_ALT4		3
#define GPIO_ALT5		2

//General Purpose Clock Register Information
#define BCM2835_PERI_GPCLK	0x101070
#define GPCLK_CTL0		0
#define GPCLK_DIV0		1

#define CLOCK_PWD		0x5A000000
#define CLOCK_SRC_GND		0
#define CLOCK_SRC_OSC		1
#define CLOCK_SRC_DBG0		2
#define CLOCK_SRC_DBG1		3
#define CLOCK_SRC_PLLA		4
#define CLOCK_SRC_PLLC		5
#define CLOCK_SRC_PLLD		6
#define CLOCK_SRC_HDMI		7
#define DEFAULT_CLOCK_DIV	(500 << 12)
#define CLOCK_ENABLE		(1 << 4)
#define CLOCK_BUSY		(1 << 7)

//System Clock Location
#define BCM2835_SYS_CLK		0x3004

//SPI Information
#define SPI_BUS			0
#define SPI_BUS_CS		0
#define SPI_MAX_CLOCK		20000000
#define SPI_BITS_PER_WORD	16
#define SPI_BUFFER_LEN		2

/******************************************************************************************************
* Driver Structs
******************************************************************************************************/
/*********************************************************************************/
/* Struct for passing edits to the driver.					 */
/* update: stores flags for whivh values need to be updated.			 */
/* steps: the number of steps the local oscillator must take			 */
/* pow2_samples_per_step: the number of samples per step, as a power of 2	 */
/* delay: the delay between samples, in microseconds				 */
/* amp_vals: the amplifier values to be set					 */
/* lo_values: the local oscillator values as a list of oct values then dac values*/
/*********************************************************************************/
struct specAn_edit_values
{
	uint16_t update;	
	uint16_t steps;
	uint16_t pow2_samples_per_step;
	uint16_t delay;
	uint16_t amp_vals;
	uint16_t lo_values[0];//OCT values followed by DAC values
};

#endif
