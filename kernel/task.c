#include <inc/mmu.h>
#include <inc/types.h>
#include <inc/string.h>
#include <inc/x86.h>
#include <inc/memlayout.h>
#include <kernel/task.h>
#include <kernel/mem.h>
#include <kernel/cpu.h>
#include <kernel/spinlock.h>
// Global descriptor table.
//
// Set up global descriptor table (GDT) with separate segments for
// kernel mode and user mode.  Segments serve many purposes on the x86.
// We don't use any of their memory-mapping capabilities, but we need
// them to switch privilege levels. 
//
// The kernel and user segments are identical except for the DPL.
// To load the SS register, the CPL must equal the DPL.  Thus,
// we must duplicate the segments for the user and the kernel.
//
// In particular, the last argument to the SEG macro used in the
// definition of gdt specifies the Descriptor Privilege Level (DPL)
// of that descriptor: 0 for kernel and 3 for user.
//
struct Segdesc gdt[NCPU + 5] =
{
	// 0x0 - unused (always faults -- for trapping NULL far pointers)
	SEG_NULL,

	// 0x8 - kernel code segment
	[GD_KT >> 3] = SEG(STA_X | STA_R, 0x0, 0xffffffff, 0),

	// 0x10 - kernel data segment
	[GD_KD >> 3] = SEG(STA_W, 0x0, 0xffffffff, 0),

	// 0x18 - user code segment
	[GD_UT >> 3] = SEG(STA_X | STA_R, 0x0, 0xffffffff, 3),

	// 0x20 - user data segment
	[GD_UD >> 3] = SEG(STA_W , 0x0, 0xffffffff, 3),

	// First TSS descriptors (starting from GD_TSS0) are initialized
	// in task_init()
	[GD_TSS0 >> 3] = SEG_NULL
	
};

struct Pseudodesc gdt_pd = {
	sizeof(gdt) - 1, (unsigned long) gdt
};



//static struct tss_struct tss;
Task tasks[NR_TASKS];

extern char bootstack[];

extern char UTEXT_start[], UTEXT_end[];
extern char UDATA_start[], UDATA_end[];
extern char UBSS_start[], UBSS_end[];
extern char URODATA_start[], URODATA_end[];
/* Initialized by task_init */
uint32_t UTEXT_SZ;
uint32_t UDATA_SZ;
uint32_t UBSS_SZ;
uint32_t URODATA_SZ;

struct spinlock task_lock;

Task *cur_task = NULL; //Current running task

extern void sched_yield(void);

int task_create() {
	Task *ts = NULL;	
	spin_lock(&task_lock);

	/* Find a free task structure */
	int i,new_pid=0;
	for( i=0; i<NR_TASKS; i++){
		if( tasks[i].state==TASK_FREE){
			ts = &tasks[i];
			new_pid = i;
			break;
		}
	}
	if(!ts) return -1;

	/* Setup Page Directory and pages for kernel*/
	if (!(ts->pgdir = setupkvm()))
		panic("Not enough memory for per process page directory!\n");

	/* Setup User Stack */
	uint32_t va;
	for( va=USTACKTOP-USR_STACK_SIZE ; va<USTACKTOP ; va+=PGSIZE ){
		struct PageInfo *pg=page_alloc(ALLOC_ZERO);
		if( !pg ) panic("task_create():page_alloc() failed");
		if( page_insert(ts->pgdir, pg, (void*)va, PTE_U|PTE_W)!=0)
			panic("task_creat():page_insert failed");
	}

	/* Setup Trapframe */
	memset( &(ts->tf), 0, sizeof(ts->tf));
	ts->tf.tf_cs = GD_UT | 0x03;
	ts->tf.tf_ds = GD_UD | 0x03;
	ts->tf.tf_es = GD_UD | 0x03;
	ts->tf.tf_ss = GD_UD | 0x03;
	ts->tf.tf_esp = USTACKTOP-PGSIZE;

	/* Setup task structure (task_id and parent_id) */
	ts->task_id = new_pid;
	ts->parent_id = 0;
	if(thiscpu->cpu_task) ts->parent_id = thiscpu->cpu_task->task_id;
	ts->state = TASK_RUNNABLE;
	ts->remind_ticks = TIME_QUANT;
	
	spin_unlock(&task_lock);
	return new_pid;
}

static void task_free(int pid) {
	pte_t *usr_pgdir = tasks[pid].pgdir;
	uintptr_t va;

	lcr3(PADDR(kern_pgdir));
	
	for( va=USTACKTOP-USR_STACK_SIZE ; va<USTACKTOP ; va+=PGSIZE )
		page_remove(usr_pgdir, (void *)va);

	ptable_remove(usr_pgdir);
	pgdir_remove(usr_pgdir);
}

// Lab6 TODO
//
// Modify it so that the task will be removed form cpu runqueue
// ( we not implement signal yet so do not try to kill process
// running on other cpu )
//
void sys_kill(int pid) {
	if ( pid>0 && pid<NR_TASKS ) {
		if( thiscpu->cpu_id==tasks[pid].cpu_id ){
			
			spin_lock(&task_lock);
			
			// remove from run queue
			Runqueue *rq = &thiscpu->cpu_rq;
			int index = tasks[pid].rq_index;
			if( --rq->nr!=index ){
				rq->runq[index] = rq->runq[rq->nr];
				tasks[rq->runq[index]].rq_index = index;
			}

			// remove from task
			tasks[pid].state = TASK_FREE;
			task_free(pid);
			
			spin_unlock(&task_lock);
			
			if( thiscpu->cpu_task->task_id == pid){
				thiscpu->cpu_task = &tasks[rq->runq[index-1]];
				sched_yield();
			}
		}
	}
}

// Lab6 TODO:
//
// Modify it so that the task will disptach to different cpu runqueue
// (please try to load balance, don't put all task into one cpu)
//
int sys_fork()
{
	/* pid for newly created process */
	int pid = task_create();
	pte_t *pgdir = tasks[pid].pgdir;

	if( pid<0 ) return -1;

	if ( (uint32_t)thiscpu->cpu_task ){
		tasks[pid].tf = thiscpu->cpu_task->tf;

		uint32_t va;
		for( va=USTACKTOP-USR_STACK_SIZE ; va<USTACKTOP ; va+=PGSIZE ){	
			pte_t *child_pte = pgdir_walk(pgdir, (void*)va, 0);
			pte_t *parent_pte = pgdir_walk(thiscpu->cpu_task->pgdir, (void*)va, 0);
			void *child_kva = KADDR(PTE_ADDR(*child_pte));
			void *parent_kva = KADDR(PTE_ADDR(*parent_pte)); 
			memcpy( child_kva, parent_kva, PGSIZE);
		}

		setupvm(pgdir, (uint32_t)UTEXT_start, UTEXT_SZ);
		setupvm(pgdir, (uint32_t)UDATA_start, UDATA_SZ);
		setupvm(pgdir, (uint32_t)UBSS_start, UBSS_SZ);
		setupvm(pgdir, (uint32_t)URODATA_start, URODATA_SZ);
		thiscpu->cpu_task->tf.tf_regs.reg_eax = pid;
		tasks[pid].tf.tf_regs.reg_eax = 0;
	}

	/* dispatch to the minimum loading cpu */
	spin_lock(&task_lock);
	int i;
	int min_cpu = 0;
	int min_load = cpus[0].cpu_rq.nr;
	for( i=1; i<ncpu; i++){
		if( cpus[i].cpu_rq.nr>=min_load ) continue;
		min_cpu = i;
		min_load = cpus[i].cpu_rq.nr;
	}
	tasks[pid].cpu_id = min_cpu;
	tasks[pid].rq_index = min_load;
	cpus[min_cpu].cpu_rq.runq[min_load] = pid;
	cpus[min_cpu].cpu_rq.nr++;
	spin_unlock(&task_lock);
	
	return pid;
}

void task_init()
{
	extern int user_entry();
	int i;
	UTEXT_SZ = (uint32_t)(UTEXT_end - UTEXT_start);
	UDATA_SZ = (uint32_t)(UDATA_end - UDATA_start);
	UBSS_SZ = (uint32_t)(UBSS_end - UBSS_start);
	URODATA_SZ = (uint32_t)(URODATA_end - URODATA_start);

	spin_initlock(&task_lock);

	for (i = 0; i < NR_TASKS; i++)
	{
		memset(&(tasks[i]), 0, sizeof(Task));
		tasks[i].state = TASK_FREE;
	}
	task_init_percpu();
}

// Lab6 TODO
//
// Please modify this function to:
//
// 1. init idle task for non-booting AP 
//    (remember to put the task in cpu runqueue) 
//
// 2. init per-CPU Runqueue
//
// 3. init per-CPU system registers
//
// 4. init per-CPU TSS
//
void task_init_percpu(){
	int i;
	extern int user_entry();
	extern int idle_entry();
	
	// Setup a TSS so that we get the right stack
	// when we trap to the kernel.
	memset( &thiscpu->cpu_tss, 0, sizeof(thiscpu->cpu_tss));
	thiscpu->cpu_tss.ts_esp0 = (uint32_t)percpu_kstacks[thiscpu->cpu_id] + KSTKSIZE;
	thiscpu->cpu_tss.ts_ss0 = GD_KD;

	// fs and gs stay in user data segment
	thiscpu->cpu_tss.ts_fs = GD_UD|0x03;
	thiscpu->cpu_tss.ts_gs = GD_UD|0x03;

	/* Setup TSS in GDT */
	gdt[ (GD_TSS0>>3) + thiscpu->cpu_id ] = 
		SEG16( STS_T32A, (uint32_t)&thiscpu->cpu_tss, sizeof(struct tss_struct), 0);
	gdt[ (GD_TSS0>>3) + thiscpu->cpu_id ].sd_s = 0;

	/* Setup first task */
	i = task_create();
	thiscpu->cpu_task = &(tasks[i]);

	/* For user program */
	setupvm( thiscpu->cpu_task->pgdir, (uint32_t)UTEXT_start, UTEXT_SZ);
	setupvm( thiscpu->cpu_task->pgdir, (uint32_t)UDATA_start, UDATA_SZ);
	setupvm( thiscpu->cpu_task->pgdir, (uint32_t)UBSS_start, UBSS_SZ);
	setupvm( thiscpu->cpu_task->pgdir, (uint32_t)URODATA_start, URODATA_SZ);
	if(thiscpu==bootcpu) thiscpu->cpu_task->tf.tf_eip = (uint32_t)user_entry;
	else thiscpu->cpu_task->tf.tf_eip = (uint32_t)idle_entry;

	/* Init cpu run queue */
	thiscpu->cpu_rq.runq[0] = i;
	thiscpu->cpu_rq.cur = 0;
	thiscpu->cpu_rq.nr = 1;


    /* Load GDT&LDT */
	lgdt( &gdt_pd );

	lldt( 0 );

	// Load the TSS selector 
	ltr( GD_TSS0 + (thiscpu->cpu_id<<3) );

	thiscpu->cpu_task->state = TASK_RUNNING;	
}
