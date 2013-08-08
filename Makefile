obj-m += fls_lcd.o

all:
	make -C $(KPATH) M=$(PWD) modules
	$(CROSS_COMPILE)gcc -g $(CFLAGS) lcd_unit_test.c -o lcd_unit_test

clean:
	make -C $(KPATH) M=$(PWD) clean
	rm -rf lcd_unit_test
