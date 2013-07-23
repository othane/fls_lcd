
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

static void __iomem *e_dir_base = NULL;
static void __iomem *e_out_base = NULL;
#define E_BIT (1<<8)


static void set_e_dir(int dir)
{
	unsigned short val = ioread16(e_dir_base);
	
	printk(KERN_INFO "set e dir\n");

	// dbg - print old state
	if (val & E_BIT)
		printk(KERN_INFO "e dir is output\n");
	else
		printk(KERN_INFO "e dir is input\n");
	
	// set dir
	if (dir)
		val |= E_BIT;
	else
		val &= ~E_BIT;
	iowrite16(val, e_dir_base);
}

static void set_e(int e)
{
	unsigned short val = ioread16(e_out_base);
	
	printk(KERN_INFO "set e\n");

	// dbg - print old state
	if (val & E_BIT)
		printk(KERN_INFO "e is hi\n");
	else
		printk(KERN_INFO "e is lo\n");
	
	// set dir
	if (e)
		val |= E_BIT;
	else
		val &= ~E_BIT;
	iowrite16(val, e_out_base);
}

int lcd_init(void)
{
	int ret = 0;

	// dbg start up msg
	printk(KERN_INFO "LCD POC driver started\n");

	// grab control over the memory for the io pins
	printk(KERN_INFO "requesting E dir region\n");
	if (!request_region(E_DIR_BASE, 2, "lcd"))
	{
		printk(KERN_ERR "%s: requested I/O region (%#x:2) is "
			   "in use\n", "lcd, E_DIR_BASE", E_DIR_BASE);
		ret = -ENODEV;
		goto out1;
	}
	printk(KERN_INFO "requesting E out region\n");
	if (!request_region(E_OUT_BASE, 2, "lcd"))
	{
		printk(KERN_ERR "%s: requested I/O region (%#x:2) is "
			   "in use\n", "lcd, E_OUT_BASE", E_OUT_BASE);
		ret = -ENODEV;
		goto out2;
	}

	printk(KERN_INFO "ioremap E dir region\n");
	e_dir_base = ioremap_nocache(E_DIR_BASE, 2);
	if (!e_dir_base)
	{
		printk(KERN_ERR "%s: map I/O region (%#x:2) is "
			   "in use\n", "lcd, E_DIR_BASE", E_DIR_BASE);
		ret = -EIO;
		goto out3;
	}
	printk(KERN_INFO "ioremap E dir region from 0x%.8x to 0x%.8x\n", E_DIR_BASE, (int)e_dir_base);
	
	printk(KERN_INFO "ioremap E out region\n");
	e_out_base = ioremap_nocache(E_OUT_BASE, 2);
	if (!e_dir_base)
	{
		printk(KERN_ERR "%s: map I/O region (%#x:2) is "
			   "in use\n", "lcd, E_OUT_BASE", E_OUT_BASE);
		ret = -EIO;
		goto out3;
	}
	printk(KERN_INFO "ioremap E out region from 0x%.8x to 0x%.8x\n", E_OUT_BASE, (int)e_out_base);

	// set e pin to output mode hi
	set_e_dir(1);
	set_e(1);
	
	return 0;

	// bail out section
out3:
	printk(KERN_INFO "bailing out !\n");
	if (e_out_base)
		ioport_unmap(e_out_base);
	e_out_base = NULL;
	if (e_dir_base)
		ioport_unmap(e_dir_base);
	e_dir_base= NULL;
	release_region(E_OUT_BASE, 2);
out2:
	release_region(E_DIR_BASE, 2);
out1:
	return ret;
}

void lcd_cleanup(void)
{
	// reset e
	set_e(0);

	// clear resources
	printk(KERN_INFO "freeing resources\n");
	if (e_out_base)
		ioport_unmap(e_out_base);
	e_out_base = NULL;
	if (e_dir_base)
		ioport_unmap(e_dir_base);
	e_dir_base = NULL;
	release_region(E_OUT_BASE, 2);
	release_region(E_DIR_BASE, 2);
	printk(KERN_INFO "LCD POC driver done\n");
}

#ifdef MODULE
module_init(lcd_init);
#else
early_initcall(lcd_init);
#endif
module_exit(lcd_cleanup);

