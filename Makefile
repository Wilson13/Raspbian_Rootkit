obj-m += rootkit.o

rootkit-objs += main.o
rootkit-objs += file_hiding.o
rootkit-objs += open_hooking.o
rootkit-objs += read_hooking.o
rootkit-objs += send_UDP.o
rootkit-objs += write_hooking.o
rootkit-objs += gpio_hijacking.o
rootkit-objs += module_hiding.o
rootkit-objs += syscall_table.o
rootkit-objs += kernel_variables.o

all:
	make ARCH=arm CROSS_COMPILE=/usr/bin/arm-linux-gnueabi- -C /home/rpi/raspberrypi/linux M=$(PWD) modules

clean:
	make -C /home/rpi/raspberrypi/linux M=$(PWD) clean
