#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <asm/io.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/spi/spi.h>

/******************************************************************************************************
* Defines
******************************************************************************************************/
//Driver Specific Default and Reference Values
#define DEFAULT_SAMPLES_PER_STEP	10000
#define BITS				8
#define	AMP_BITS			3
#define DEFAULT_STEPS			10
#define DEFAULT_SAMPLES			(DEFAULT_SAMPLES_PER_STEP * DEFAULT_STEPS)
#define DEFAULT_KMALLOC			(DEFAULT_SAMPLES / PAGE_SIZE + 2)

#define BIT0			0
#define BIT1			1
#define	BIT2			25
#define BIT3			24
#define BIT4			23
#define BIT5			18
#define BIT6			15
#define	BIT7			14
#define AMPCTL0			17
#define AMPCTL1			21
#define AMPCTL2			22
#define CLOCK_PIN		4

#define UPDATE_STEPS		1
#define UPDATE_SAMPLES_PER_STEP (1 << 1)
#define UPDATE_DELAY		(1 << 2)
#define UPDATE_DAC_OCT		(1 << 3)
#define UPDATE_AMP_BITS		(1 << 4)

//BCM2835 Peripheral Register Information
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
#define SPI_BUS			1
#define SPI_BUS_CS		1
#define DEFAULT_SPI_CLK_SPEED	20000000

/******************************************************************************************************
* Prototypes
******************************************************************************************************/
/********** Open **********/
static int specAn_open(struct inode*, struct file*);

/********** Release **********/
static int specAn_release(struct inode*, struct file*);

/********** Write ***********/
static ssize_t specAn_write(struct file*, const char*, size_t, loff_t*);
int update_clock(void);
int update_buffer(void);

/********** Read **********/
static ssize_t specAn_read(struct file*, char*, size_t, loff_t*);
int take_samples(void);
void convert_data_to_byte(void);
int increment_lo(void);
static void spi_transfer_complete_handler(void*);

/********** MMap ***********/
static int specAn_mmap(struct file*, struct vm_area_struct*);

/********** Probe **********/
static int specAn_probe(struct spi_device*);

/********** Remove **********/
static int specAn_remove(struct spi_device*);

/********** Initialize **********/
int init_module(void);
int init_cdrv(void);
int init_spidrv(void);
int add_spidev_to_bus(void);
int init_gpio(void);
int init_mem(void);
int init_structs(void);

/*********** Cleanup **********/
void cleanup_module(void);
void clean_structs(void);
void clean_mem(void);
void clean_gpio(void);
void clean_spidrv(void);
void clean_cdrv(void);

/******************************************************************************************************
* Driver Structs
******************************************************************************************************/
struct specAn_cdev_driver
{
	struct cdev chardev;
	dev_t dev_no;
	int refcount;	
	int steps;
	int samples_per_step;
	int bits;
	int amp_bits;
	int page_order;
	int n_pages;
	int* pins;
	int* amp_pins;
	int* amp_vals;
	uint32_t delay;
	uint32_t abort_delay;
	uint8_t* data;
	uint32_t* buffer;
	uint8_t* buffer_t;
	volatile void* sys_clock;
	volatile void* sample;
};

struct specAn_spidev_driver
{
	struct spi_message msg;
	struct spi_transfer transfer;
	struct spi_device* spi_dev;
	uint8_t* tx_buf;
	uint8_t* oct;
	uint16_t* dac;
	uint8_t cnf;
	uint8_t busy;
	int lo_step;
	spinlock_t spi_lock;
};

struct specAn_edit_values
{
	uint8_t update;	
	int steps;
	int samples_per_step;
	uint32_t delay;
	int amp_vals[3];
};

static struct specAn_cdev_driver cdriver;
static struct specAn_spidev_driver spidriver;

/******************************************************************************************************
* Character Device
******************************************************************************************************/
/********** Structs **********/
static struct file_operations specAn_fops =
{
	.read		= specAn_read,
	.write		= specAn_write,
	.open		= specAn_open,
	.release	= specAn_release,
	.mmap		= specAn_mmap,
	.owner		= THIS_MODULE
};

/********** Open **********/
static int specAn_open(struct inode* node, struct file* filp)
{
	cdriver.refcount++;
	try_module_get(THIS_MODULE);
	return 0;
}

/********** Release **********/
static int specAn_release(struct inode* node, struct file* filp)
{
	cdriver.refcount--;
	module_put(THIS_MODULE);
	return 0;
}

/********** Write **********/
static ssize_t specAn_write(struct file* filp, const char* buffer, size_t length, loff_t* offset)
{
	struct specAn_edit_values* edit_vals;
	int array_start = sizeof(*edit_vals);
	uint16_t* dac_ptr = 0;
	uint8_t* oct_ptr = 0;
	int i = 0;
		
	if(length < sizeof(*edit_vals))
		return -1;

	edit_vals = (struct specAn_edit_values*)buffer;
	
	if(edit_vals->update & UPDATE_STEPS)
		cdriver.steps = edit_vals->steps;

	if(edit_vals->update & UPDATE_SAMPLES_PER_STEP)
	{
		cdriver.samples_per_step = edit_vals->samples_per_step;
		if(update_buffer() < 0)
			return -1;
	}

	if(cdriver.samples_per_step * cdriver.steps > cdriver.n_pages * PAGE_SIZE)
	{
		printk(KERN_ALERT "Total sample space larger than 1 MB allocated storage requested, reverting to default sizes\n");
		cdriver.samples_per_step = DEFAULT_SAMPLES_PER_STEP;
		cdriver.steps = DEFAULT_STEPS;
		update_buffer();
		return -1;
	}

	if(edit_vals->update & UPDATE_DELAY)
	{
		switch(edit_vals->delay)
		{
			case 0: //1 MHz		DIV = 500	PLL D
			case 1: //500 kHz	DIV = 1000	
			case 2: //333.333 kHz 	DIV = 1500
			case 3: //250 kHz	DIV = 2000
			case 4: //200 kHz	DIV = 2500
			case 5: //166.667 kHz	DIV = 3000
			case 6: //142.857 kHz	DIV = 3500
			case 7: //125 kHz	DIV = 4000
				break;
			default:
				printk(KERN_ALERT "Delay updated to an invalid value\n");
				return -1;
		}

		cdriver.delay = edit_vals->delay;
		cdriver.abort_delay = 2 * (1 + cdriver.delay) * cdriver.samples_per_step;
		if(update_clock() < 0)
			return -1;
	}

	if(edit_vals->update & UPDATE_AMP_BITS)
	{
		cdriver.amp_vals[0] = edit_vals->amp_vals[0];
		cdriver.amp_vals[1] = edit_vals->amp_vals[1];
		cdriver.amp_vals[2] = edit_vals->amp_vals[2];
	}		

	if(edit_vals->update & UPDATE_DAC_OCT)
	{
		if(length > sizeof(*edit_vals) + edit_vals->steps * (sizeof(uint16_t) + sizeof(uint8_t)))
		{
			return -1;
		}

		for(i = 0; i < cdriver.steps; i++)
		{
			spidriver.dac[i] = *dac_ptr;
			dac_ptr++;
		}

		array_start += cdriver.steps * sizeof(uint16_t);
		oct_ptr = (uint8_t*)(&buffer[array_start]);
		
		for(i = 0; i < cdriver.steps; i++)
		{
			spidriver.oct[i] = *oct_ptr;
			oct_ptr++;
		}
	}

	return 0;		
}

int update_clock(void)
{
	volatile void* gp_clock;
	uint32_t reg_val;
	int clock_try_count = 0;
	int ret = 0;
	uint32_t set_clk_ctl = 0;
	uint32_t clock_div = 0;
	gp_clock = ioremap(BCM2835_PERI_BASE | BCM2835_PERI_GPCLK, 2 * BCM2835_REG_SIZE);

clockset:	
	reg_val = (uint32_t)ioread32(gp_clock);
	iowrite32(~CLOCK_ENABLE & reg_val, gp_clock);
	
	reg_val = (uint32_t)ioread32(gp_clock);	

	if(reg_val & CLOCK_BUSY)
	{
		if(clock_try_count > 100)
		{
			printk(KERN_ALERT "Clock is busy\n");
			ret = -1;
			goto iounmap;
		}
		udelay(100);
		clock_try_count++;
		goto clockset;
	}

	set_clk_ctl = CLOCK_PWD | CLOCK_ENABLE | CLOCK_SRC_PLLD;
	iowrite32(set_clk_ctl, gp_clock);

	clock_div = (1 + cdriver.delay) * 500 << 12;
	iowrite32(CLOCK_PWD | clock_div, gp_clock + BCM2835_REG_SIZE * GPCLK_DIV0);
iounmap:
	
	iounmap(gp_clock);

	return ret;
}

int update_buffer(void)
{
	kfree(cdriver.buffer);
	kfree(cdriver.buffer_t);

	cdriver.buffer = kmalloc_array(cdriver.samples_per_step, sizeof(uint32_t), GFP_KERNEL);
	cdriver.buffer_t = kmalloc_array(cdriver.samples_per_step, sizeof(uint32_t), GFP_KERNEL);

	if(!cdriver.buffer | !cdriver.buffer_t)
	{
		printk(KERN_ALERT "Buffers failed to reallocate\n");
		return -1;
	}

	return 0;
}

/********** Read **********/
static ssize_t specAn_read(struct file* filp, char* buffer, size_t length, loff_t* offset)
{
	int i = 0, error = 0, wait = 0;
	spidriver.lo_step = 0;
	
	while(i < cdriver.steps)
	{
		int status = 0

		if(error >= 100)
			return -1;

		status = take_samples();

		if(status < 0)
		{
			error++;
			continue;
		}

		convert_data_to_byte();

		if(increment_lo() < 0)
			return -1;

		i++;

		while(spidriver.busy)
		{
			if(wait > 10000)
				return -2;
			wait++;
		}
	}

	return 0;
}

int take_samples(void)
{
	unsigned long irq_flags = 0;
	uint32_t t = 0, next_t = 0, abort = 0;
	uint32_t* buf_ptr 	= cdriver.buffer;
	uint8_t* buf_t_ptr 	= cdriver.buffer_t;
	uint32_t* buf_end	= cdriver.buffer + cdriver.samples_per_step;
	int overruns 		= 0;

	local_irq_save(irq_flags);
	local_fiq_disable();

	t = (uint32_t)ioread32(cdriver.sys_clock);
	abort = t + cdriver.abort_delay;

	while(next_t < abort)
	{
		do{ next_t = (uint32_t)ioread32(cdriver.sys_clock); } while(t > next_t);

		*buf_ptr++	= (uint32_t)ioread32(cdriver.sample);
		*buf_t_ptr++	= (uint8_t)(next_t - t - cdriver.delay);

		overruns 	= next_t - t - cdriver.delay;
		t 		= next_t + cdriver.delay;
		
		if(buf_ptr >= buf_end)
			break;
	}

	if(next_t >= abort)
		overruns = -1;

	local_fiq_enable();
	local_irq_restore(irq_flags);

	return overruns;
}

void convert_data_to_byte(void)
{
	uint8_t* data_ptr 	= cdriver.data + cdriver.samples_per_step * spidriver.lo_step;
	uint8_t* data_end 	= data_ptr + cdriver.samples_per_step;
	int t = 0;
	
	for(;data_ptr < data_end; data_ptr++)
	{
		if(cdriver.buffer_t[t] == 0)
		{
			int j 		= 0;
			*data_ptr 	= 0;
			
			for(;j < cdriver.bits; j++)
			{
				*data_ptr |= (((1 < cdriver.pins[j]) & cdriver.buffer[t]) >> cdriver.pins[j]) << j;
			}
			t++;
		}
		else
		{
			*data_ptr = 0;
			cdriver.buffer_t[t]--;
		}
	}
}

int increment_lo(void)
{
	unsigned long flags = 0;
	int status = 0;
	uint16_t spi_transfer;

	spidriver.lo_step = (spidriver.lo_step + 1) % cdriver.steps;

	/*
	*		SPITRANSFER = [OCT3 OCT2 OCT1 OCT0 DAC9 DAC8 DAC7 DAC6] [DAC5 DAC4 DAC3 DAC2 DAC1 DAC0 CNF1 CNF0]
	*/
	spi_transfer 		= (spidriver.oct[spidriver.lo_step] << 12) | (spidriver.dac[spidriver.lo_step] << 2) | (spidriver.cnf);
	
	spi_message_init(&spidriver.msg);

	spidriver.msg.complete 		= spi_transfer_complete_handler;
	spidriver.msg.context 		= NULL;

	spidriver.tx_buf[0] 		= (uint8_t)((spi_transfer & 0xFF00) >> 8);
	spidriver.tx_buf[1]		= (uint8_t)(spi_transfer & 0x00FF);

	spidriver.transfer.tx_buf 	= spidriver.tx_buf;
	spidriver.transfer.rx_buf 	= NULL;
	spidriver.transfer.len 		= sizeof(spi_transfer);

	spi_message_add_tail(&spidriver.transfer, &spidriver.msg);

	spin_lock_irqsave(&spidriver.spi_lock, flags);

	if(spidriver.spi_dev)
		status = spi_sync(spidriver.spi_dev, &spidriver.msg);
	else
		status = -ENODEV;

	spin_unlock_irqrestore(&spidriver.spi_lock, flags);

	if(status == 0)
		spidriver.busy = 1;

	return status;	
}

static void spi_transfer_complete_handler(void* arg)
{
	spidriver.busy = 0;
}

/********** MMap **********/
static int specAn_mmap(struct file* filp, struct vm_area_struct* vma)
{
	int ret 	= 0;
	unsigned long length 	= vma->vm_end - vma->vm_start;

	if(length > cdriver.n_pages * PAGE_SIZE);
		ret 	= -1;
	
	if(vma->vm_pgoff == 0)
		ret 	= remap_pfn_range(vma, vma->vm_start, virt_to_phys(cdriver.data) >> PAGE_SHIFT, length, vma->vm_page_prot);

	return ret;
}
/******************************************************************************************************
* SPI Device
******************************************************************************************************/
/********** Structs **********/
static struct spi_driver specAn_spi_driver =
{
	.driver =
	{
		.name 	= "gpio_spi_driver",
		.owner	= THIS_MODULE
	},
	.probe		= specAn_probe,
	.remove		= specAn_remove,
};

/********** Probe **********/
static int specAn_probe(struct spi_device* sdev)
{
	unsigned long flags;
	
	if(!sdev)
	{
		printk(KERN_ALERT "SPI device is null\n");
		return -1;
	}

	spin_lock_irqsave(&spidriver.spi_lock, flags);
	
	sdev->chip_select = 0;
	sdev->max_speed_hz = 20000000;
	
	spidriver.spi_dev = sdev;

	spin_unlock_irqrestore(&spidriver.spi_lock, flags);

	return 0;
}

/********** Remove **********/
static int specAn_remove(struct spi_device* sdev)
{
	unsigned long flags;

	spin_lock_irqsave(&spidriver.spi_lock, flags);
	spidriver.spi_dev = NULL;
	spin_unlock_irqrestore(&spidriver.spi_lock, flags);
	
	return 0;
}
/******************************************************************************************************
* Driver Initiliazation
******************************************************************************************************/
int init_module(void)
{
	int ret = 0;
	memset(&cdriver, 0, sizeof(cdriver));
	memset(&spidriver, 0, sizeof(spidriver));

	spin_lock_init(&spidriver.spi_lock);

	if(init_structs() < 0)
		goto fail_1;
	
	printk(KERN_INFO "Structs Initialized\n");

	if(init_cdrv() < 0)
		goto fail_2;
	
	printk(KERN_INFO "Character Device Allocated and Registered\n");

	if(init_spidrv() < 0)
		goto fail_3;
	
	printk(KERN_INFO "SPI device Allocated and Registered\n");

	if(init_gpio() < 0)
		goto fail_4;

	printk(KERN_INFO "GPIO Formatted\n");

	if(init_mem() < 0)
		goto fail_5;

	printk(KERN_INFO "Memory allocated\n");

	return 0;

fail_5:
	ret++;
	clean_mem();

fail_4:
	ret++;
	clean_gpio();

fail_3:
	ret++;
	clean_spidrv();

fail_2:
	ret++;
	clean_cdrv();

fail_1:
	ret++;
	clean_structs();

	printk(KERN_ALERT "Initialization Failed with error: %d\n", ret);
	return -ret;
}

int init_cdrv(void)
{
	int error = 0;

	cdriver.dev_no = MKDEV(0, 0);
	error = alloc_chrdev_region(&cdriver.dev_no, 0, 1, "specAn_driver");

	if(error < 0)
	{
		printk(KERN_ALERT "Character device allocation failed: %d\n", error);
		return -1;
	}
	
	cdev_init(&cdriver.chardev, &specAn_fops);
	cdriver.chardev.owner = THIS_MODULE;

	error = cdev_add(&cdriver.chardev, cdriver.dev_no, 1);
	
	if(error < 0)
	{
		printk(KERN_ALERT "Character device add failed: %d\n", error);
		return -1;
	}

	return 0;
}

int init_spidrv(void)
{
	int error = 0;
	spidriver.tx_buf = kmalloc_array(2, sizeof(uint8_t), GFP_KERNEL);
	if(!spidriver.tx_buf)
	{
		printk(KERN_ALERT "SPI Buffer Allocation Failed\n");
		return -1;
	}

	error = spi_register_driver(&specAn_spi_driver);
	if(error < 0)
	{
		printk(KERN_ALERT "SPI device registration failed: %d\n", error);
		return -1;
	}
	
/*	error = add_spidev_to_bus();
	if(error < 0)
	{
		printk(KERN_ALERT "SPI device failed to add to bus: %d\n", error);
		return -1;
	}

*/	return 0;
}

/*int add_spidev_to_bus(void)
{
	struct spi_master* master;
	struct spi_device* spi_dev;
	int status = 0;

	master = spi_busnum_to_master(1);
	if(!master)
	{
		printk(KERN_ALERT "SPI bus to master returned null: %d\n", 1);
		return -1;
	}

	spi_dev = spi_alloc_device(master);
	if(!spi_dev)
	{
		printk(KERN_ALERT "SPI device failed to allocate\n");
		return -1;
	}

	spi_dev->chip_select 	= 1;
	spi_dev->max_speed_hz	= DEFAULT_SPI_CLK_SPEED;
	spi_dev->mode		= SPI_MODE_0 | SPI_CS_HIGH;
	spi_dev->bits_per_word	= 16;
	spi_dev->irq		= -1;
	spi_dev->controller_state	= NULL;
	spi_dev->controller_data	= NULL;

	strlcpy(spi_dev->modalias, "specAn_driver", 14);
	
	status = spi_add_device(spi_dev);

	if(status < 0)
	{
		printk(KERN_ALERT "SPI add device failed: %d\n", status);
		return -1;
	}

	put_device(&master->dev);

	return 0;
}*/

int init_gpio(void)
{
	volatile void* gpio_base = 0;
	volatile void* gp_clock = 0;

	uint32_t set_inp = 0;
	uint32_t reg_val = 0;
	uint32_t set_clk_out = 0;
	uint32_t set_clk_ctl = 0;

	int clock_try_count = 0;
	
	int i = 0;
	int ret = 0;

	gpio_base 		= ioremap(BCM2835_PERI_BASE | BCM2835_PERI_GPIO, 16 * BCM2835_REG_SIZE);
	cdriver.sample 		= ioremap(BCM2835_PERI_BASE | BCM2835_PERI_GPIO | (BCM2835_REG_SIZE * GPIO_LVL_GET), BCM2835_REG_SIZE);
	cdriver.sys_clock	= ioremap(BCM2835_PERI_BASE | BCM2835_SYS_CLK, BCM2835_REG_SIZE);
	gp_clock		= ioremap(BCM2835_PERI_BASE | BCM2835_PERI_GPCLK, 2 * BCM2835_REG_SIZE);

	for(i = 0; i < cdriver.bits; i++)
	{
		int offset = cdriver.pins[i] / 10;
		uint32_t reg_val = (uint32_t)ioread32(gpio_base + offset * BCM2835_REG_SIZE);
		uint32_t set_inp = ~(7 << (3 * (cdriver.pins[i] % 10))) & reg_val;
		uint32_t set_clr = 1 << cdriver.pins[i];
		
		iowrite32(set_inp, gpio_base + offset * BCM2835_REG_SIZE);
		iowrite32(set_clr, gpio_base + GPIO_LVL_CLR * BCM2835_REG_SIZE);
	}

	for(i = 0; i < cdriver.amp_bits; i++)
	{
		int offset = cdriver.amp_pins[i] / 10;
		uint32_t reg_val = (uint32_t)ioread32(gpio_base + offset * BCM2835_REG_SIZE);
		uint32_t set_inp = ~(7 << (3 * (cdriver.amp_pins[i] % 10))) & reg_val;
		uint32_t set_clr = 1 << cdriver.amp_pins[i];
		uint32_t set_val = cdriver.amp_vals[i] << cdriver.amp_pins[i];
		uint32_t set_out = 0;

		iowrite32(set_inp, gpio_base + offset * BCM2835_REG_SIZE);
		reg_val = (uint32_t)ioread32(gpio_base + offset * BCM2835_REG_SIZE);

		set_out = (1 << (3 * (cdriver.amp_pins[i] % 10))) | reg_val;

		iowrite32(set_out, gpio_base + offset * BCM2835_REG_SIZE);
		iowrite32(set_clr, gpio_base + GPIO_LVL_CLR * BCM2835_REG_SIZE);
		iowrite32(set_val, gpio_base + GPIO_LVL_SET * BCM2835_REG_SIZE);
	}

	reg_val = (uint32_t)ioread32(gpio_base);
	set_inp = ~(7 << (3 * CLOCK_PIN)) & reg_val;

	iowrite32(set_inp, gpio_base);
	
	reg_val = (uint32_t)ioread32(gpio_base);
	set_clk_out = (GPIO_ALT0 << (3 * CLOCK_PIN)) | reg_val;
	iowrite32(set_clk_out, gpio_base);

	clock_try_count = 0;
clockset:
	reg_val = (uint32_t)ioread32(gp_clock);
	iowrite32(~CLOCK_ENABLE & reg_val, gp_clock);
	
	reg_val = (uint32_t)ioread32(gp_clock);	

	if(reg_val & CLOCK_BUSY)
	{
		if(clock_try_count > 100)
		{
			printk(KERN_ALERT "Clock is busy\n");
			ret = -1;
			goto iounmap;
		}
		udelay(100);
		clock_try_count++;
		goto clockset;
	}

	set_clk_ctl = CLOCK_PWD | CLOCK_ENABLE | CLOCK_SRC_PLLD;
	iowrite32(CLOCK_PWD | DEFAULT_CLOCK_DIV, gp_clock + BCM2835_REG_SIZE * GPCLK_DIV0);
	iowrite32(set_clk_ctl, gp_clock);

iounmap:
	iounmap(gp_clock);
	iounmap(gpio_base);

	return ret;
}

int init_mem(void)
{
	int i = 0;
	cdriver.buffer = kmalloc_array(cdriver.samples_per_step, sizeof(*cdriver.buffer), GFP_KERNEL);
	cdriver.buffer_t = kmalloc_array(cdriver.samples_per_step, sizeof(*cdriver.buffer_t), GFP_KERNEL);

	if(!cdriver.buffer || !cdriver.buffer_t)
	{
		printk(KERN_ALERT "Buffers failed to be allocated\n");
		return -1;
	}

	cdriver.data = (uint8_t*)__get_free_pages(GFP_KERNEL, cdriver.page_order);

	if(!cdriver.data)
	{
		printk(KERN_ALERT "Memory pages for mmaping failed to allocate\n");
		return -1;
	}

	for(i = 0; i < cdriver.n_pages; i++)
	{
		SetPageReserved(virt_to_page(cdriver.data) + i);
	}	

	return 0;
}

int init_structs(void)
{
	int i = 0;
	int pins[]		= {BIT0, BIT1, BIT2, BIT3, BIT4, BIT5, BIT6, BIT7};
	int amp_pins[]		= {AMPCTL0, AMPCTL1, AMPCTL2};
	int amp_vals[]		= {0, 0, 1};
	uint8_t oct[]		= {8, 8, 8, 8, 8, 8, 8, 8, 8, 8};
	uint16_t dac[]		= {535, 576, 615, 651, 686, 719, 751, 781, 810, 837};
	/* 360.037 kHz, 370.065 kHz, 380.136 kHz, 389.932 kHz, 399.952 kHz, 409.883 kHz, 419.996 kHz, 429.940 kHz, 440.012 kHz, 449.822 kHz*/

	cdriver.refcount 	= 0;
	cdriver.steps		= DEFAULT_STEPS;
	cdriver.samples_per_step= DEFAULT_SAMPLES_PER_STEP;
	cdriver.bits		= BITS;
	cdriver.amp_bits	= AMP_BITS;
	cdriver.page_order	= 8;
	cdriver.n_pages		= (1 << cdriver.page_order);
	cdriver.pins		= kmalloc_array(BITS, sizeof(*cdriver.pins), GFP_KERNEL);
	cdriver.amp_pins	= kmalloc_array(AMP_BITS, sizeof(*cdriver.amp_pins), GFP_KERNEL);
	cdriver.amp_vals	= kmalloc_array(AMP_BITS, sizeof(*cdriver.amp_vals), GFP_KERNEL);
	cdriver.delay		= 0;
	cdriver.abort_delay	= 2 * (1 + cdriver.delay) * cdriver.samples_per_step;
	
	spidriver.oct		= kmalloc_array(cdriver.steps, sizeof(*spidriver.oct), GFP_KERNEL);
	spidriver.dac		= kmalloc_array(cdriver.steps, sizeof(*spidriver.dac), GFP_KERNEL);
	spidriver.cnf		= 0;
	spidriver.busy 		= 0;
	spidriver.lo_step	= 9;
	increment_lo();

	for(i = 0; i < BITS; i++)
	{
		cdriver.pins[i] = pins[i];
	}

	for(i = 0; i < AMP_BITS; i++)
	{
		cdriver.amp_pins[i] = amp_pins[i];
		cdriver.amp_vals[i] = amp_vals[i];
	}

	for(i = 0; i < cdriver.steps; i++)
	{
		spidriver.dac[i] = dac[i];
		spidriver.oct[i] = oct[i];
	}
	
	return 0;
}

/******************************************************************************************************
* Driver Cleanup
******************************************************************************************************/
void cleanup_module(void)
{
	clean_structs();
	clean_mem();
	clean_gpio();
	clean_spidrv();
	clean_cdrv();
}

void clean_structs(void)
{
	kfree(cdriver.pins);
	kfree(cdriver.amp_pins);
	kfree(cdriver.amp_vals);

	kfree(spidriver.oct);
	kfree(spidriver.dac);
}

void clean_mem(void)
{
	int i =0;
	for(i = 0; i < cdriver.n_pages; i++)
	{
		ClearPageReserved(virt_to_page(cdriver.data) + i);
	}
	
	kfree(cdriver.buffer);
	kfree(cdriver.buffer_t);
	free_pages((unsigned long)cdriver.data, cdriver.page_order);
	cdriver.data = 0;
}

void clean_gpio()
{
	uint32_t set_pin = 0;
	volatile void* gpio_base 	= ioremap(BCM2835_PERI_BASE | BCM2835_PERI_GPIO, 16 * BCM2835_REG_SIZE);
	volatile void* gp_clock	= ioremap(BCM2835_PERI_BASE | BCM2835_PERI_GPCLK, 2 * BCM2835_REG_SIZE);

	uint32_t reg_val = (uint32_t)ioread32(gp_clock);
	uint32_t set_clock = CLOCK_PWD | (~CLOCK_ENABLE & reg_val);
	iowrite32(set_clock, gp_clock);

	reg_val = (uint32_t)ioread32(gpio_base);
	set_pin = ~(7 << 3 * CLOCK_PIN) & reg_val;
	iowrite32(set_pin, gpio_base);

	iounmap(gpio_base);
	iounmap(gp_clock);
	iounmap(cdriver.sample);
	iounmap(cdriver.sys_clock);
}

void clean_spidrv(void)
{
	kfree(spidriver.tx_buf);
	spi_unregister_device(spidriver.spi_dev);
	spi_unregister_driver(&specAn_spi_driver);
}

void clean_cdrv(void)
{
	cdev_del(&cdriver.chardev);
	unregister_chrdev_region(cdriver.dev_no, 1);
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("dandy");
MODULE_DESCRIPTION("RasPi Spectrum Analyzer Driver Rev 2.0");
