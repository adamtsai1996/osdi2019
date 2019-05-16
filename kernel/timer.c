/* Reference: http://www.osdever.net/bkerndev/Docs/pit.htm */
#include <kernel/trap.h>
#include <kernel/picirq.h>
#include <kernel/task.h>
#include <kernel/cpu.h>
#include <inc/mmu.h>
#include <inc/x86.h>

#define TIME_HZ 100

static unsigned long jiffies = 0;

void set_timer(int hz) {
	int divisor = 1193180 / hz;   /* Calculate our divisor */
	outb(0x43, 0x36);             /* Set our command byte 0x36 */
	outb(0x40, divisor & 0xFF);   /* Set low byte of divisor */
	outb(0x40, divisor >> 8);     /* Set high byte of divisor */
}

void timer_handler(struct Trapframe *tf) {
	extern void sched_yield();
	int i;

	jiffies++;

	extern Task tasks[];

	lapic_eoi();
	for( i=0; i<thiscpu->cpu_rq.nr; i++) {
		Task *task = &tasks[thiscpu->cpu_rq.runq[i]];
		if( task->state==TASK_SLEEP ) {
			task->remind_ticks--;
			if( task->remind_ticks<=0 )
				task->state = TASK_RUNNABLE;
		}
	}

	if ( thiscpu->cpu_task!=NULL ) {
		if( thiscpu->cpu_task->state==TASK_RUNNING ) {
			thiscpu->cpu_task->remind_ticks--;
			if( thiscpu->cpu_task->remind_ticks<=0 ) {
				thiscpu->cpu_task->state = TASK_RUNNABLE;
				sched_yield();
			}
		}
	}
}

unsigned long sys_get_ticks() {
	return jiffies;
}

void timer_init() {
	set_timer(TIME_HZ);

	/* Enable interrupt */
	irq_setmask_8259A(irq_mask_8259A & ~(1<<IRQ_TIMER));

	/* Register trap handler */
	extern void TIM_ISR();
	register_handler( IRQ_OFFSET + IRQ_TIMER, &timer_handler, &TIM_ISR, 0, 0);
}

