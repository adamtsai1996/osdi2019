#include <kernel/task.h>
#include <kernel/timer.h>
#include <kernel/mem.h>
#include <kernel/syscall.h>
#include <kernel/trap.h>
#include <inc/stdio.h>

void do_puts(char *str, uint32_t len)
{
	uint32_t i;
	for (i = 0; i < len; i++)
	{
		k_putch(str[i]);
	}
}

int32_t do_getc()
{
	return k_getc();
}

int32_t do_syscall(uint32_t syscallno, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5)
{
	int32_t retVal = -1;
	extern Task *cur_task;

	extern int sys_fork();
	extern void sched_yield();
	extern void sys_kill(int);
	extern void sys_settextcolor(unsigned char, unsigned char);
	extern void sys_cls();

	switch (syscallno)
	{
		case SYS_fork:
			/* TODO: Lab 5
			 * You can reference kernel/task.c, kernel/task.h
			 */
			retVal = sys_fork();
			break;

		case SYS_getc:
			retVal = do_getc();
			break;

		case SYS_puts:
			do_puts((char*)a1, a2);
			retVal = 0;
			break;

		case SYS_getpid:
			/* TODO: Lab 5
			 * Get current task's pid
			 */
			retVal = cur_task->task_id;
			break;

		case SYS_sleep:
			/* TODO: Lab 5
			 * Yield this task
			 * You can reference kernel/sched.c for yielding the task
			 */
			cur_task->remind_ticks = a1;
			cur_task->state = TASK_SLEEP;
			sched_yield();
			retVal = 0;
			break;

		case SYS_kill:
			/* TODO: Lab 5
			 * Kill specific task
			 * You can reference kernel/task.c, kernel/task.h
			 */
			sys_kill(cur_task->task_id);
			retVal=0;
			break;

		case SYS_get_num_free_page:
			/* TODO: Lab 5
			 * You can reference kernel/mem.c
			 */
			retVal = sys_get_num_free_page();
			break;

		case SYS_get_num_used_page:
			/* TODO: Lab 5
			 * You can reference kernel/mem.c
			 */
			retVal = sys_get_num_used_page();
			break;

		case SYS_get_ticks:
			/* TODO: Lab 5
			 * You can reference kernel/timer.c
			 */
			retVal = sys_get_ticks();
			break;

		case SYS_settextcolor:
			/* TODO: Lab 5
			 * You can reference kernel/screen.c
			 */
			sys_settextcolor(a1,a2);
			retVal=0;
			break;

		case SYS_cls:
			/* TODO: Lab 5
			 * You can reference kernel/screen.c
			 */
			sys_cls();
			retVal=0;
			break;

	}
	return retVal;
}

static void syscall_handler(struct Trapframe *tf)
{
	/* TODO: Lab5
   * call do_syscall
   * Please remember to fill in the return value
   * HINT: You have to know where to put the return value
   */
	struct PushRegs *regs = &tf->tf_regs;
	regs->reg_eax = do_syscall(
			regs->reg_eax, regs->reg_edx, regs->reg_ecx,
			regs->reg_ebx, regs->reg_edi, regs->reg_esi);
	
}

void syscall_init()
{
  /* TODO: Lab5
   * Please set gate of system call into IDT
   * You can leverage the API register_handler in kernel/trap.c
   */
	extern void SYSCALL();
	register_handler(T_SYSCALL, &syscall_handler, SYSCALL, 1, 3);
}

