/*  
 * FLS front panel lcd driver
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/ioport.h>
#include <linux/delay.h>
#include <asm/io.h>

#define MODULE_NAME "FLS front panel LCD"

#define SYSCON_BASE (0x80004000)
#define RS	(1 << 6)
#define RW	(1 << 7)
#define E	(1 << 8)
#define D4	(1 << 0)
#define D5	(1 << 1)
#define D6	(1 << 4)
#define D7	(1 << 5)

// timings from datasheet (plus tm to add a little margin so we are safe)
#define Tpor0     (50)
#define Tpor1     (5)
#define Tc        (500)
#define Tpw       (230)
#define Tr        (20)
#define Tf        (20)
#define Tsp1      (40)
#define Tsp2      (80)
#define Td        (120)
#define Tm        (50)

struct dio_reg_t {
	unsigned long paddr;
	size_t size;
	struct resource *res;
	void __iomem *vaddr;
};

static struct dio_t {
	struct dio_reg_t dir;	
	struct dio_reg_t in;	
	struct dio_reg_t out;	
} lcd_dio = {
	.dir = {.paddr = SYSCON_BASE + 0x1e, .size = 2},
	.in  = {.paddr = SYSCON_BASE + 0x26, .size = 2},
	.out = {.paddr = SYSCON_BASE + 0x16, .size = 2},
};

enum lcd_display {
	lcd_display_off = 0x00,
	lcd_display_on = 0x04,
};

enum lcd_cursor {
	lcd_cursor_off = 0x00,
	lcd_cursor_on = 0x02,
};

enum lcd_blink {
	lcd_blink_off = 0x00,
	lcd_blink_on = 0x01,
};

static struct lcd_t {
	struct dio_t *dio;
	int pos;
	enum lcd_display display_state;
	enum lcd_cursor cursor_state;
	enum lcd_blink blink_state;
} lcd = {
	.dio = &lcd_dio,
};


static void dio_set(struct dio_t *dio, unsigned int set_mask, unsigned int clear_mask)
{
	unsigned int dir, out;
	unsigned long flags;
	unsigned int output_mask = set_mask | clear_mask;

	// make these operations seemly atomic (at least
	// on our single core system)
	local_irq_save(flags);

	// set and clear output state
	out = ioread16(dio->out.vaddr);
	out |= set_mask;
	out &= ~clear_mask;
	iowrite16(out, dio->out.vaddr);

	// ensure these pins are outputs (if already inputs they will
	// all switch together, if some were inputs and some where
	// outputs there might be a slight glitch between some pins
	// and if all were outputs this step does nothing effectively
	dir = ioread16(dio->dir.vaddr);
	dir |= output_mask; // 1 = output, 0 = input
	iowrite16(dir, dio->dir.vaddr);

	local_irq_restore(flags);
}

static unsigned int dio_get(struct dio_t *dio, unsigned int get_mask)
{
	unsigned int dir, in;
	unsigned long flags;

	// make these operations seemly atomic (at least
	// on our single core system)
	local_irq_save(flags);

	// ensure these pins are inputs
	dir = ioread16(dio->dir.vaddr);
	dir &= ~get_mask; // 1 = output, 0 = input
	iowrite16(dir, dio->dir.vaddr);

	// set and clear output state
	in = ioread16(dio->in.vaddr);
	in &= get_mask;
	
	local_irq_restore(flags);
	return in;
}

static int dio_init(struct dio_t *dio)
{
	// sanity check (can remove after devel phase)
	if (!dio) {
		printk(KERN_ERR "invalid parameter\n");
		return -EFAULT;
	}

	// request dir, in, out regions
	dio->dir.res = request_region(dio->dir.paddr, dio->dir.size, MODULE_NAME);
	if (!dio->dir.res) {
		printk(KERN_ERR "requested io region (%.8lx) is in use\n", dio->dir.paddr);
		return -EBUSY;
	}
	dio->in.res = request_region(dio->in.paddr, dio->in.size, MODULE_NAME);
	if (!dio->in.res) {
		printk(KERN_ERR "requested io region (%.8lx) is in use\n", dio->in.paddr);
		return -EBUSY;
	}
	dio->out.res = request_region(dio->out.paddr, dio->out.size, MODULE_NAME);
	if (!dio->out.res) {
		printk(KERN_ERR "requested io region (%.8lx) is in use\n", dio->out.paddr);
		return -EBUSY;
	}
	
	// get virtual address of dir, in, out from phys address so we may use them
	dio->dir.vaddr = ioremap_nocache(dio->dir.paddr, dio->dir.size);
	if (!dio->dir.vaddr) {
		printk(KERN_ERR "unable to remap io region (%.8lx)\n", dio->dir.paddr);
		return -EFAULT;
	}
	dio->in.vaddr = ioremap_nocache(dio->in.paddr, dio->in.size);
	if (!dio->in.vaddr) {
		printk(KERN_ERR "unable to remap io region (%.8lx)\n", dio->in.paddr);
		return -EFAULT;
	}
	dio->out.vaddr = ioremap_nocache(dio->out.paddr, dio->out.size);
	if (!dio->out.vaddr) {
		printk(KERN_ERR "unable to remap io region (%.8lx)\n", dio->out.paddr);
		return -EFAULT;
	}
	
	return 0;
}

static void dio_deinit(struct dio_t *dio)
{
	// unmap virtual addresses of dir, in, out
	if (dio->dir.vaddr)
		iounmap(dio->dir.vaddr);
	dio->dir.vaddr = NULL;
	if (dio->in.vaddr)
		iounmap(dio->in.vaddr);
	dio->in.vaddr = NULL;
	if (dio->out.vaddr)
		iounmap(dio->out.vaddr);
	dio->out.vaddr = NULL;

	// release memory regions
	if (dio->dir.res)
		release_region(dio->dir.paddr, dio->dir.size);
	dio->dir.res = NULL;
	if (dio->in.res)
		release_region(dio->in.paddr, dio->in.size);
	dio->in.res = NULL;
	if (dio->out.res)
		release_region(dio->out.paddr, dio->out.size);
	dio->out.res = NULL;
}

#define cond_to_dio_masks(cond, set, clear, bit) {if (cond) set |= bit; else clear |= bit;}
static void lcd_write4(struct lcd_t *lcd, uint8_t rs, uint8_t db)
{
	unsigned int set = 0;
	unsigned int clear = 0;

	// set rw = 0 (write), and rs
	cond_to_dio_masks(rs, set, clear, RS);
	dio_set(lcd->dio, set, clear | RW);

	// wait for >= tsp1
	ndelay(Tsp1 - Tr + Tm);

	// set e hi
	dio_set(lcd->dio, E, 0);
	ndelay(Tr + Tm);

	// hold e hi for >= tpw - tsp2
	ndelay(Tpw - Tsp2 + Tm);

	// set/clear db
	set = 0;
	clear = 0;
	cond_to_dio_masks((db & (1 << 4)), set, clear, D4);
	cond_to_dio_masks((db & (1 << 5)), set, clear, D5);
	cond_to_dio_masks((db & (1 << 6)), set, clear, D6);
	cond_to_dio_masks((db & (1 << 7)), set, clear, D7);
	dio_set(lcd->dio, set, clear);
	
	// hack for TS8500: even though u10 is powered off it still adds a lot of capacitance
	// the d5 line, this takes 5us to die away so we add a 10us delay here to handle that
	// on the real fls this should not be needed as there is no u10
	udelay(10);
	
	// hold db and enable for >= tps2
	ndelay(Tsp2 + Tm);

	// set e lo
	dio_set(lcd->dio, 0, E);
	ndelay(Tf + Tm);

	// wait for >= thd1 + tf
	ndelay(Tc - Tr - Tpw - Tf + Tm);
}

static uint8_t lcd_read4(struct lcd_t *lcd, uint8_t rs)
{
	uint8_t db = 0, tmp;
	unsigned int set = 0;
	unsigned int clear = 0;

	// set rw = 1 (read), and rs
	cond_to_dio_masks(rs, set, clear, RS);
	dio_set(lcd->dio, set | RW, clear);

	// wait for >= tsp1
	ndelay(Tsp1 - Tr + Tm);

	// set e hi
	dio_set(lcd->dio, E, 0);
	ndelay(Tr + Tm);

	// hold e hi for >= tpw - tsp2
	ndelay(Td - Tr + Tm);

	// hack for TS8500: even though u10 is powered off it still adds a lot of capacitance
	// the d5 line, this takes 5us to die away so we add a 10us delay here to handle that
	// on the real fls this should not be needed as there is no u10
	udelay(10);

	// set/clear db
	tmp = dio_get(lcd->dio, D4 | D5 | D6 | D7);
	db |= tmp & D4 ? (1 << 4): 0;
	db |= tmp & D5 ? (1 << 5): 0;
	db |= tmp & D6 ? (1 << 6): 0;
	db |= tmp & D7 ? (1 << 7): 0;
	
	// hold db and enable for >= tps2
	ndelay(Tpw + Tr - Td + Tm);
	
	// set e lo
	dio_set(lcd->dio, 0, E);
	ndelay(Tf + Tm);

	// wait for >= thd1 + tf
	ndelay(Tc - Tr - Tpw - Tf + Tm);

	return db;
}

int lcd_init(void)
{
	int ret = 0;

	// start up msg
	printk(KERN_INFO "FLS LCD driver started\n");

	// init the registers etc
	ret = dio_init(lcd.dio);
	if (ret < 0) {
		printk(KERN_ERR "lcd module unable to init dio, bailing out\n");
		goto fail;
	}

	// set RS, RW, and E as lo outputs (these remain outputs throughout lcd operation)
	dio_set(lcd.dio, 0, (RS | RW | E));

	return 0;

fail:
	// deinit registers etc
	dio_deinit(lcd.dio);
	return ret;
}

void lcd_cleanup(void)
{
	// deinit registers etc
	dio_deinit(lcd.dio);

	// shutdown msg
	printk(KERN_INFO "FLS LCD driver done\n");
}

#ifdef MODULE
module_init(lcd_init);
#else
early_initcall(lcd_init);
#endif
module_exit(lcd_cleanup);

