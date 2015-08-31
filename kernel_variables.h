//extern void** get_sys_call_table_addr(void);

#ifndef KERNEL_VARIABLES_H
#define KERNEL_VARIABLES_H

#define KVAR(type, name) type name = (type) rk_##name
	extern struct list_head* modules;
	extern struct proc_dir_entry* proc_root;
	extern void** get_sys_call_table_addr(void);
#endif
