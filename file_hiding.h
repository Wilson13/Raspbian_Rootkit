#ifndef FILE_HIDING_H
#define FILE_HIDING_H
#endif

#include <linux/dirent.h>	// For getdents64

#include "hack_pi.h"
#include "syscall_table.h"

BROOTUS_MODULE(file_hiding);    // Macros for init_file_hiding

extern void set_file_prefix(char* prefix);

// Define linux_dirent as in fs/readdir.c (line 135)
// (needed for getdents syscall)
struct linux_dirent
{
	unsigned long d_ino;
	unsigned long d_off;
	unsigned short d_reclen;
	char d_name[1];
};
