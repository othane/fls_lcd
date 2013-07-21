obj-m += lcd.o

all:
	make -C /home/othane/Workspace/ts4x00/linux-2.6.29 M=$(PWD) modules

clean:
	make -C /home/othane/Workspace/ts4x00/linux-2.6.29 M=$(PWD) clean
