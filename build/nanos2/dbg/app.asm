
build/nanos2/bin/app.elf:     file format elf32-littlearm


Disassembly of section .text:

c0de0000 <main>:
c0de0000:	b5f0      	push	{r4, r5, r6, r7, lr}
c0de0002:	b099      	sub	sp, #100	; 0x64
c0de0004:	ac0d      	add	r4, sp, #52	; 0x34
c0de0006:	4605      	mov	r5, r0
c0de0008:	4620      	mov	r0, r4
c0de000a:	f000 f8af 	bl	c0de016c <setjmp>
c0de000e:	f8ad 0060 	strh.w	r0, [sp, #96]	; 0x60
c0de0012:	0400      	lsls	r0, r0, #16
c0de0014:	d130      	bne.n	c0de0078 <main+0x78>
c0de0016:	a80d      	add	r0, sp, #52	; 0x34
c0de0018:	f000 f88a 	bl	c0de0130 <try_context_set>
c0de001c:	466e      	mov	r6, sp
c0de001e:	9017      	str	r0, [sp, #92]	; 0x5c
c0de0020:	211b      	movs	r1, #27
c0de0022:	1d70      	adds	r0, r6, #5
c0de0024:	f000 f892 	bl	c0de014c <__aeabi_memclr>
c0de0028:	481b      	ldr	r0, [pc, #108]	; (c0de0098 <main+0x98>)
c0de002a:	2700      	movs	r7, #0
c0de002c:	9705      	str	r7, [sp, #20]
c0de002e:	f88d 7004 	strb.w	r7, [sp, #4]
c0de0032:	9004      	str	r0, [sp, #16]
c0de0034:	4819      	ldr	r0, [pc, #100]	; (c0de009c <main+0x9c>)
c0de0036:	9000      	str	r0, [sp, #0]
c0de0038:	2034      	movs	r0, #52	; 0x34
c0de003a:	f88d 0018 	strb.w	r0, [sp, #24]
c0de003e:	f000 f831 	bl	c0de00a4 <check_api_level>
c0de0042:	2001      	movs	r0, #1
c0de0044:	e9cd 670b 	strd	r6, r7, [sp, #44]	; 0x2c
c0de0048:	900a      	str	r0, [sp, #40]	; 0x28
c0de004a:	f44f 7080 	mov.w	r0, #256	; 0x100
c0de004e:	9009      	str	r0, [sp, #36]	; 0x24
c0de0050:	4913      	ldr	r1, [pc, #76]	; (c0de00a0 <main+0xa0>)
c0de0052:	4479      	add	r1, pc
c0de0054:	9108      	str	r1, [sp, #32]
c0de0056:	b15d      	cbz	r5, c0de0070 <main+0x70>
c0de0058:	6868      	ldr	r0, [r5, #4]
c0de005a:	68e9      	ldr	r1, [r5, #12]
c0de005c:	900a      	str	r0, [sp, #40]	; 0x28
c0de005e:	910c      	str	r1, [sp, #48]	; 0x30
c0de0060:	a808      	add	r0, sp, #32
c0de0062:	f000 f83f 	bl	c0de00e4 <os_lib_call>
c0de0066:	9809      	ldr	r0, [sp, #36]	; 0x24
c0de0068:	6028      	str	r0, [r5, #0]
c0de006a:	f000 f845 	bl	c0de00f8 <os_lib_end>
c0de006e:	e003      	b.n	c0de0078 <main+0x78>
c0de0070:	9009      	str	r0, [sp, #36]	; 0x24
c0de0072:	a808      	add	r0, sp, #32
c0de0074:	f000 f836 	bl	c0de00e4 <os_lib_call>
c0de0078:	f000 f852 	bl	c0de0120 <try_context_get>
c0de007c:	42a0      	cmp	r0, r4
c0de007e:	d102      	bne.n	c0de0086 <main+0x86>
c0de0080:	9817      	ldr	r0, [sp, #92]	; 0x5c
c0de0082:	f000 f855 	bl	c0de0130 <try_context_set>
c0de0086:	f8bd 0060 	ldrh.w	r0, [sp, #96]	; 0x60
c0de008a:	b910      	cbnz	r0, c0de0092 <main+0x92>
c0de008c:	2000      	movs	r0, #0
c0de008e:	b019      	add	sp, #100	; 0x64
c0de0090:	bdf0      	pop	{r4, r5, r6, r7, pc}
c0de0092:	f000 f810 	bl	c0de00b6 <os_longjmp>
c0de0096:	bf00      	nop
c0de0098:	0e9ac0ce 	.word	0x0e9ac0ce
c0de009c:	4e4f454e 	.word	0x4e4f454e
c0de00a0:	00000132 	.word	0x00000132

c0de00a4 <check_api_level>:
c0de00a4:	b580      	push	{r7, lr}
c0de00a6:	f000 f813 	bl	c0de00d0 <get_api_level>
c0de00aa:	280d      	cmp	r0, #13
c0de00ac:	bf38      	it	cc
c0de00ae:	bd80      	popcc	{r7, pc}
c0de00b0:	20ff      	movs	r0, #255	; 0xff
c0de00b2:	f000 f829 	bl	c0de0108 <os_sched_exit>

c0de00b6 <os_longjmp>:
c0de00b6:	4604      	mov	r4, r0
c0de00b8:	f000 f832 	bl	c0de0120 <try_context_get>
c0de00bc:	4621      	mov	r1, r4
c0de00be:	f000 f85b 	bl	c0de0178 <longjmp>

c0de00c2 <SVC_Call>:
c0de00c2:	df01      	svc	1
c0de00c4:	2900      	cmp	r1, #0
c0de00c6:	d100      	bne.n	c0de00ca <exception>
c0de00c8:	4770      	bx	lr

c0de00ca <exception>:
c0de00ca:	4608      	mov	r0, r1
c0de00cc:	f7ff fff3 	bl	c0de00b6 <os_longjmp>

c0de00d0 <get_api_level>:
c0de00d0:	b5e0      	push	{r5, r6, r7, lr}
c0de00d2:	2000      	movs	r0, #0
c0de00d4:	4669      	mov	r1, sp
c0de00d6:	e9cd 0000 	strd	r0, r0, [sp]
c0de00da:	2001      	movs	r0, #1
c0de00dc:	f7ff fff1 	bl	c0de00c2 <SVC_Call>
c0de00e0:	bd8c      	pop	{r2, r3, r7, pc}
	...

c0de00e4 <os_lib_call>:
c0de00e4:	b5e0      	push	{r5, r6, r7, lr}
c0de00e6:	f000 f82d 	bl	c0de0144 <OUTLINED_FUNCTION_0>
c0de00ea:	4802      	ldr	r0, [pc, #8]	; (c0de00f4 <os_lib_call+0x10>)
c0de00ec:	4669      	mov	r1, sp
c0de00ee:	f7ff ffe8 	bl	c0de00c2 <SVC_Call>
c0de00f2:	bd8c      	pop	{r2, r3, r7, pc}
c0de00f4:	01000067 	.word	0x01000067

c0de00f8 <os_lib_end>:
c0de00f8:	b5e0      	push	{r5, r6, r7, lr}
c0de00fa:	2000      	movs	r0, #0
c0de00fc:	4669      	mov	r1, sp
c0de00fe:	9001      	str	r0, [sp, #4]
c0de0100:	2068      	movs	r0, #104	; 0x68
c0de0102:	f7ff ffde 	bl	c0de00c2 <SVC_Call>
c0de0106:	bd8c      	pop	{r2, r3, r7, pc}

c0de0108 <os_sched_exit>:
c0de0108:	b082      	sub	sp, #8
c0de010a:	f000 f81b 	bl	c0de0144 <OUTLINED_FUNCTION_0>
c0de010e:	4803      	ldr	r0, [pc, #12]	; (c0de011c <os_sched_exit+0x14>)
c0de0110:	4669      	mov	r1, sp
c0de0112:	f7ff ffd6 	bl	c0de00c2 <SVC_Call>
c0de0116:	deff      	udf	#255	; 0xff
c0de0118:	e7fe      	b.n	c0de0118 <os_sched_exit+0x10>
c0de011a:	bf00      	nop
c0de011c:	0100009a 	.word	0x0100009a

c0de0120 <try_context_get>:
c0de0120:	b5e0      	push	{r5, r6, r7, lr}
c0de0122:	2000      	movs	r0, #0
c0de0124:	4669      	mov	r1, sp
c0de0126:	9001      	str	r0, [sp, #4]
c0de0128:	2087      	movs	r0, #135	; 0x87
c0de012a:	f7ff ffca 	bl	c0de00c2 <SVC_Call>
c0de012e:	bd8c      	pop	{r2, r3, r7, pc}

c0de0130 <try_context_set>:
c0de0130:	b5e0      	push	{r5, r6, r7, lr}
c0de0132:	f000 f807 	bl	c0de0144 <OUTLINED_FUNCTION_0>
c0de0136:	4802      	ldr	r0, [pc, #8]	; (c0de0140 <try_context_set+0x10>)
c0de0138:	4669      	mov	r1, sp
c0de013a:	f7ff ffc2 	bl	c0de00c2 <SVC_Call>
c0de013e:	bd8c      	pop	{r2, r3, r7, pc}
c0de0140:	0100010b 	.word	0x0100010b

c0de0144 <OUTLINED_FUNCTION_0>:
c0de0144:	2100      	movs	r1, #0
c0de0146:	e9cd 0100 	strd	r0, r1, [sp]
c0de014a:	4770      	bx	lr

c0de014c <__aeabi_memclr>:
c0de014c:	2200      	movs	r2, #0
c0de014e:	f000 b800 	b.w	c0de0152 <__aeabi_memset>

c0de0152 <__aeabi_memset>:
c0de0152:	4613      	mov	r3, r2
c0de0154:	460a      	mov	r2, r1
c0de0156:	4619      	mov	r1, r3
c0de0158:	f000 b800 	b.w	c0de015c <memset>

c0de015c <memset>:
c0de015c:	4402      	add	r2, r0
c0de015e:	4603      	mov	r3, r0
c0de0160:	4293      	cmp	r3, r2
c0de0162:	d100      	bne.n	c0de0166 <memset+0xa>
c0de0164:	4770      	bx	lr
c0de0166:	f803 1b01 	strb.w	r1, [r3], #1
c0de016a:	e7f9      	b.n	c0de0160 <memset+0x4>

c0de016c <setjmp>:
c0de016c:	46ec      	mov	ip, sp
c0de016e:	e8a0 5ff0 	stmia.w	r0!, {r4, r5, r6, r7, r8, r9, sl, fp, ip, lr}
c0de0172:	f04f 0000 	mov.w	r0, #0
c0de0176:	4770      	bx	lr

c0de0178 <longjmp>:
c0de0178:	e8b0 5ff0 	ldmia.w	r0!, {r4, r5, r6, r7, r8, r9, sl, fp, ip, lr}
c0de017c:	46e5      	mov	sp, ip
c0de017e:	0008      	movs	r0, r1
c0de0180:	bf08      	it	eq
c0de0182:	2001      	moveq	r0, #1
c0de0184:	4770      	bx	lr
c0de0186:	bf00      	nop
c0de0188:	7445      	strb	r5, [r0, #17]
c0de018a:	6568      	str	r0, [r5, #84]	; 0x54
c0de018c:	6572      	str	r2, [r6, #84]	; 0x54
c0de018e:	6d75      	ldr	r5, [r6, #84]	; 0x54
c0de0190:	0000      	movs	r0, r0
	...

c0de0194 <_etext>:
	...
