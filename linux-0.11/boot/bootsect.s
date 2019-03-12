	.code16
# rewrite with AT&T syntax by falcon <wuzhangjin@gmail.com> at 081012
#
# SYS_SIZE is the number of clicks (16 bytes) to be loaded.
# 0x3000 is 0x30000 bytes = 196kB, more than enough for current
# versions of linux
#
	.equ SYSSIZE, 0x3000
#
#	bootsect.s		(C) 1991 Linus Torvalds
#
# bootsect.s is loaded at 0x7c00 by the bios-startup routines, and moves
# iself out of the way to address 0x90000, and jumps there.
#
# It then loads 'setup' directly after itself (0x90200), and the system
# at 0x10000, using BIOS interrupts. 
#
# NOTE! currently system is at most 8*65536 bytes long. This should be no
# problem, even in the future. I want to keep it simple. This 512 kB
# kernel size should be enough, especially as this doesn't contain the
# buffer cache as in minix
#
# The loader has been made as simple as possible, and continuos
# read errors will result in a unbreakable loop. Reboot by hand. It
# loads pretty fast by getting whole sectors at a time whenever possible.

	.global _start, begtext, begdata, begbss, endtext, enddata, endbss
	.text
	begtext:
	.data
	begdata:
	.bss
	begbss:

	.text	
	.equ HELLOLEN, 1
	.equ HELLOSEG, 0x0100
	.equ SETUPLEN, 4		# nr of setup-sectors
	.equ BOOTSEG, 0x07c0		# original address of boot-sector
	.equ INITSEG, 0x9000		# we move boot here - out of the way
	.equ SETUPSEG, 0x9020		# setup starts here
	.equ SYSSEG, 0x1000		# system loaded at 0x10000 (65536).
	.equ ENDSEG, SYSSEG + SYSSIZE	# where to stop loading
	
# ROOT_DEV:	0x000 - same type of floppy as boot.
#		0x301 - /dev/hd1 first partition on first drive etc
#
#		0x305 - /dev/hd5 second device
#		0x306 - /dev/hd6 first partition on second ...
# old dev name rule in linux 
	.equ ROOT_DEV, 0x301
	ljmp    $BOOTSEG, $_start	# jump to 0x07c00:$_start
_start:
	mov	$BOOTSEG, %ax
	mov	%ax, %ds
	mov	$INITSEG, %ax
	mov	%ax, %es
	mov	$256, %cx
	sub	%si, %si	# DS:SI = 0x07c00:0x0000
	sub	%di, %di	# ES:DI = 0x90000:0x0000
	rep	
	movsw			# mov DS:SI to ES:DI 
	ljmp	$INITSEG, $go	# jump to 0x90000:$go => also set the new CS to INITSEG(0x90000)
go:	mov	%cs, %ax
	mov	%ax, %ds
	mov	%ax, %es
	mov	%ax, %ss
	mov	$0xFF00, %sp		# arbitrary value >>512
# let ds = cs = ss = es = 0x90000
# and put stack at 0x90000:0xFF000

####### multi-boot #######
multiboot:
	mov	$0x03, %ah
	xor	%bh, %bh
	int	$0x10
	# AH=0x03, read cursor pos
	# BH page number
	# on return:
	# CH cursor starting scan line (low order 5 bits)
	# CL cursor ending scan line (low order 5 bits)
	# DH row
	# DL column
	
	mov	$54, %cx
	mov	$0x0007, %bx		# page 0, attribute 7 (normal)
	mov     $multiboot_msg, %bp
	mov	$0x1301, %ax		# write string, move cursor
	int	$0x10
	# AH=0x13, write string
	# AL=0xXY, write mode
	# X set to use color attribute
	# Y set to update cursor
	# BH, page number
	# BL, color attributes
	# (hight 4 bits for bg, low 4 bits for fg, search BIOS Color Attribute)
	# CX, string length
	# DH, row
	# DL, column
	# ES:BP, pointer to string

read_char:	
	sub	%ah, %ah
	int	$0x16
	# AH=0x00, read key press
	# return:
	# AH scancode
	# AL ASCII code

	cmp	$0x31, %al
	je	load_setup
	cmp	$0x32, %al
	je	load_hello	
	jmp	read_char
	
	
####### load_hello #######
load_hello:

# TODO
	mov	$0x0000, %dx
	mov	$0x0006, %cx
	mov	$HELLOSEG, %ax
	mov	%ax, %es
	sub	%bx, %bx		# buffer is ES:BX = 0x1000:0x00
	.equ	AX, 0x0200+HELLOLEN
	mov 	$AX, %ax
	int	$0x13
	# 0x13 the disk servic
	# AH = 0x02 Read Sectors
	# AL number of sectors
	# ES:BX pointer to buffer
	# CH Cylinder
	# CL Sector
	# DH Head
	# DL Drive
	# CF is	0 if successful
	#	1 if error

	jnc	ok_load_hello
	mov	$0x0000, %dx
	mov	$0x0000, %ax
	int	$0x13
	jmp	load_hello

ok_load_hello:
	mov	$INITSEG, %ax
	mov	$multiboot, %bx
	ljmp	$HELLOSEG, $0	# Jump to hello
	
####### load_hello #######

# load the setup-sectors directly after the bootblock.
# Note that 'es' is already set up.

load_setup:
	
	mov	$0x0000, %dx		# drive 0, head 0
	mov	$0x0002, %cx		# sector 3, track 0
	mov	$0x0200, %bx		# address = 512, in INITSEG
	.equ    AX, 0x0200+SETUPLEN	# .equ is like define in C
	mov     $AX, %ax		# service 2, nr of sectors
	int	$0x13			# read it
	# 0x13 the disk servic
	# ax = AX = AH:AL = 02:04
	# AH = 02, Read Disk Sectors
	# AL = 04, number of sectors
	# ES:BX = 0x90000:0x00200, pointer to buffer
	# CF is	0 if successful
	#	1 if error
	
	jnc	ok_load_setup		# if CF!=1 jump to ok_load_setup
	mov	$0x0000, %dx
	mov	$0x0000, %ax		# reset the diskette
	int	$0x13			# AH = 00, reset disk system
	jmp	load_setup

ok_load_setup:

# Get disk drive parameters, specifically nr of sectors/track

	mov	$0x00, %dl		# driver_nr, 0=A, 1=2nd floppy, 80h=drive 0, 81h=drive 1
	mov	$0x0800, %ax		# AH=8 is get drive parameters
	int	$0x13
	# AH=08, get drive parameters
	# on return:
	# AH = status (define in INT13, STATUS)
	# BL the type of driver
	# CH the last 8 bits of max cylinder/track number
	# CL the max sector number per cylinder/track
	# DH slide number
	# DL number of drives attached
	# ES:DI store the disk base table (DBT)
	mov	$0x00, %ch
	#seg cs
	mov	%cx, %cs:sectors+0	# %cs means sectors is in %cs, let CX:sectors be the number of sectors per track
	mov	$INITSEG, %ax
	mov	%ax, %es

# Print some inane message

	mov	$0x03, %ah		# read cursor pos
	xor	%bh, %bh
	int	$0x10
	# AH=0x03, read cursor pos
	# BH page number
	# on return:
	# CH cursor starting scan line (low order 5 bits)
	# CL cursor ending scan line (low order 5 bits)
	# DH row
	# DL column

	mov	$24, %cx
	mov	$0x0007, %bx		# page 0, attribute 7 (normal)
	#lea	msg1, %bp
	mov     $msg1, %bp
	mov	$0x1301, %ax		# write string, move cursor
	int	$0x10
	# AH=0x13, write string
	# AL=0xXY, write mode
	# X is 1 if the string contains attribtes (char1 attribute1 char2 attribute2...)
	# Y is 1 if cursor moves
	# BH, page number
	# BL, attributes (if AL==00H or 01H)
	# CX, string length
	# DH, row
	# DL, column
	# ES:BP, pointer to string

# ok, we've written the message, now
# we want to load the system (at 0x10000)

	mov	$SYSSEG, %ax
	mov	%ax, %es		# segment of 0x010000
	call	read_it
	call	kill_motor

# After that we check which root-device to use. If the device is
# defined (#= 0), nothing is done and the given device is used.
# Otherwise, either /dev/PS0 (2,28) or /dev/at0 (2,8), depending
# on the number of sectors that the BIOS reports currently.

	#seg cs
	mov	%cs:root_dev+0, %ax
	cmp	$0, %ax
	jne	root_defined
	#seg cs
	mov	%cs:sectors+0, %bx
	mov	$0x0208, %ax		# /dev/ps0 - 1.2Mb
	cmp	$15, %bx
	je	root_defined
	mov	$0x021c, %ax		# /dev/PS0 - 1.44Mb
	cmp	$18, %bx
	je	root_defined
undef_root:
	jmp undef_root
root_defined:
	#seg cs
	mov	%ax, %cs:root_dev+0

# after that (everyting loaded), we jump to
# the setup-routine loaded directly after
# the bootblock:

	ljmp	$SETUPSEG, $0

# This routine loads the system at address 0x10000, making sure
# no 64kB boundaries are crossed. We try to load it as fast as
# possible, loading whole tracks whenever we can.
#
# in:	es - starting address segment (normally 0x1000)
#
sread:	.word 1 + HELLOLEN + SETUPLEN	# sectors read of current track
head:	.word 0				# current head
track:	.word 0				# current track

read_it:
	mov	%es, %ax
	test	$0x0fff, %ax		# test 64KB boundary (0x10000)
die:	jne 	die			# es must be at 64kB boundary
	xor 	%bx, %bx		# clear bx, bx is starting address within segment
rp_read:
	mov 	%es, %ax
 	cmp 	$ENDSEG, %ax		# have we loaded all yet?
	jb	ok1_read
	ret
ok1_read:
	#seg cs
	mov	%cs:sectors+0, %ax	# AX = sector number per track
	sub	sread, %ax		# let AX be the number of remaining sectors at this segment
	mov	%ax, %cx
	shl	$9, %cx			# CX, remain sector * 512 bytes = remain bytes
	add	%bx, %cx		# current allocation + remaining sectors
	jnc 	ok2_read		# not overflow
	je 	ok2_read		# is 0 => 0x10000 is 0 in 16 bits
	xor 	%ax, %ax
	sub 	%bx, %ax		# AX = 0 - BX = 65536 - BX, remaining space in this segment
	shr 	$9, %ax			# AX/=512, remaining sector in this segment
ok2_read:
	call 	read_track
	mov 	%ax, %cx
	add 	sread, %ax
	#seg cs
	cmp 	%cs:sectors+0, %ax
	jne 	ok3_read
	mov 	$1, %ax
	sub 	head, %ax
	jne 	ok4_read
	incw    track 
ok4_read:
	mov	%ax, head
	xor	%ax, %ax
ok3_read:
	mov	%ax, sread
	shl	$9, %cx
	add	%cx, %bx
	jnc	rp_read
	mov	%es, %ax
	add	$0x1000, %ax
	mov	%ax, %es
	xor	%bx, %bx
	jmp	rp_read

read_track:
	push	%ax
	push	%bx
	push	%cx
	push	%dx
	mov	track, %dx
	mov	sread, %cx
	inc	%cx
	mov	%dl, %ch
	mov	head, %dx
	mov	%dl, %dh
	mov	$0, %dl
	and	$0x0100, %dx
	mov	$2, %ah
	int	$0x13
	jc	bad_rt
	pop	%dx
	pop	%cx
	pop	%bx
	pop	%ax
	ret
bad_rt:	mov	$0, %ax
	mov	$0, %dx
	int	$0x13
	pop	%dx
	pop	%cx
	pop	%bx
	pop	%ax
	jmp	read_track

#/*
# * This procedure turns off the floppy drive motor, so
# * that we enter the kernel in a known state, and
# * don't have to worry about it later.
# */
kill_motor:
	push	%dx
	mov	$0x3f2, %dx
	mov	$0, %al
	outsb
	pop	%dx
	ret





sectors:
	.word 0

# length = 24
msg1:
	.byte 13,10
	.ascii "Loading system ..."
	.byte 13,10,13,10

# length = 8 + 5 + 19 + 22 = 54
multiboot_msg:
	.byte 13,10
	.ascii "press"
	.byte 13,10
	.ascii "1) linux-0.11 Image"
	.byte 13,10
	.ascii "2) hello world program"
	.byte 13,10



.org 508
root_dev:
	.word ROOT_DEV
boot_flag:
	.word 0xAA55
	
	.text
	endtext:
	.data
	enddata:
	.bss
	endbss:


