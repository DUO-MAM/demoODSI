/*
    FreeRTOS V8.2.3 - Copyright (C) 2015 Real Time Engineers Ltd.
    All rights reserved

    VISIT http://www.FreeRTOS.org TO ENSURE YOU ARE USING THE LATEST VERSION.

    This file is part of the FreeRTOS distribution.

    FreeRTOS is free software; you can redistribute it and/or modify it under
    the terms of the GNU General Public License (version 2) as published by the
    Free Software Foundation >>>> AND MODIFIED BY <<<< the FreeRTOS exception.

    ***************************************************************************
    >>!   NOTE: The modification to the GPL is included to allow you to     !<<
    >>!   distribute a combined work that includes FreeRTOS without being   !<<
    >>!   obliged to provide the source code for proprietary components     !<<
    >>!   outside of the FreeRTOS kernel.                                   !<<
    ***************************************************************************

    FreeRTOS is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE.  Full license text is available on the following
    link: http://www.freertos.org/a00114.html

    ***************************************************************************
     *                                                                       *
     *    FreeRTOS provides completely free yet professionally developed,    *
     *    robust, strictly quality controlled, supported, and cross          *
     *    platform software that is more than just the market leader, it     *
     *    is the industry's de facto standard.                               *
     *                                                                       *
     *    Help yourself get started quickly while simultaneously helping     *
     *    to support the FreeRTOS project by purchasing a FreeRTOS           *
     *    tutorial book, reference manual, or both:                          *
     *    http://www.FreeRTOS.org/Documentation                              *
     *                                                                       *
    ***************************************************************************

    http://www.FreeRTOS.org/FAQHelp.html - Having a problem?  Start by reading
    the FAQ page "My application does not run, what could be wrong?".  Have you
    defined configASSERT()?

    http://www.FreeRTOS.org/support - In return for receiving this top quality
    embedded software for free we request you assist our global community by
    participating in the support forum.

    http://www.FreeRTOS.org/training - Investing in training allows your team to
    be as productive as possible as early as possible.  Now you can receive
    FreeRTOS training directly from Richard Barry, CEO of Real Time Engineers
    Ltd, and the world's leading authority on the world's leading RTOS.

    http://www.FreeRTOS.org/plus - A selection of FreeRTOS ecosystem products,
    including FreeRTOS+Trace - an indispensable productivity tool, a DOS
    compatible FAT file system, and our tiny thread aware UDP/IP stack.

    http://www.FreeRTOS.org/labs - Where new FreeRTOS products go to incubate.
    Come and try FreeRTOS+TCP, our new open source TCP/IP stack for FreeRTOS.

    http://www.OpenRTOS.com - Real Time Engineers ltd. license FreeRTOS to High
    Integrity Systems ltd. to sell under the OpenRTOS brand.  Low cost OpenRTOS
    licenses offer ticketed support, indemnification and commercial middleware.

    http://www.SafeRTOS.com - High Integrity Systems also provide a safety
    engineered and independently SIL3 certified version for use in safety and
    mission critical applications that require provable dependability.

    1 tab == 4 spaces!
*/
#define __ASSEMBLY__
#include <pip/vidt.h>

.file "portASM.S"
#include "FreeRTOSConfig.h"
#include "ISR_Support.h"

	.extern pxCurrentTCB
	.extern vTaskSwitchContext
	.extern vPortCentralInterruptHandler
	.extern xTaskIncrementTick
	.extern vPortAPICErrorHandler
	.extern pucPortTaskFPUContextBuffer
	.extern ulPortYieldPending

	.global vPortStartFirstTask
	.global vPortCentralInterruptWrapper
	.global vPortAPICErrorHandlerWrapper
	.global vPortTimerHandler
	.global vPortYieldCall
	.global vPortAPICSpuriousHandler
    .extern viret
    .extern xTaskSwitchToProtectedTask

	.text

/*-----------------------------------------------------------*/


.macro enableInterrupt
	call Pip_VSTI
.endm
.macro disableInterrupt
	call Pip_VCLI
.endm



.align 4
.func vPortYieldCall
vPortYieldCall:
	/* Save general purpose registers. */
	pusha
	call disableSerialInChild
	.if configSUPPORT_FPU == 1

		/* If the task has a buffer allocated to save the FPU context then save
		the FPU context now. */
		movl	pucPortTaskFPUContextBuffer, %eax
		test	%eax, %eax
		je		1f
		fnsave	( %eax )
		fwait

		1:

		/* Save the address of the FPU context, if any. */
		push	pucPortTaskFPUContextBuffer

	.endif /* configSUPPORT_FPU */

	/* Find the TCB. */
	movl 	pxCurrentTCB, %eax

	/* Stack location is first item in the TCB. */
	movl	%esp, (%eax)

	call vTaskSwitchContext

	/* Find the location of pxCurrentTCB again - a callee saved register could
	be used in place of eax to prevent this second load, but that then relies
	on the compiler and other asm code. */
	call xTaskSwitchToProtectedTask
	movl 	pxCurrentTCB, %eax
	movl	(%eax), %esp

	.if configSUPPORT_FPU == 1

		/* Restore address of task's FPU context buffer. */
		pop 	pucPortTaskFPUContextBuffer

		/* If the task has a buffer allocated in which its FPU context is saved,
		then restore it now. */
		movl	pucPortTaskFPUContextBuffer, %eax
		test	%eax, %eax
		je		1f
		frstor	( %eax )
		1:
	.endif

	call Pip_VSTI
	popa
	iret

.endfunc
/*-----------------------------------------------------------*/

.align 4
.func vPortStartFirstTask
vPortStartFirstTask:

	/* Find the TCB. */
	movl 	pxCurrentTCB, %eax

	/* Stack location is first item in the TCB. */
	movl	(%eax), %esp

	/* Restore FPU context flag. */
	.if configSUPPORT_FPU == 1

		pop 	pucPortTaskFPUContextBuffer

	.endif /* configSUPPORT_FPU */

	//enableInterrupt
  call Pip_VSTI
  popa
	iret
	/* Restore general purpose registers. */
.endfunc
/*-----------------------------------------------------------*/

.align 4
.func vPortAPICErrorHandlerWrapper
vPortAPICErrorHandlerWrapper:
	pusha
	call	vPortAPICErrorHandler
	popa
	/* EOI. */
	/*movl	$0x00, (0xFEE000B0)*/
	iret
.endfunc
/*-----------------------------------------------------------*/

.align 4
.func vPortTimerHandler
.extern start_time
.extern printInfo
.extern disableSerialInChild
vPortTimerHandler:

	/* Save general purpose registers. */

	call Pip_VCLI
	call disableSerialInChild
	/*pusha*/
	pusha

/* Increment nesting count. */
	add 	$1, ulInterruptNesting

  /*call printInfo*/
	call xTaskIncrementTick
	/* Is a switch to another task required? */
	test	%eax, %eax
	je	_skip_context_switch

  call Pip_VCLI
  call	vTaskSwitchContext

_skip_context_switch:

	call Pip_VCLI
	/* Decrement the variable used to determine if a switch to a system
	stack is necessary. */
	sub $1, ulInterruptNesting
	/* Stack location is first item in the TCB. */

	call xTaskSwitchToProtectedTask

	movl pxCurrentTCB,%eax
	movl	(%eax), %esp
	movl 	%esp, %ebp

	call Pip_VSTI
	popa

	iret

.endfunc
/*-----------------------------------------------------------*/

.if configUSE_COMMON_INTERRUPT_ENTRY_POINT == 1

	.align 4
	.func vPortCentralInterruptWrapper
	vPortCentralInterruptWrapper:

		portFREERTOS_INTERRUPT_ENTRY

		movl $0xFEE00170, %eax 			/* Highest In Service Register (ISR) long word. */
		movl $8, %ecx					/* Loop counter. */

	next_isr_long_word:
		test %ecx, %ecx					/* Loop counter reached 0? */
		je wrapper_epilogue				/* Looked at all ISR registers without finding a bit set. */
		sub $1, %ecx					/* Sub 1 from loop counter. */
		movl (%eax), %ebx				/* Load next ISR long word. */
		sub $0x10, %eax					/* Point to next ISR long word in case no bits are set in the current long word. */
		test %ebx, %ebx					/* Are there any bits set? */
		je next_isr_long_word			/* Look at next ISR long word if no bits were set. */
		mov $0xFFFFFFFC, %eax
		and $0xFFFFFFFC, (%eax)
		bsr %ebx, %ebx					/* A bit was set, which one? */
		movl $32, %eax					/* Destination operand for following multiplication. */
		mul %ecx						/* Calculate base vector for current register, 32 vectors per register. */
		add %ebx, %eax					/* Add bit offset into register to get final vector number. */
		push %eax						/* Vector number is function parameter. */
		call vPortCentralInterruptHandler
		pop %eax						/* Remove parameter. */

	wrapper_epilogue:
		portFREERTOS_INTERRUPT_EXIT

	.endfunc

.endif /* configUSE_COMMON_INTERRUPT_ENTRY_POINT */
/*-----------------------------------------------------------*/

.align 4
.func vPortAPISpuriousHandler
vPortAPICSpuriousHandler:
	iret

.endfunc


.extern timerHandler

	INTERRUPT_HANDLER(irqTimer,vPortTimerHandler)
	INTERRUPT_HANDLER(pfAsm,pfHandler)
	INTERRUPT_HANDLER(keyAsm,keyHandler)
	INTERRUPT_HANDLER(serviceRoutineAsm,serviceRoutine)
	INTERRUPT_HANDLER(validChildAsm,validChild)
.end
