// defines the data strucures used by the kernel to manage processes.

#ifndef PROC_H
#define PROC_H

#include "types.h"

#define NPROC (16)

// 1.) safer upper limit for array size, we need to ensure that the size of TaskInfo struct is 
// reasonable and does not cause memory issues. The actual number can be adjusted based on 
// the expected number of syscalls and memory constraints.
#define MAX_SYSCALL_NUM 500

// Saved registers for kernel context switches.
struct context {
	uint64 ra;
	uint64 sp;

	// callee-saved
	uint64 s0;
	uint64 s1;
	uint64 s2;
	uint64 s3;
	uint64 s4;
	uint64 s5;
	uint64 s6;
	uint64 s7;
	uint64 s8;
	uint64 s9;
	uint64 s10;
	uint64 s11;
};

enum procstate { UNUSED, USED, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };

// define 
typedef enum {
	UnInit,
	Ready,
	Running,
	Exited,
} TaskStatus;

// 1) contract between the user and the kernel
// kernel will fill in the fields with requested information
struct TaskInfo{
	TaskStatus status;
	unsigned int syscall_times[MAX_SYSCALL_NUM];
	int time;
} ;

// 1) Every process in the Kernel needs itw own memory
struct proc {
	enum procstate state; // Process state
	int pid; // Process ID
	uint64 ustack; // Virtual address of user stack
	uint64 kstack; // Virtual address of kernel stack
	struct trapframe *trapframe; // data page for trampoline.S
	struct context context; // swtch() here to run process
	/*
	* LAB1: you may need to add some new fields here
	*/
	// array to store syscall times for each syscall
	unsigned int syscall_times[MAX_SYSCALL_NUM]; 
	uint64 start_time; // process exact CPU Start-time

	//TaskInfo info; // Process information
	
};


/*
* LAB1: you may need to define struct for TaskInfo here
*/

struct proc *curr_proc();
void exit(int);
void proc_init();
void scheduler() __attribute__((noreturn));
void sched();
void yield();
struct proc *allocproc();
// swtch.S
void swtch(struct context *, struct context *);

#endif // PROC_H