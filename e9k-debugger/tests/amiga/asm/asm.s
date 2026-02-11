#NO_APP
	.text
	.align	2
	.globl	_printf
_printf:
	lea (-20,sp),sp
	lea (28,sp),a0
	move.l a0,(sp)
	move.l (sp),d0
	move.l d0,-(sp)
	move.l (28,sp),-(sp)
	pea _buf.4328
	jsr _vsprintf
	lea (12,sp),sp
	move.l d0,(8,sp)
	move.l #16515072,(4,sp)
	move.l #_buf.4328,(16,sp)
	clr.l (12,sp)
	jra .L2
.L3:
	move.l (16,sp),a0
	move.b (a0),d0
	addq.l #1,a0
	move.l a0,(16,sp)
	move.l (4,sp),a0
	move.b d0,(a0)
	addq.l #1,(12,sp)
.L2:
	move.l (12,sp),d0
	cmp.l (8,sp),d0
	jlt .L3
	move.l (8,sp),d0
	lea (20,sp),sp
	rts
	.globl	example
	.data
	.align	2
example:
	.long	-559038737
	.long	-267530225
	.text
.LC0:
	.ascii "function: %d %08x\12\0"
	.align	2
	.globl	function
function:
	move.l 4+example,d0
	move.l d0,-(sp)
	move.l (8,sp),-(sp)
	pea .LC0
	jsr _printf
	lea (12,sp),sp
	move.l example,d0
	addq.l #1,d0
	move.l d0,example
	move.l 4+example,d0
	addq.l #1,d0
	move.l d0,4+example
	moveq #1,d0
	rts
	.align	2
	.globl	funtimes
funtimes:
	subq.l #8,sp
	move.l (12,sp),(4,sp)
	move.l (16,sp),(sp)
	move.l (4,sp),d0
	move.l d0,-(sp)
	jsr function
	addq.l #4,sp
	move.l (sp),d0
	move.l d0,-(sp)
	jsr function
	addq.l #4,sp
	move.l (sp),d0
	addq.l #8,sp
	rts
.LC1:
	.ascii "ASM TEST\12\0"
.LC2:
	.ascii "AllocMem failed\12\0"
.LC3:
	.ascii "AllocMem allocated!\12\0"
.LC4:
	.ascii "fun = %d\12\0"
	.align	2
	.globl	_main
_main:
	lea (-40,sp),sp
	move.l a6,-(sp)
	moveq #1,d0
	move.l d0,(20,sp)
	move.l (20,sp),d0
	tst.l d0
	jeq .L10
	moveq #2,d0
	move.l d0,(20,sp)
.L10:
	pea .LC1
	jsr _printf
	addq.l #4,sp
	moveq #100,d0
	move.l d0,(40,sp)
	clr.l (36,sp)
	move.l _SysBase,d0
	move.l d0,a6
	move.l (40,sp),d0
	move.l (36,sp),d1
#APP
| 71 "locals.c" 1
	jsr a6@(-0xc6:W)
| 0 "" 2
#NO_APP
	move.l d0,(32,sp)
	move.l (32,sp),d0
	move.l d0,(16,sp)
	tst.l d0
	jne .L11
	pea .LC2
	jsr _printf
	addq.l #4,sp
	jra .L12
.L11:
	pea .LC3
	jsr _printf
	addq.l #4,sp
.L12:
	move.l (16,sp),d0
	tst.l d0
	jeq .L13
	move.l (16,sp),(28,sp)
	moveq #100,d0
	move.l d0,(24,sp)
	move.l _SysBase,d0
	move.l d0,a6
	move.l (28,sp),a1
	move.l (24,sp),d0
#APP
| 78 "locals.c" 1
	jsr a6@(-0xd2:W)
| 0 "" 2
#NO_APP
.L13:
	moveq #1,d0
	move.l d0,(12,sp)
	moveq #2,d0
	move.l d0,(8,sp)
.L14:
	move.l (8,sp),d1
	move.l (12,sp),d0
	move.l d1,-(sp)
	move.l d0,-(sp)
	jsr funtimes
	addq.l #8,sp
	move.l d0,(4,sp)
	move.l (4,sp),d0
	move.l d0,-(sp)
	pea .LC4
	jsr _printf
	addq.l #8,sp
	jra .L14
.lcomm _buf.4328,255
