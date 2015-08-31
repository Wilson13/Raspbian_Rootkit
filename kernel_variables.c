#include <linux/kernel.h>

#include "kernel_variables.h"

ssize_t *sys_call_table = (ssize_t *)NULL;

void** get_sys_call_table_addr(void)
{
	void *swi_addr=(long *)0xffff0008;
	unsigned long offset=0;
	unsigned long *vector_swi_addr=0;
	//unsigned long sys_call_table=0;

	offset=((*(long *)swi_addr)&0xfff)+8;
	vector_swi_addr=*(unsigned long *)(swi_addr+offset);

	while(vector_swi_addr++){
		if(((*(unsigned long *)vector_swi_addr)&
		0xfffff000)==0xe28f8000){
			offset=((*(unsigned long *)vector_swi_addr)&
			0xfff)+8;
			sys_call_table=(void *)vector_swi_addr+offset;
			break;
		}
	}

	//printk("\nGot sys_call_table: %p\n", sys_call_table);
	return (void **) sys_call_table;
}
