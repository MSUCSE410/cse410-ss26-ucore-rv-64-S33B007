#include "syscall.h"
#include "defs.h"
#include "loader.h"
#include "syscall_ids.h"
#include "timer.h"
#include "trap.h"
#include "proc.h"

uint64 sys_write(int fd, uint64 va, uint len)
{
	debugf("sys_write fd = %d va = %x, len = %d", fd, va, len);
	if (fd != STDOUT)
		return -1;
	struct proc *p = curr_proc();
	char str[MAX_STR_LEN];
	int size = copyinstr(p->pagetable, str, va, MIN(len, MAX_STR_LEN));
	debugf("size = %d", size);
	for (int i = 0; i < size; ++i) {
		console_putchar(str[i]);
	}
	return size;
}

__attribute__((noreturn)) void sys_exit(int code)
{
	exit(code);
	__builtin_unreachable();
}

uint64 sys_sched_yield()
{
	yield();
	return 0;
}

uint64 sys_gettimeofday(TimeVal *val, int _tz) // TODO: implement sys_gettimeofday in pagetable. (VA to PA)
{

	/* The code in `ch3` will leads to memory bugs*/
	// ch4: translate user virtual address to kernel physical address via useraddr
	// 1) get the current process
	struct proc *p = curr_proc();

	// 2) translate user VA to kernel PA
	TimeVal *kval = (TimeVal *)useraddr(p->pagetable, (uint64)val);

	// 3) check if the translation was successful. If useraddr returns 0, it means the address is invalid or not mapped, and we should return -1 to indicate an error.
	if (kval == 0)
		return -1;

	// 4) we can safely write to translated pointer
	// convert cpu cycle count to seconds and microseconds
	uint64 cycle = get_cycle();
	kval->sec = cycle / CPU_FREQ;
	kval->usec = (cycle % CPU_FREQ) * 1000000 / CPU_FREQ;
	return 0;
}

// TODO: add support for mmap and munmap syscall.
// hint: read through docstrings in vm.c. Watching CH4 video may also help.
// Note the return value and PTE flags (especially U,X,W,R)
/*
* LAB1: you may need to define sys_task_info here
*/
// same issue as sys_gettimeofday, we need to translate user VA to kernel PA before writing to user's struct
uint64 sys_task_info(struct TaskInfo *info)
{
	struct proc *p = curr_proc();
	// 2) Translate user VA to kernel PA so we ca write to user's struct
	struct TaskInfo *kinfo = (struct TaskInfo *)useraddr(p->pagetable, (uint64)info);
	if (kinfo == 0)
		return -1;
	kinfo->status = 2; // Running - task is currently executing this syscall
	// 4) Copy per-syscall counts from the process tracking array
	for (int i = 0; i < MAX_SYSCALL_NUM; i++) {
		kinfo->syscall_times[i] = p->syscall_times[i];
	}
	// Elapsed wall-clock time in ms since first scheduled
	kinfo->time = (int)((get_cycle() - p->start_time) / (CPU_FREQ / 1000));
	return 0;
}

// 1.) Syscall ID 222: map anonymous physical memory to user virtual address space
uint64 sys_mmap(uint64 start, uint64 len, int port, int flag, int fd)
{
	if (len == 0)
		return 0;
	// Error checks: page-aligned, not too large, valid permission bits
	if (start % PGSIZE != 0)
		return -1;
	if (len > 1024 * 1024 * 1024)
		return -1;
	if ((port & ~0x7) != 0) // bits beyond the lowest 3 should not be set
		return -1;
	if ((port & 0x7) == 0)  // at least one of R/W/X required
		return -1;

	struct proc *p = curr_proc();
	uint64 end = start + len;

	// Check that none of the pages are already mapped
	for (uint64 a = start; a < end; a += PGSIZE) {
		if (walkaddr(p->pagetable, a) != 0)
			return -1;
	}

	// Convert port bits to RISC-V PTE flags (PTE_U always set for user access)
	int perm = PTE_U;
	if (port & 0x1) perm |= PTE_R;
	if (port & 0x2) perm |= PTE_W;
	if (port & 0x4) perm |= PTE_X;

	// Allocate a physical page for each VA, zero it, and create the PTE
	for (uint64 a = start; a < end; a += PGSIZE) {
		void *pa = kalloc();
		if (pa == 0)
			return -1;
		memset(pa, 0, PGSIZE);
		if (mappages(p->pagetable, a, PGSIZE, (uint64)pa, perm) != 0) {
			kfree(pa);
			return -1;
		}
	}

	// Track the highest mapped page for cleanup in uvmfree
	uint64 new_max = PGROUNDUP(end) / PGSIZE;
	if (new_max > p->max_page)
		p->max_page = new_max;

	return 0;
}

// 2.) Syscall ID 215: unmap a block of virtual memory
uint64 sys_munmap(uint64 start, uint64 len)
{
	if (start % PGSIZE != 0)
		return -1;
	if (len == 0)
		return 0;

	struct proc *p = curr_proc();
	uint64 va0 = start;
	uint64 va_end = PGROUNDUP(start + len);

	// Check that all pages in the range are mapped
	for (uint64 a = va0; a < va_end; a += PGSIZE) {
		if (walkaddr(p->pagetable, a) == 0)
			return -1;
	}

	// Remove PTEs and free the backing physical pages (do_free=1)
	uint64 npages = (va_end - va0) / PGSIZE;
	uvmunmap(p->pagetable, va0, npages, 1);

	return 0;
}

extern char trap_page[];

void syscall()
{
	struct trapframe *trapframe = curr_proc()->trapframe;
	int id = trapframe->a7, ret;
	uint64 args[6] = { trapframe->a0, trapframe->a1, trapframe->a2,
			   trapframe->a3, trapframe->a4, trapframe->a5 };
	tracef("syscall %d args = [%x, %x, %x, %x, %x, %x]", id, args[0],
	       args[1], args[2], args[3], args[4], args[5]);
	/*
	* LAB1: you may need to update syscall counter for task info here
	*/
	// count the syscall times for the current process, if id is valid
	struct proc *p = curr_proc();
	if (id >= 0 && id < MAX_SYSCALL_NUM) {
		p->syscall_times[id]++;
	}
	
	switch (id) {
	case SYS_write:
		ret = sys_write(args[0], args[1], args[2]);
		break;
	case SYS_exit:
		sys_exit(args[0]);
		// __builtin_unreachable();
	case SYS_sched_yield:
		ret = sys_sched_yield();
		break;
	case SYS_gettimeofday:
		ret = sys_gettimeofday((TimeVal *)args[0], args[1]);
		break;
	/*
	* LAB1: you may need to add SYS_taskinfo case here
	*/
	case SYS_task_info:
		ret = sys_task_info((struct TaskInfo *)args[0]);
		break;
	case SYS_mmap:
		ret = sys_mmap(args[0], args[1], args[2], args[3], args[4]);
		break;
	case SYS_munmap:
		ret = sys_munmap(args[0], args[1]);
		break;
	default:
		ret = -1;
		errorf("unknown syscall %d", id);
	}
	trapframe->a0 = ret;
	tracef("syscall ret %d", ret);
}
