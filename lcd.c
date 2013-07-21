
/*  
 * proof of concept lcd driver
 */
#include <linux/module.h>
#include <linux/kernel.h>

int lcd_init(void)
{
	// dbg start up msg
	printk(KERN_INFO "LCD POC driver started\n");

	return 0;
}

void lcd_cleanup(void)
{
	printk(KERN_INFO "LCD POC driver done\n");
}

module_init(lcd_init);
module_exit(lcd_cleanup);

