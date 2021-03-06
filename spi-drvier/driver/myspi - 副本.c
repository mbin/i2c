/*

 File: dv_spi.c

 Author: Joshua Hintze (joshh@imsar.com )




 Description: A very simple implementation for using the SPI

 port on the Davinci 644x platform. Thanks for Joshua Hintze's sharing




 Thanks goes to Sean on Davinci Mailing List

 Limitations: Currently this is written to only use a single Chip Select to read/write EEPROM




 Platform Dependencies: Davinci




 Change History:

 Date               Author       Description

 2010/08/15  Rain Peng   Porting to DM6441 for read/write EEPROM, should mknod -m 666 /dev/spi c 60 1

 */

//

///////////////////////////

// INCLUDES

//////////////////////////

#include <linux/init.h>

#include <linux/module.h>

#include <linux/kernel.h>

#include <linux/delay.h>              // udelay()

#include <linux/fs.h>                 // everything...

#include <asm/uaccess.h>              // copy_from/to_user

// Definition for SPI Base Address and the local Power or Sleep Controller LPSoC

// #include <asm/arch/hardware.h>
// #include <asm/arch/clock.h>

#include <mach/spi.h>
#include <linux/gpio.h>
#include <mach/hardware.h>
#include <linux/spi/spi.h>
#include <linux/clk.h>
///////////////////////////

// PROTOTYPES

//////////////////////////

/* Declaration of dv_spi.c functions */

int dv_spi_open(struct inode *inode, struct file *filp);

int dv_spi_release(struct inode *inode, struct file *filp);

ssize_t dv_spi_read(struct file *filp, char *buf, size_t count, loff_t

	*f_pos);

ssize_t dv_spi_write(struct file *filp, const char *buf, size_t count,

	loff_t *f_pos);

static void dv_spi_exit(void);

static int dv_spi_init(void);

///////////////////////////

// DEFINITIONS & GLOBALS

//////////////////////////

// Register definitions to control the SPI

#define SPIGCR0                                   0x01C66800  //SPI Global Control Register0

#define SPIGCR1                                   0x01C66804  //SPI Global Control Register1

#define SPIINT                                    0x01C66808  //SPI Interrupt Register

#define SPILVL                                    0x01C6680c  //SPI Interrupt Level Register

#define SPIFLG                                    0x01C66810  // SPI Flag Status Register

#define SPIPC0                                    0x01C66814  // SPI Pin Control Register 0

#define SPIPC2                                    0x01C6681C  // SPI Pin Control Register 2

#define SPIDAT1                                   0x01C6683C  // SPI Shift Register 1

#define SPIBUF                                    0x01C66840  // SPI Buffer Register

#define SPIEMU                                    0x01C66844  // SPI Emulation Register

#define SPIDELAY	                              0x01C66848  // SPI Delay Register

#define SPIDEF                                    0x01C6684C  // SPI Default Chip Select Register

#define SPIFMT0                                   0x01C66850  // SPI Data Format Register 0

#define SPIFMT1                                   0x01C66854  // SPI Data Format Register 1

#define SPIFMT2                                   0x01C66858  // SPI Data Format Register 2

#define SPIFMT3                                   0x01C6685C  // SPI Data Format Register 3

#define INTVEC0                                   0x01C66860  // SPI Interrupt Vector Register 0

#define INTVEC1                                   0x01C66864  // SPI Interrupt Vector Register 1

// Definition for GPIO Pin Multiplexing

#ifndef PINMUX1REG

#define PINMUX0REG                                0x01C40000
#define PINMUX1REG                                0x01C40004

#endif

#if 0
// use GPIO[7]

#define GPIO_DIR01                                0x01C67010

#define GPIO_OUT_DATA01                           0x01C67014

#define GPIO_SET_7                                (1 << 7)

#define GPIO_CLR_7                                ~(1 << 7)

#else

// use GPIO[7]

#endif

// SPI format - Polarity and Phase

// ***NOTE*** It doesn't seem like the SPI Phase for the davinci follows the standard

// phase as described by the motorola architecture. I.E. phase 0 = sample on rising edge of clock

// In the davinci it seems this is opposite.

#define SPI_PHASE               1

#define SPI_POLARITY            0

#define SPI_PRESCALE            16

// Macro for accessing a memory location such as a register

#define SPI_REG(reg)    (*(int *__iomem) IO_ADDRESS(reg))

// Version numbers

#define MAJOR_VERSION           153

#define MINOR_VERSION           01

// Max Read/Write buff size

#define MAX_BUF_SIZE            2048

// Global pointer to the clock struct. We use this to start up the LPSoC (local power system on chip)

// so our SPI peripheral has power going to it.

struct clk *g_clkptr = 0x0;

// We will use a 1K read buffer to store data

static unsigned char *g_readbuf = 0;

static unsigned int g_readbufcount = 0;

static unsigned int spidat1 = 0;

// Called when a userspace program opens the file

int dv_spi_open(struct inode *inode, struct file *filp)

{

	unsigned int control;

	printk("<1>open +\n");

	unsigned int old_reg = 1;

	// --------------------------------

	// Configure GPIO Pins for SPI

	// --------------------------------

	// Enable the SPI pins on the GPIO

	SPI_REG(PINMUX0REG) = 0x80020809;
	SPI_REG(PINMUX1REG) = 0x10181;

#if 0
	// let GPIO7 pin as output pin

	control = SPI_REG(GPIO_DIR01);

	// printk("<1>GPIO_DIR01 = 0x%x\n", SPI_REG(GPIO_DIR01));

	SPI_REG(GPIO_DIR01) = control & GPIO_CLR_7;

	// printk("<1>GPIO_DIR01 = 0x%x\n", SPI_REG(GPIO_DIR01));

	// pull GPIO7 to high

	control = SPI_REG(GPIO_OUT_DATA01);

	// printk("<1>GPIO_OUT_DATA01 = 0x%x\n", SPI_REG(GPIO_OUT_DATA01));

	SPI_REG(GPIO_OUT_DATA01) = control | GPIO_SET_7;

	// printk("<1>GPIO_OUT_DATA01 = 0x%x\n", SPI_REG(GPIO_OUT_DATA01));
#endif

	// Power up the SPI hardware by requesting and enabling the clock

	g_clkptr = clk_get(NULL, "spi");
	if (g_clkptr <= 0)
	{
		printk("<l>Error could not get the clock\n");
	}
	else
	{
		// printk("\n clk_name=%s\n", g_clkptr->name);
		old_reg = clk_enable(g_clkptr);
		printk("<l> get the clock\n");
	}

	// --------------------------------

	// Reset SPI

	// --------------------------------

	control = 0x00000000;

	SPI_REG(SPIGCR0) = control;

	mdelay(1); // Delay for a bit

	// Remove from reset

	control = 0x00000001;

	SPI_REG(SPIGCR0) = control;

	// printk("<1>SPIGCR0 = 0x%x\n", SPI_REG(SPIGCR0));

	// --------------------------------

	// Enable SPI CLK & Master

	// --------------------------------

	control = 0

		| (0 << 24)

		| (0 << 16) // loopback test

		| (1 << 1)

		| (1 << 0);

	control = 0x00000003;
	SPI_REG(SPIGCR1) = control;

	printk("\nSPIGCR1 = 0x%x\n", SPI_REG(SPIGCR1));

	// --------------------------------

	// Enable pins : DI,DO,CLK,EN0,EN1, set EN0 to o, EN1 to 1

	// --------------------------------

	control = 0

		| (1 << 11) // DI

		| (1 << 10) // DO

		| (1 << 9) // CLK

		| (1 << 1) // EN1

		| (1 << 0); // EN0

	control = 0x00000E01;
	SPI_REG(SPIPC0) = control;

	printk("\nSPIPC0 = 0x%x\n", SPI_REG(SPIPC0));

	/* --------------------------------

	 // Set data format in SPIFMT0 - THIS CAN BE CHANGED BY IOCTL commands

	 // SHIFTDIR in bit 20 set to 0 : MSB first

	 // POLARITY and PHASE in bit 17, 16 set to 0, 1

	 // PRESCALE in bit 15-8 set to whatever you need, SPI_CLK = SYSCLK5 / (Prescale + 1)

	 // CHARLEN in bit 4-0 set to 08 : 8 bit characters

	 -------------------------------- */
#if 0
	control = 0

		| (0 << 20) // SHIFTDIR

		| (0 << 17) // Polarity

		| (1 << 16) // Phase

		| (50 << 8) // Prescale

		| (8 << 0); // Char Len

	control = 0x00021A10;
	SPI_REG(SPIFMT0) = control;
	printk("\nSPIFMT0 = 0x%x\n", SPI_REG(SPIFMT0));
#else
	control = 0x00021A10;
	SPI_REG(SPIFMT0) = control;
	printk("\nSPIFMT0 = 0x%x\n", control);
	SPI_REG(SPIFMT1) = control;
	printk("\nSPIFMT1 = 0x%x\n", control);
#endif

	// --------------------------------

	// Set data format for used

	// --------------------------------

	spidat1 = 0

		| (1 << 28) // CSHOLD

		| (0 << 24) // Format [0]

		| (2 << 16) // CSNR   [only CS0 enbled]

		| (0 << 0); // Data

	control = spidat1;

	control = 0x00000000;

	SPI_REG(SPIDAT1) = control;

	printk("\nSPIDAT1 = 0x%x\n", SPI_REG(SPIDAT1));

	control = 0x00020000;

	SPI_REG(SPIDAT1) = control;

	printk("\nSPIDAT1 = 0x%x\n", SPI_REG(SPIDAT1));

#if 0
	// --------------------------------

	// Set hold time and setup time

	// --------------------------------

	control = 0

		| (6 << 24) // C2TDELAY

		| (7 << 16); // T2CDELAY

	SPI_REG(SPIDELAY) = control;

	printk("\nSPIDELAY = 0x%x\n", SPI_REG(SPIDELAY));
#endif

	// --------------------------------

	// Set chip select default pattern-

	// --------------------------------

	control = 0

		| (1 << 1) // EN1 inactive high

		| (1 << 0); // EN0 inactive high

	control = 0x00000001;
	SPI_REG(SPIDEF) = control;

	printk("\nSPIDEF = 0x%x\n", SPI_REG(SPIDEF));

	// --------------------------------

	// Enable for transmitting

	// --------------------------------

	control = SPI_REG(SPIGCR1);

	control = control | 1 << 24; // enable SPIENA

	SPI_REG(SPIGCR1) = 0x01000003;

	printk("\nSPIGCR1 = 0x%x\n", SPI_REG(SPIGCR1));

#if 1
	// while(SPI_REG(SPIBUF)&(0x20000000) == 0)
	{
		// printk("\nSPIBUF=%x\n",SPI_REG(SPIBUF));
	} SPI_REG(SPIDAT1) = 0x010104AA;
	while ((SPI_REG(SPIBUF) & 0x20000000)) {
		printk("\nIn Sending,%x\n", SPI_REG(SPIBUF));
	}
	printk("\n2SPIBUF=%x\n", SPI_REG(SPIBUF));
#endif

	// Zero out our read buffer
	memset(g_readbuf, 0, MAX_BUF_SIZE);
	g_readbufcount = 0;
	return 0;

}

// Called when a userspace program closes the file

int dv_spi_release(struct inode *inode, struct file *filp)

{

	printk("<1>close\n");

	// Place SPI peripheral into reset

	SPI_REG(SPIGCR0) = 0;

	// Remove the SPI output on the GPIO

	SPI_REG(PINMUX1REG) &= ~0x100;

	// Disable the clock thus removing power from the peripheral

	if (g_clkptr) {

		clk_disable(g_clkptr);
	}

	g_clkptr = 0;

	return 0;

}

// Reading from the SPI device

ssize_t dv_spi_read(struct file *filp, char *buf, size_t count, loff_t

	*f_pos)

{

	// printk("<1>read\n");

	if (g_readbufcount == 0)
		return 0; // No data

	// See if there is enough available

	if (count > g_readbufcount)
		count = g_readbufcount;

	// Transferring data to user space

	copy_to_user(buf, g_readbuf, count);

	// Now shift the memory down

	memmove(&g_readbuf[0], &g_readbuf[count], g_readbufcount - count);

	g_readbufcount -= count;

	// Advance the file pointer

	*f_pos += count;

	return count;

}

// Writing to the SPI device

ssize_t dv_spi_write(struct file *filp, const char *buf, size_t count,

	loff_t *f_pos)

{
#if 1
	unsigned char spiData;
	int len = 1;
#else
	unsigned short spiData;
	int len = 2;
#endif

	size_t i;

	unsigned int control = 0;

	unsigned int ReadByte = 0;

	printk("<1>write\n");

	// Wait until the TX buffer is clear

	control = SPI_REG(SPIBUF);
	printk("\ncontrol 0x%x\n", control);

	while (control & (1 << 29)) // Wait until the TX data has been transmitted
	{
		control = SPI_REG(SPIBUF);
		printk("\nSPIBUF 1: 0x%x\n", SPI_REG(SPIBUF));
		// Write out data one byte at a time

		for (i = 0; i < count; i++) {
			ReadByte = 0;
			// Send the data
			copy_from_user(&spiData, buf + i, len);
			if (i == (count - 1)) {
				SPI_REG(SPIDAT1) = (spidat1 & 0x0fffffff) | spiData;
				// Remove the chip select hold
			}
			else {
				SPI_REG(SPIDAT1) = spidat1 | spiData;
				// Hold the chip select line between multibytes
				printk("\nSPIDAT1 1: 0x%x\n", SPI_REG(SPIDAT1));
			}
			control = SPI_REG(SPIBUF);
			printk("\nSPIBUF 2: 0x%x\n", SPI_REG(SPIBUF));

			while (control & (1 << 29))
				// Wait until the TX data has been transmitted
			{
				// Check for data received
				if (!(control & (1 << 31))) {
					// We have just read a byte of data, store it.
					if (g_readbufcount < MAX_BUF_SIZE) {
						printk("<1>Read in byte count = %d, value =  %x!\n",
							count, control & 0xFF);
						g_readbuf[g_readbufcount] = control & 0xFF;
					}
					g_readbufcount++;
					ReadByte = 1;
				}
			}
			control = SPI_REG(SPIBUF);
		}

		// Make sure we have always read in a byte

		while (!ReadByte) {
			// Check for data received
			if (!(control & (1 << 31))) {

				printk("<1>we received a byte with value %x\n", control & 0xFF);

				// We have just read a byte of data, store it.

				if (g_readbufcount < MAX_BUF_SIZE)

				{

					printk("<1>Read in byte count = %d, value =  %x!\n", count,
						control & 0xFF);

					g_readbuf[g_readbufcount] = control & 0xFF;

					g_readbufcount++;

					ReadByte = 1;

				}

			}

			control = SPI_REG(SPIBUF);

		}

	}

	return count;

}

ssize_t dv_spi_ioctl(struct inode *inode, struct file *filp,

	unsigned int cmd, unsigned long arg)

{

	return 0;

}

// Structure that declares the usual file access functions

static struct file_operations dv_spi_fops = {

	.owner = THIS_MODULE,

	.read = dv_spi_read,

	.write = dv_spi_write,

	.ioctl = dv_spi_ioctl,

	.open = dv_spi_open,

	.release = dv_spi_release

};

static int dv_spi_init(void)

{

	int result;

	// unsigned int control = 0;

	gpio_direction_output(37, 0);

	/* Registering device */

	result = register_chrdev(MAJOR_VERSION, "spi", &dv_spi_fops);
	if (result < 0) {
		printk("<1>dv_spi: cannot obtain major number %d\n", MAJOR_VERSION);
		return result;
	}

	// Allocate space for the read buffer

	g_readbuf = kmalloc(MAX_BUF_SIZE, GFP_KERNEL);

	if (!g_readbuf) {

		result = -ENOMEM;

		dv_spi_exit();

		return result;

	}

	// printk("<1>PLL_SYS5 = 0x%x\n", SPI_REG(0x01c40964));

	// printk("<1>PLL_SYS5_STAT = 0x%x\n", SPI_REG(0x01c40950));

	printk("<1>Inserting SPI module\n");

	return 0;

}

static void dv_spi_exit(void)

{

	// pull GPIO7 to low

	// unsigned int control;

	// control = SPI_REG(GPIO_OUT_DATA01);

	// printk("<1>GPIO_OUT_DATA01 = 0x%x\n", SPI_REG(GPIO_OUT_DATA01));

	// SPI_REG(GPIO_OUT_DATA01) = control & GPIO_CLR_7;

	// printk("<1>GPIO_OUT_DATA01 = 0x%x\n", SPI_REG(GPIO_OUT_DATA01));

	/* Freeing the major number */

	unregister_chrdev(MAJOR_VERSION, "spi");

	/* Freeing buffer memory */

	if (g_readbuf)

		kfree(g_readbuf);

	if (g_clkptr)

		dv_spi_release(0, 0);

	printk("<1>Removing SPI module\n");

}

MODULE_LICENSE("Dual BSD/GPL");

module_init(dv_spi_init);

module_exit(dv_spi_exit);
