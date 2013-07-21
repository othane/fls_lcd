
/*  
 * proof of concept lcd driver
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/ioport.h>
#include <asm/io.h>

#define SYSCON_BASE (0x80004000)

#define E_IN_BASE  (SYSCON_BASE + 0x26)
#define E_OUT_BASE (SYSCON_BASE + 0x16)
#define E_DIR_BASE (SYSCON_BASE + 0x1e)

#define RED_OUT_BASE (SYSCON_BASE + 0x12)
static void __iomem *red_out_base = NULL;
#define RED_BIT (1<<12)


static void set_red(int red)
{
	unsigned short val = ioread16(red_out_base);
	
	printk(KERN_INFO "set red\n");

	if (val & RED_BIT)
		printk(KERN_INFO "red led if off\n");
	else
		printk(KERN_INFO "red led if on\n");

	// set red on
	if (red)
		val |= RED_BIT;
	else
		val &= ~RED_BIT;
	iowrite16(red_out_base, val);
}

int lcd_init(void)
{
	int ret = 0;

	// dbg start up msg
	printk(KERN_INFO "LCD POC driver started\n");

	// grab control over the memory for the io pins
	printk(KERN_INFO "requesting red output region\n");
	if (!request_region(RED_OUT_BASE, 2, "lcd"))
	{
		printk(KERN_ERR "%s: requested I/O region (%#x:2) is "
			   "in use\n", "lcd, RED_OUT_BASE", RED_OUT_BASE);
		ret = -ENODEV;
		goto out1;
	}

	printk(KERN_INFO "ioport_map red output region\n");
	red_out_base = ioport_map(RED_OUT_BASE, 2);
	if (!red_out_base)
	{
		printk(KERN_ERR "%s: requested I/O region (%#x:2) is "
			   "in use\n", "lcd, RED_OUT_BASE", RED_OUT_BASE);
		ret = -EIO;
		goto out2;
	}
	
	// just set the some output for now
	printk(KERN_INFO "calling set red\n");
	set_red(1);

	return 0;

	// bail out section
	printk(KERN_INFO "bailing out !\n");
out2:
	if (red_out_base)
		ioport_unmap(red_out_base);
	red_out_base = NULL;
	release_region(RED_OUT_BASE, 2);
out1:
	return ret;
}

void lcd_cleanup(void)
{
	// clear the e pin so we can retest
	printk(KERN_INFO "calling clear red\n");
	set_red(0);
	
	// clear resources
	printk(KERN_INFO "freeing resources\n");
	if (red_out_base)
		ioport_unmap(red_out_base);
	red_out_base = NULL;
	release_region(RED_OUT_BASE, 2);
	printk(KERN_INFO "LCD POC driver done\n");
}

module_init(lcd_init);
module_exit(lcd_cleanup);

