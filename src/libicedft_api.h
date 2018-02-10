/*-
 * Copyright (c) 2010, 2011, 2012, 2013, Columbia University
 * All rights reserved.
 *
 * This software was developed by Vasileios P. Kemerlis <vpk@cs.columbia.edu>
 * at Columbia University, New York, NY, USA, in June 2010.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Columbia University nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef LIBDFT_API_H
#define LIBDFT_API_H

#include <stdint.h>



#define GPR_NUM		8			/* general purpose registers */

/* FIXME: turn off the EFLAGS.AC bit by applying the corresponding mask */
#define CLEAR_EFLAGS_AC(eflags)	((eflags & 0xfffbffff))


enum {
/* #define */ SYSCALL_ARG0 = 0,			/* 1st argument in syscall */
/* #define */ SYSCALL_ARG1 = 1,			/* 2nd argument in syscall */
/* #define */ SYSCALL_ARG2 = 2,			/* 3rd argument in syscall */
/* #define */ SYSCALL_ARG3 = 3,			/* 4th argument in syscall */
/* #define */ SYSCALL_ARG4 = 4,			/* 5th argument in syscall */
/* #define */ SYSCALL_ARG5 = 5,			/* 6th argument in syscall */
/* #define */ SYSCALL_ARG_NUM = 6		/* syscall arguments */
};

enum {						 /* {en,dis}able (ins_desc_t) */
/* #define */ INSDFL_ENABLE	= 0,
/* #define */ INSDFL_DISABLE	= 1
};

// NOTE: This uses the same mapping as the vcpu_ctx_t struct defined below!
enum gpr {GPR_EDI, GPR_ESI, GPR_EBP, GPR_ESP, GPR_EBX, GPR_EDX, GPR_ECX, GPR_EAX, GPR_SCRATCH};

#ifdef USE_CUSTOM_TAG
#define TAGS_PER_GPR 4

// The gpr_reg_idx struct specifies an individual byte of a gpr reg.
struct gpr_idx
{
    gpr reg;
    uint8_t idx;
};
#endif

/*
 * virtual CPU (VCPU) context definition;
 * x86/x86_32/i386 arch
 */
typedef struct {
	/*
	 * general purpose registers (GPRs)
	 *
	 * we assign one bit of tag information for
	 * for every byte of addressable memory; the 32-bit
	 * GPRs of the x86 architecture will be represented
	 * with 4 bits each (the lower 4 bits of a 32-bit
	 * unsigned integer)
	 *
	 * NOTE the mapping:
	 * 	0: EDI
	 * 	1: ESI
	 * 	2: EBP
	 * 	3: ESP
	 * 	4: EBX
	 * 	5: EDX
	 * 	6: ECX
	 * 	7: EAX
	 * 	8: scratch (not a real register; helper) 
	 */

	uint32_t gpr[GPR_NUM + 1];

} vcpu_ctx_t;

/*
 * system call context definition
 *
 * only up to SYSCALL_ARGS (i.e., 6) are saved
 */
typedef struct {
	int 	nr;			/* syscall number */
	ADDRINT arg[SYSCALL_ARG_NUM];	/* arguments */
	ADDRINT ret;			/* return value */
	void	*aux;			/* auxiliary data (processor state) */
/* 	ADDRINT errno; */		/* error code */
} syscall_ctx_t;

/* thread context definition */
typedef struct {
	vcpu_ctx_t	vcpu;		/* VCPU context */
	syscall_ctx_t	syscall_ctx;	/* syscall context */
	void		*uval;		/* local storage */
} thread_ctx_t;




/* libdft API */
int	libicedft_init(idft_executer_api_t* executer_api ,  idft_context_t ** pcontext);
void libicedft_die(void);


/* REG API */
uint32_t REG32_INDX(idft_ins_t* ins , idft_context_t * context, idft_reg_t reg);
uint32_tREG16_INDX(idft_ins_t* ins , idft_context_t * context, idft_reg_t reg);







#endif