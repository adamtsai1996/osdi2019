	.code16
	.global _start
	.text
_start:
	mov	%cs, %ax
	mov 	%ax, %ds
    	mov 	%ax, %es
 
    	mov 	$0x03, %ah      # read cursor pos
    	xor 	%bh, %bh
    	int 	$0x10
     	# AH=0x03, read cursor pos
	# BH page number
	# on return:
	# CH cursor starting scan line (low order 5 bits)
	# CL cursor ending line (low order 5 bits)
	# DH row
	# DL column
	
    	mov 	$20, %cx
    	mov 	$0x008b, %bx
    	mov     $msg1, %bp
    	mov 	$0x1311, %ax
    	int 	$0x10
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
	
end_hello:
   	ljmp	end_hello
 
msg1:
    	.byte 	13,10
    	.ascii 	"Hello OSDI Lab2!"
    	.byte 	13,10

