#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/shell.h>
#include <inc/timer.h>

struct Command {
	const char *name;
	const char *desc;
	// return -1 to force monitor to exit
	int (*func)(int argc, char** argv);
};

static struct Command commands[] = {
	{ "help", "Display this list of commands", mon_help },
	{ "kerninfo", "Display information about the kernel", mon_kerninfo },
	{ "print_tick", "Display system tick", print_tick },
	{ "chgcolor", "Change the screen text color", chg_color},
};
#define NCOMMANDS (sizeof(commands)/sizeof(commands[0]))


int mon_help(int argc, char **argv)
{
	int i;

	for (i = 0; i < NCOMMANDS; i++)
		cprintf("%s - %s\n", commands[i].name, commands[i].desc);
	return 0;
}

int mon_kerninfo(int argc, char **argv)
{
	/* TODO: Print the kernel code and data section size 
   	* NOTE: You can count only linker script (kernel/kern.ld) to
   	*       provide you with those information.
   	*       Use PROVIDE inside linker script and calculate the
   	*       offset.
   	*/
	
	extern uint32_t kernel_load_addr[], etext[], data_seg[], end[];
	uint32_t _text_start = (uint32_t)kernel_load_addr;
	uint32_t _text_end = (uint32_t)etext;
	uint32_t _data_start = (uint32_t)data_seg;
	uint32_t _end = (uint32_t)end;
	cprintf("Kernel code base start=0x%08x size = %u\n", _text_start, _text_end - _text_start );
	cprintf("Kernel data base start=0x%08x size = %u\n", _data_start, _end - _data_start );
	cprintf("Kernel executable memory footprint: %uKB\n", (_end - _text_start)>>10 );

	return 0;
}

int print_tick(int argc, char **argv)
{
	cprintf("Now tick = %d\n", get_tick());
	return 0;
}

int chg_color(int argc, char **argv)
{
	if(argc<2) {
		cprintf("No input text color!\n");
		return 0;
	}
	uint8_t bg = 0x00;
	uint8_t fg = argv[1][0]-'0';
	if(argv[1][0]>='A' && argv[1][0]<='F') fg = 10 + argv[1][0]-'A';
	else if(argv[1][0]>='a' && argv[1][0]<='f') fg = 10 + argv[1][0]-'a';
	settextcolor(fg,bg);
	cprintf("chgcolor %u!\n",fg);
	return 0;
}

#define WHITESPACE "\t\r\n "
#define MAXARGS 16

static int runcmd(char *buf)
{
	int argc;
	char *argv[MAXARGS];
	int i;

	// Parse the command buffer into whitespace-separated arguments
	argc = 0;
	argv[argc] = 0;
	while (1) {
		// gobble whitespace
		while (*buf && strchr(WHITESPACE, *buf))
			*buf++ = 0;
		if (*buf == 0)
			break;

		// save and scan past next arg
		if (argc == MAXARGS-1) {
			cprintf("Too many arguments (max %d)\n", MAXARGS);
			return 0;
		}
		argv[argc++] = buf;
		while (*buf && !strchr(WHITESPACE, *buf))
			buf++;
	}
	argv[argc] = 0;

	// Lookup and invoke the command
	if (argc == 0)
		return 0;
	for (i = 0; i < NCOMMANDS; i++) {
		if (strcmp(argv[0], commands[i].name) == 0)
			return commands[i].func(argc, argv);
	}
	cprintf("Unknown command '%s'\n", argv[0]);
	return 0;
}
void shell()
{
	char *buf;
	cprintf("Welcome to the OSDI course!\n");
	cprintf("Type 'help' for a list of commands.\n");
	
	while(1)
	{
		buf = readline("OSDI> ");
		if (buf != NULL)
		{
			if (runcmd(buf) < 0)
				break;
		}
	}
}
