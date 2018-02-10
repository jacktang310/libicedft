#include "libicedft_core.h"
#include "libicedft_types.h"
#include "libicedft_util.h"
#include "libicedft_api.h"
#include "tagmap.h"
#include <stdint.h>


#define EXE context->executer_api


/* tagmap */
extern uint8_t	*bitmap;

/* fast tag extension (helper); [0] -> 0, [1] -> VCPU_MASK16 */
static uint32_t	MAP_8L_16[] = {0, VCPU_MASK16};

/* fast tag extension (helper); [0] -> 0, [2] -> VCPU_MASK16 */
static uint32_t	MAP_8H_16[] = {0, 0, VCPU_MASK16};

/* fast tag extension (helper); [0] -> 0, [1] -> VCPU_MASK32 */
static uint32_t	MAP_8L_32[] = {0, VCPU_MASK32};

/* fast tag extension (helper); [0] -> 0, [2] -> VCPU_MASK32 */
static uint32_t	MAP_8H_32[] = {0, 0, VCPU_MASK32};


/* Register reference helper macro's */

// Quickly reference the tags of register, only valid in a context where
// thread_ctx is defined!
#define RTAG thread_ctx->vcpu.gpr

// Quickly create arrays of register tags, only valid in a context where RTAG is valid!
#define R16TAG(RIDX) \
    {RTAG[(RIDX)][0], RTAG[(RIDX)][1]}


#define R32TAG(RIDX) \
    {RTAG[(RIDX)][0], RTAG[(RIDX)][1], RTAG[(RIDX)][2], RTAG[(RIDX)][3]}

// Quickly create arrays of memory tags, only valid in a context where tag_dir_getb is valid!
// Note: Unlike the R*TAG macros, the M*TAG macros cannot be used to assign tags!
#define M8TAG(ADDR) \
    tag_dir_getb(tag_dir, (ADDR))

#define M16TAG(ADDR) \
    {M8TAG(ADDR), M8TAG(ADDR+1)}

#define M32TAG(ADDR) \
    {M8TAG(ADDR), M8TAG(ADDR+1), M8TAG(ADDR+2), M8TAG(ADDR+3) }

/*
 * tag propagation (analysis function)
 *
 * propagate tag among three 8-bit registers as t[dst] |= t[upper(src)];
 * dst is AX, whereas src is an upper 8-bit register (e.g., CH, BH, ...)
 *
 * NOTE: special case for DIV and IDIV instructions
 *
 * @thread_ctx:	the thread context
 * @src:	source register index (VCPU)
 */
void r2r_ternary_opb_u(thread_ctx_t *thread_ctx, idft_reg_t src)
{
	/* temporary tag value */
	idft_reg_t tmp_tag = thread_ctx->vcpu.gpr[src] & (VCPU_MASK8 << 1);
	
	/* update the destination (ternary) */
	thread_ctx->vcpu.gpr[7] |= MAP_8H_16[tmp_tag];

}

/*
 * tag propagation (analysis function)
 *
 * propagate tag among three 8-bit registers as t[dst] |= t[lower(src)];
 * dst is AX, whereas src is a lower 8-bit register (e.g., CL, BL, ...)
 *
 * NOTE: special case for DIV and IDIV instructions
 *
 * @thread_ctx:	the thread context
 * @src:	source register index (VCPU)
 */
void r2r_ternary_opb_l(thread_ctx_t *thread_ctx, idft_reg_t src)
{
	/* temporary tag value */
	idft_reg_t tmp_tag = thread_ctx->vcpu.gpr[src] & VCPU_MASK8;

	/* update the destination (ternary) */
	thread_ctx->vcpu.gpr[7] |= MAP_8L_16[tmp_tag];

}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between among three 16-bit 
 * registers as t[dst1] |= t[src] and t[dst2] |= t[src];
 * dst1 is DX, dst2 is AX, and src is a 16-bit register 
 * (e.g., CX, BX, ...)
 *
 * NOTE: special case for DIV and IDIV instructions
 *
 * @thread_ctx:	the thread context
 * @src:	source register index (VCPU)
 */
void r2r_ternary_opw(thread_ctx_t *thread_ctx, idft_reg_t src)
{
	/* temporary tag value */
	idft_reg_t tmp_tag = thread_ctx->vcpu.gpr[src] & VCPU_MASK16;
	
	/* update the destinations */
	thread_ctx->vcpu.gpr[5] |= tmp_tag;
	thread_ctx->vcpu.gpr[7] |= tmp_tag;

}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between among three 32-bit 
 * registers as t[dst1] |= t[src] and t[dst2] |= t[src];
 * dst1 is EDX, dst2 is EAX, and src is a 32-bit register
 * (e.g., ECX, EBX, ...)
 *
 * NOTE: special case for DIV and IDIV instructions
 *
 * @thread_ctx:	the thread context
 * @src:	source register index (VCPU)
 */
void r2r_ternary_opl(thread_ctx_t *thread_ctx, idft_reg_t src)
{ 
	/* update the destinations */
	thread_ctx->vcpu.gpr[5] |= thread_ctx->vcpu.gpr[src];
	thread_ctx->vcpu.gpr[7] |= thread_ctx->vcpu.gpr[src];

}

/*
 * tag propagation (analysis function)
 *
 * propagate tag among two 8-bit registers
 * and an 8-bit memory location as t[dst] |= t[src];
 * dst is AX, whereas src is an 8-bit memory location
 *
 * NOTE: special case for DIV and IDIV instructions
 *
 * @thread_ctx:	the thread context
 * @src:	source memory address
 */
void m2r_ternary_opb(thread_ctx_t *thread_ctx, ADDRINT src)
{
	/* temporary tag value */
	idft_reg_t tmp_tag = 
		(bitmap[VIRT2BYTE(src)] >> VIRT2BIT(src)) & VCPU_MASK8;
	
	/* update the destination (ternary) */
	thread_ctx->vcpu.gpr[7] |= MAP_8L_16[tmp_tag];

}

/*
 * tag propagation (analysis function)
 *
 * propagate tag among two 16-bit registers
 * and a 16-bit memory address as
 * t[dst1] |= t[src] and t[dst1] |= t[src];
 *
 * dst1 is DX, dst2 is AX, and src is a 16-bit
 * memory location
 *
 * NOTE: special case for DIV and IDIV instructions
 *
 * @thread_ctx:	the thread context
 * @src:	source memory address
 */
void m2r_ternary_opw(thread_ctx_t *thread_ctx, ADDRINT src)
{
	/* temporary tag value */
	idft_reg_t tmp_tag = 
		(*((uint16_t *)(bitmap + VIRT2BYTE(src))) >> VIRT2BIT(src)) &
		VCPU_MASK16;
	
	/* update the destinations */
	thread_ctx->vcpu.gpr[5] |= tmp_tag; 
	thread_ctx->vcpu.gpr[7] |= tmp_tag; 

}

/*
 * tag propagation (analysis function)
 *
 * propagate tag among two 32-bit 
 * registers and a 32-bit memory as
 * t[dst1] |= t[src] and t[dst2] |= t[src];
 * dst1 is EDX, dst2 is EAX, and src is a 32-bit
 * memory location
 *
 * NOTE: special case for DIV and IDIV instructions
 *
 * @thread_ctx:	the thread context
 * @src:	source memory address
 */
void m2r_ternary_opl(thread_ctx_t *thread_ctx, ADDRINT src)
{
	/* temporary tag value */
	idft_reg_t tmp_tag = 
		(*((uint16_t *)(bitmap + VIRT2BYTE(src))) >> VIRT2BIT(src)) &
		VCPU_MASK32;

	/* update the destinations */
	thread_ctx->vcpu.gpr[5] |= tmp_tag;
	thread_ctx->vcpu.gpr[7] |= tmp_tag;

}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between two 8-bit 
 * registers as t[upper(dst)] |= t[lower(src)];
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source register index (VCPU)
 */
void r2r_binary_opb_ul(thread_ctx_t *thread_ctx, idft_reg_t dst, idft_reg_t src)
{
	thread_ctx->vcpu.gpr[dst] |=
		(thread_ctx->vcpu.gpr[src] & VCPU_MASK8) << 1;

}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between two 8-bit 
 * registers as t[lower(dst)] |= t[upper(src)];
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source register index (VCPU)
 */
void r2r_binary_opb_lu(thread_ctx_t *thread_ctx, idft_reg_t dst, idft_reg_t src)
{
	thread_ctx->vcpu.gpr[dst] |=
		(thread_ctx->vcpu.gpr[src] & (VCPU_MASK8 << 1)) >> 1;

}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between two 8-bit 
 * registers as t[upper(dst)] |= t[upper(src)]
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source register index (VCPU)
 */
void r2r_binary_opb_u(thread_ctx_t *thread_ctx, idft_reg_t dst, idft_reg_t src)
{
	thread_ctx->vcpu.gpr[dst] |=
		thread_ctx->vcpu.gpr[src] & (VCPU_MASK8 << 1);

}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between two 8-bit 
 * registers as t[lower(dst)] |= t[lower(src)]
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source register index (VCPU)
 */
void r2r_binary_opb_l(thread_ctx_t *thread_ctx, idft_reg_t dst, idft_reg_t src)
{
	thread_ctx->vcpu.gpr[dst] |=
		thread_ctx->vcpu.gpr[src] & VCPU_MASK8;

}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between two 16-bit registers
 * as t[dst] |= t[src]
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source register index (VCPU)
 */
void r2r_binary_opw(thread_ctx_t *thread_ctx, idft_reg_t dst, idft_reg_t src)
{
	thread_ctx->vcpu.gpr[dst] |=
		thread_ctx->vcpu.gpr[src] & VCPU_MASK16;
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between two 32-bit 
 * registers as t[dst] |= t[src]
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source register index (VCPU)
 */
void r2r_binary_opl(thread_ctx_t *thread_ctx, idft_reg_t dst, idft_reg_t src)
{
	thread_ctx->vcpu.gpr[dst] |= thread_ctx->vcpu.gpr[src];

}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between an 8-bit 
 * register and a memory location as
 * t[upper(dst)] |= t[src] (dst is a register)
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source memory address
 */
void m2r_binary_opb_u(thread_ctx_t *thread_ctx, idft_reg_t dst, ADDRINT src)
{
	thread_ctx->vcpu.gpr[dst] |=
		((bitmap[VIRT2BYTE(src)] >> VIRT2BIT(src)) & VCPU_MASK8) << 1;
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between an 8-bit 
 * register and a memory location as
 * t[lower(dst)] |= t[src] (dst is a register)
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source memory address
 */
void m2r_binary_opb_l(thread_ctx_t *thread_ctx, idft_reg_t dst, ADDRINT src)
{
	thread_ctx->vcpu.gpr[dst] |=
		(bitmap[VIRT2BYTE(src)] >> VIRT2BIT(src)) & VCPU_MASK8;
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between a 16-bit 
 * register and a memory location as
 * t[dst] |= t[src] (dst is a register)
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source memory address
 */
void m2r_binary_opw(thread_ctx_t *thread_ctx, idft_reg_t dst, ADDRINT src)
{
	thread_ctx->vcpu.gpr[dst] |=
		(*((uint16_t *)(bitmap + VIRT2BYTE(src))) >> VIRT2BIT(src)) &
		VCPU_MASK16;

}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between a 32-bit 
 * register and a memory location as
 * t[dst] |= t[src] (dst is a register)
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source memory address
 */
void m2r_binary_opl(thread_ctx_t *thread_ctx, idft_reg_t dst, ADDRINT src)
{
	thread_ctx->vcpu.gpr[dst] |=
		(*((uint16_t *)(bitmap + VIRT2BYTE(src))) >> VIRT2BIT(src)) &
		VCPU_MASK32;

}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between an 8-bit 
 * register and a memory location as
 * t[dst] |= t[upper(src)] (src is a register)
 *
 * @thread_ctx:	the thread context
 * @dst:	destination memory address
 * @src:	source register index (VCPU)
 */
void r2m_binary_opb_u(thread_ctx_t *thread_ctx, ADDRINT dst, idft_reg_t src)
{
	bitmap[VIRT2BYTE(dst)] |=
		((thread_ctx->vcpu.gpr[src] & (VCPU_MASK8 << 1)) >> 1)
		<< VIRT2BIT(dst);
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between an 8-bit 
 * register and a memory location as
 * t[dst] |= t[lower(src)] (src is a register)
 *
 * @thread_ctx:	the thread context
 * @dst:	destination memory address
 * @src:	source register index (VCPU)
 */
void r2m_binary_opb_l(thread_ctx_t *thread_ctx, ADDRINT dst, idft_reg_t src)
{
	bitmap[VIRT2BYTE(dst)] |=
		(thread_ctx->vcpu.gpr[src] & VCPU_MASK8) << VIRT2BIT(dst);

}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between a 16-bit 
 * register and a memory location as
 * t[dst] |= t[src] (src is a register)
 *
 * @thread_ctx:	the thread context
 * @dst:	destination memory address
 * @src:	source register index (VCPU)
 */
void r2m_binary_opw(thread_ctx_t *thread_ctx, ADDRINT dst, idft_reg_t src)
{
	*((uint16_t *)(bitmap + VIRT2BYTE(dst))) |=
		(thread_ctx->vcpu.gpr[src] & VCPU_MASK16) <<
		VIRT2BIT(dst);
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between a 32-bit 
 * register and a memory location as
 * t[dst] |= t[src] (src is a register)
 *
 * @thread_ctx:	the thread context
 * @dst:	destination memory address
 * @src:	source register index (VCPU)
 */
void r2m_binary_opl(thread_ctx_t *thread_ctx, ADDRINT dst, idft_reg_t src)
{
	*((uint16_t *)(bitmap + VIRT2BYTE(dst))) |=
		(thread_ctx->vcpu.gpr[src] & VCPU_MASK32) <<
		VIRT2BIT(dst);

}

/*
 * tag propagation (analysis function)
 *
 * clear the tag of EAX, EBX, ECX, EDX
 *
 * @thread_ctx:	the thread context
 * @reg:	register index (VCPU) 
 */
void r_clrl4(thread_ctx_t *thread_ctx)
{
	thread_ctx->vcpu.gpr[4] = 0;
	thread_ctx->vcpu.gpr[5] = 0;
	thread_ctx->vcpu.gpr[6] = 0;
	thread_ctx->vcpu.gpr[7] = 0;

}

/*
 * tag propagation (analysis function)
 *
 * clear the tag of EAX, EDX
 *
 * @thread_ctx:	the thread context
 * @reg:	register index (VCPU) 
 */
void r_clrl2(thread_ctx_t *thread_ctx)
{
	thread_ctx->vcpu.gpr[5] = 0;
	thread_ctx->vcpu.gpr[7] = 0;
}

/*
 * tag propagation (analysis function)
 *
 * clear the tag of a 16-bit register
 *
 * @thread_ctx:	the thread context
 * @reg:	register index (VCPU) 
 */
void r_clrl(thread_ctx_t *thread_ctx, idft_reg_t reg)
{

	thread_ctx->vcpu.gpr[reg] = 0;

}



/*
 * tag propagation (analysis function)
 *
 * clear the tag of a 16-bit register
 *
 * @thread_ctx:	the thread context
 * @reg:	register index (VCPU) 
 */
void r_clrw(thread_ctx_t *thread_ctx, idft_reg_t reg)
{

	thread_ctx->vcpu.gpr[reg] &= ~VCPU_MASK16;

}




/*
 * tag propagation (analysis function)
 *
 * clear the tag of an upper 8-bit register
 *
 * @thread_ctx:	the thread context
 * @reg:	register index (VCPU) 
 */
void r_clrb_u(thread_ctx_t *thread_ctx, idft_reg_t reg)
{
	thread_ctx->vcpu.gpr[reg] &= ~(VCPU_MASK8 << 1);
}


/*
 * tag propagation (analysis function)
 *
 * clear the tag of a lower 8-bit register
 *
 * @thread_ctx:	the thread context
 * @reg:	register index (VCPU) 
 */
void r_clrb_l(thread_ctx_t *thread_ctx, idft_reg_t reg)
{
	thread_ctx->vcpu.gpr[reg] &= ~VCPU_MASK8;

}


/*
 * tag propagation (analysis function)
 *
 * propagate tag between two 8-bit 
 * registers as t[upper(dst)] = t[lower(src)]
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source register index (VCPU)
 */
void r2r_xfer_opb_ul(thread_ctx_t *thread_ctx, idft_reg_t dst, idft_reg_t src)
{
	 thread_ctx->vcpu.gpr[dst] =
		 (thread_ctx->vcpu.gpr[dst] & ~(VCPU_MASK8 << 1)) |
		 ((thread_ctx->vcpu.gpr[src] & VCPU_MASK8) << 1);
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between two 8-bit 
 * registers as t[lower(dst)] = t[upper(src)];
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source register index (VCPU)
 */
void r2r_xfer_opb_lu(thread_ctx_t *thread_ctx, idft_reg_t dst, idft_reg_t src)
{
	thread_ctx->vcpu.gpr[dst] =
		(thread_ctx->vcpu.gpr[dst] & ~VCPU_MASK8) | 
		((thread_ctx->vcpu.gpr[src] & (VCPU_MASK8 << 1)) >> 1);

}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between two 8-bit 
 * registers as t[upper(dst)] = t[upper(src)]
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source register index (VCPU)
 */
void r2r_xfer_opb_u(thread_ctx_t *thread_ctx, idft_reg_t dst, idft_reg_t src)
{
	thread_ctx->vcpu.gpr[dst] =
		(thread_ctx->vcpu.gpr[dst] & ~(VCPU_MASK8 << 1)) |
		(thread_ctx->vcpu.gpr[src] & (VCPU_MASK8 << 1));
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between two 8-bit 
 * registers as t[lower(dst)] = t[lower(src)]
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source register index (VCPU)
 */
void r2r_xfer_opb_l(thread_ctx_t *thread_ctx, idft_reg_t dst, idft_reg_t src)
{
	thread_ctx->vcpu.gpr[dst] =
		(thread_ctx->vcpu.gpr[dst] & ~VCPU_MASK8) |
		(thread_ctx->vcpu.gpr[src] & VCPU_MASK8);

}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between two 16-bit 
 * registers as t[dst] = t[src]
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source register index (VCPU)
 */
void r2r_xfer_opw(thread_ctx_t *thread_ctx, idft_reg_t dst, idft_reg_t src)
{
	thread_ctx->vcpu.gpr[dst] =
		(thread_ctx->vcpu.gpr[dst] & ~VCPU_MASK16) |
		(thread_ctx->vcpu.gpr[src] & VCPU_MASK16);

}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between two 32-bit 
 * registers as t[dst] = t[src]
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source register index (VCPU)
 */
void r2r_xfer_opl(thread_ctx_t *thread_ctx, idft_reg_t dst, idft_reg_t src)
{
	thread_ctx->vcpu.gpr[dst] =
		thread_ctx->vcpu.gpr[src];
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between an upper 8-bit 
 * register and a memory location as
 * t[dst] = t[src] (dst is a register)
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source memory address
 */
void m2r_xfer_opb_u(thread_ctx_t *thread_ctx, idft_reg_t dst, ADDRINT src)
{
	thread_ctx->vcpu.gpr[dst] =
		(thread_ctx->vcpu.gpr[dst] & ~(VCPU_MASK8 << 1)) |
		(((bitmap[VIRT2BYTE(src)] >> VIRT2BIT(src)) << 1) &
		(VCPU_MASK8 << 1));

}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between a lower 8-bit 
 * register and a memory location as
 * t[dst] = t[src] (dst is a register)
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source memory address
 */
void m2r_xfer_opb_l(thread_ctx_t *thread_ctx, idft_reg_t dst, ADDRINT src)
{
	thread_ctx->vcpu.gpr[dst] =
		(thread_ctx->vcpu.gpr[dst] & ~VCPU_MASK8) |
		((bitmap[VIRT2BYTE(src)] >> VIRT2BIT(src)) & VCPU_MASK8);
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between a 16-bit 
 * register and a memory location as
 * t[dst] = t[src] (dst is a register)
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source memory address
 */
void m2r_xfer_opw(thread_ctx_t *thread_ctx, idft_reg_t dst, ADDRINT src)
{
	thread_ctx->vcpu.gpr[dst] =
		(thread_ctx->vcpu.gpr[dst] & ~VCPU_MASK16) |
		((*((uint16_t *)(bitmap + VIRT2BYTE(src))) >> VIRT2BIT(src)) &
		VCPU_MASK16);

}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between a 32-bit 
 * register and a memory location as
 * t[dst] = t[src] (dst is a register)
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source memory address
 */
void m2r_xfer_opl(thread_ctx_t *thread_ctx, idft_reg_t dst, ADDRINT src)
{
	thread_ctx->vcpu.gpr[dst] =
		(*((uint16_t *)(bitmap + VIRT2BYTE(src))) >> VIRT2BIT(src)) &
		VCPU_MASK32;

}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between an 8-bit 
 * register and a n-memory locations as
 * t[dst] = t[src]; src is AL
 *
 * @thread_ctx:	the thread context
 * @dst:	destination memory address
 * @count:	memory bytes
 * @eflags:	the value of the EFLAGS register
 *
 */
void r2m_xfer_opbn(thread_ctx_t *thread_ctx, ADDRINT dst, ADDRINT count, 
        ADDRINT eflags)
{
	if (likely(EFLAGS_DF(eflags) == 0)) {
		/* EFLAGS.DF = 0 */

		/* the source register is taged */
		if (thread_ctx->vcpu.gpr[7] & VCPU_MASK8)
			tagmap_setn(dst, count);
		/* the source register is clear */
		else
			tagmap_clrn(dst, count);
	}
	else {
		/* EFLAGS.DF = 1 */

		/* the source register is taged */
		if (thread_ctx->vcpu.gpr[7] & VCPU_MASK8)
			tagmap_setn(dst - count + 1, count);
		/* the source register is clear */
		else
			tagmap_clrn(dst - count + 1, count);
	
	}

}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between an 8-bit 
 * register and a memory location as
 * t[dst] = t[upper(src)] (src is a register)
 *
 * @thread_ctx:	the thread context
 * @dst:	destination memory address
 * @src:	source register index (VCPU)
 */
void r2m_xfer_opb_u(thread_ctx_t *thread_ctx, ADDRINT dst, idft_reg_t src)
{
#ifndef USE_CUSTOM_TAG
	bitmap[VIRT2BYTE(dst)] =
		(bitmap[VIRT2BYTE(dst)] & ~(BYTE_MASK << VIRT2BIT(dst))) |
		(((thread_ctx->vcpu.gpr[src] & (VCPU_MASK8 << 1)) >> 1)
		<< VIRT2BIT(dst));
#else
    tag_t src_tag = RTAG[src][1];

    tag_dir_setb(tag_dir, dst, src_tag);
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between an 8-bit 
 * register and a memory location as
 * t[dst] = t[lower(src)] (src is a register)
 *
 * @thread_ctx:	the thread context
 * @dst:	destination memory address
 * @src:	source register index (VCPU)
 */
void r2m_xfer_opb_l(thread_ctx_t *thread_ctx, ADDRINT dst, idft_reg_t src)
{

	bitmap[VIRT2BYTE(dst)] =
		(bitmap[VIRT2BYTE(dst)] & ~(BYTE_MASK << VIRT2BIT(dst))) |
		((thread_ctx->vcpu.gpr[src] & VCPU_MASK8) << VIRT2BIT(dst));

}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between a 16-bit 
 * register and a n-memory locations as
 * t[dst] = t[src]; src is AX
 *
 * @thread_ctx:	the thread context
 * @dst:	destination memory address
 * @count:	memory words
 * @eflags:	the value of the EFLAGS register
 */
void r2m_xfer_opwn(thread_ctx_t *thread_ctx,
		ADDRINT dst,
		ADDRINT count,
		ADDRINT eflags)
{
	if (likely(EFLAGS_DF(eflags) == 0)) {
		/* EFLAGS.DF = 0 */

		/* the source register is taged */
		if (thread_ctx->vcpu.gpr[7] & VCPU_MASK16)
			tagmap_setn(dst, (count << 1));
		/* the source register is clear */
		else
			tagmap_clrn(dst, (count << 1));
	}
	else {
		/* EFLAGS.DF = 1 */

		/* the source register is taged */
		if (thread_ctx->vcpu.gpr[7] & VCPU_MASK16)
			tagmap_setn(dst - (count << 1) + 1, (count << 1));
		/* the source register is clear */
		else
			tagmap_clrn(dst - (count << 1) + 1, (count << 1));
	}

}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between a 16-bit 
 * register and a memory location as
 * t[dst] = t[src] (src is a register)
 *
 * @thread_ctx:	the thread context
 * @dst:	destination memory address
 * @src:	source register index (VCPU)
 */
void r2m_xfer_opw(thread_ctx_t *thread_ctx, ADDRINT dst, idft_reg_t src)
{
	*((uint16_t *)(bitmap + VIRT2BYTE(dst))) =
		(*((uint16_t *)(bitmap + VIRT2BYTE(dst))) & ~(WORD_MASK <<
							      VIRT2BIT(dst))) |
		((uint16_t)(thread_ctx->vcpu.gpr[src] & VCPU_MASK16) <<
		VIRT2BIT(dst));

}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between a 32-bit 
 * register and a n-memory locations as
 * t[dst] = t[src]; src is EAX
 *
 * @thread_ctx:	the thread context
 * @dst:	destination memory address
 * @src:	source register index (VCPU)
 * @count:	memory double words
 * @eflags:	the value of the EFLAGS register
 */
void r2m_xfer_opln(thread_ctx_t *thread_ctx,
		ADDRINT dst,
		ADDRINT count,
		ADDRINT eflags)
{
#ifndef USE_CUSTOM_TAG
	if (likely(EFLAGS_DF(eflags) == 0)) {
		/* EFLAGS.DF = 0 */

		/* the source register is taged */
		if (thread_ctx->vcpu.gpr[7])
			tagmap_setn(dst, (count << 2));
		/* the source register is clear */
		else
			tagmap_clrn(dst, (count << 2));
	}
	else {
		/* EFLAGS.DF = 1 */

		/* the source register is taged */
		if (thread_ctx->vcpu.gpr[7])
			tagmap_setn(dst - (count << 2) + 1, (count << 2));
		/* the source register is clear */
		else
			tagmap_clrn(dst - (count << 2) + 1, (count << 2));
	}
#else
    tag_t src_tag[] = R32TAG(GPR_EAX);
	if (likely(EFLAGS_DF(eflags) == 0)) {
		/* EFLAGS.DF = 0 */

        for (size_t i = 0; i < (count << 2); i++)
        {
            tag_dir_setb(tag_dir, dst+i, src_tag[i%4]);

        }
	}
	else {
		/* EFLAGS.DF = 1 */

        for (size_t i = 0; i < (count << 2); i++)
        {
            size_t dst_addr = dst - (count << 2) + 1 + i;
            tag_dir_setb(tag_dir, dst_addr, src_tag[i%4]);

        }
	}
#endif
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between a 32-bit 
 * register and a memory location as
 * t[dst] = t[src] (src is a register)
 *
 * @thread_ctx:	the thread context
 * @dst:	destination memory address
 * @src:	source register index (VCPU)
 */
void r2m_xfer_opl(thread_ctx_t *thread_ctx, ADDRINT dst, idft_reg_t src)
{
	*((uint16_t *)(bitmap + VIRT2BYTE(dst))) =
		(*((uint16_t *)(bitmap + VIRT2BYTE(dst))) & ~(LONG_MASK <<
							      VIRT2BIT(dst))) |
		((uint16_t)(thread_ctx->vcpu.gpr[src] & VCPU_MASK32) <<
		VIRT2BIT(dst));

}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between two 16-bit 
 * memory locations as t[dst] = t[src]
 *
 * @dst:	destination memory address
 * @src:	source memory address
 */
void m2m_xfer_opw(ADDRINT dst, ADDRINT src)
{

	*((uint16_t *)(bitmap + VIRT2BYTE(dst))) =
		(*((uint16_t *)(bitmap + VIRT2BYTE(dst))) & ~(WORD_MASK <<
							      VIRT2BIT(dst))) |
		(((*((uint16_t *)(bitmap + VIRT2BYTE(src)))) >> VIRT2BIT(src))
		& WORD_MASK) << VIRT2BIT(dst);

}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between two 8-bit 
 * memory locations as t[dst] = t[src]
 *
 * @dst:	destination memory address
 * @src:	source memory address
 */
void m2m_xfer_opb(ADDRINT dst, ADDRINT src)
{
	*((uint16_t *)(bitmap + VIRT2BYTE(dst))) =
		(*((uint16_t *)(bitmap + VIRT2BYTE(dst))) & ~(BYTE_MASK <<
							      VIRT2BIT(dst))) |
		(((*((uint16_t *)(bitmap + VIRT2BYTE(src)))) >> VIRT2BIT(src))
		& BYTE_MASK) << VIRT2BIT(dst);
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between two 32-bit 
 * memory locations as t[dst] = t[src]
 *
 * @dst:	destination memory address
 * @src:	source memory address
 */
void m2m_xfer_opl(ADDRINT dst, ADDRINT src)
{
	*((uint16_t *)(bitmap + VIRT2BYTE(dst))) =
		(*((uint16_t *)(bitmap + VIRT2BYTE(dst))) & ~(LONG_MASK <<
							      VIRT2BIT(dst))) |
		(((*((uint16_t *)(bitmap + VIRT2BYTE(src)))) >> VIRT2BIT(src))
		& LONG_MASK) << VIRT2BIT(dst);

}

/*
 * tag propagation (analysis function)
 *
 * instrumentation helper; returns the flag that
 * takes as argument -- seems lame, but it is
 * necessary for aiding conditional analysis to
 * be inlined. Typically used with INS_InsertIfCall()
 * in order to return true (i.e., allow the execution
 * of the function that has been instrumented with
 * INS_InsertThenCall()) only once
 *
 * first_iteration:	flag; indicates whether the rep-prefixed instruction is
 * 			executed for the first time or not
 */
ADDRINT rep_predicate(BOOL first_iteration)
{
	/* return the flag; typically this is true only once */
	return first_iteration; 
}

/*
 * tag propagation (analysis function)
 *
 * restore the tag values for all the
 * 16-bit general purpose registers from
 * the memory
 *
 * NOTE: special case for POPA instruction 
 *
 * @thread_ctx:	the thread context
 * @src:	the source memory address	
 */
void m2r_restore_opw(thread_ctx_t *thread_ctx, ADDRINT src)
{
	/* tagmap value */
	uint16_t src_val = *(uint16_t *)(bitmap + VIRT2BYTE(src));

	/* restore DI */
	thread_ctx->vcpu.gpr[0] =
		(thread_ctx->vcpu.gpr[0] & ~VCPU_MASK16) |
		((src_val >> VIRT2BIT(src)) & VCPU_MASK16);
	
	/* restore SI */
	thread_ctx->vcpu.gpr[1] =
		(thread_ctx->vcpu.gpr[1] & ~VCPU_MASK16) |
		((src_val >> VIRT2BIT(src + 2)) & VCPU_MASK16);
	
	/* restore BP */
	thread_ctx->vcpu.gpr[2] =
		(thread_ctx->vcpu.gpr[2] & ~VCPU_MASK16) |
		((src_val >> VIRT2BIT(src + 4)) & VCPU_MASK16);
	
	/* update the tagmap value */
	src	+= 8;
	src_val	= *(uint16_t *)(bitmap + VIRT2BYTE(src));

	/* restore BX */
	thread_ctx->vcpu.gpr[4] =
		(thread_ctx->vcpu.gpr[4] & ~VCPU_MASK16) |
		((src_val >> VIRT2BIT(src)) & VCPU_MASK16);
	
	/* restore DX */
	thread_ctx->vcpu.gpr[5] =
		(thread_ctx->vcpu.gpr[5] & ~VCPU_MASK16) |
		((src_val >> VIRT2BIT(src + 2)) & VCPU_MASK16);
	
	/* restore CX */
	thread_ctx->vcpu.gpr[6] =
		(thread_ctx->vcpu.gpr[6] & ~VCPU_MASK16) |
		((src_val >> VIRT2BIT(src + 4)) & VCPU_MASK16);
	
	/* restore AX */
	thread_ctx->vcpu.gpr[7] =
		(thread_ctx->vcpu.gpr[7] & ~VCPU_MASK16) |
		((src_val >> VIRT2BIT(src + 6)) & VCPU_MASK16);

}

/*
 * tag propagation (analysis function)
 *
 * restore the tag values for all the
 * 32-bit general purpose registers from
 * the memory
 *
 * NOTE: special case for POPAD instruction 
 *
 * @thread_ctx:	the thread context
 * @src:	the source memory address	
 */
void m2r_restore_opl(thread_ctx_t *thread_ctx, ADDRINT src)
{
	/* tagmap value */
	uint16_t src_val = *(uint16_t *)(bitmap + VIRT2BYTE(src));

	/* restore EDI */
	thread_ctx->vcpu.gpr[0] =
		(src_val >> VIRT2BIT(src)) & VCPU_MASK32;

	/* restore ESI */
	thread_ctx->vcpu.gpr[1] =
		(src_val >> VIRT2BIT(src + 4)) & VCPU_MASK32;
	
	/* update the tagmap value */
	src	+= 8;
	src_val	= *(uint16_t *)(bitmap + VIRT2BYTE(src));

	/* restore EBP */
	thread_ctx->vcpu.gpr[2] =
		(src_val >> VIRT2BIT(src)) & VCPU_MASK32;
	
	/* update the tagmap value */
	src	+= 8;
	src_val	= *(uint16_t *)(bitmap + VIRT2BYTE(src));

	/* restore EBX */
	thread_ctx->vcpu.gpr[4] =
		(src_val >> VIRT2BIT(src)) & VCPU_MASK32;
	
	/* restore EDX */
	thread_ctx->vcpu.gpr[5] =
		(src_val >> VIRT2BIT(src + 4)) & VCPU_MASK32;
	
	/* update the tagmap value */
	src	+= 8;
	src_val	= *(uint16_t *)(bitmap + VIRT2BYTE(src));

	/* restore ECX */
	thread_ctx->vcpu.gpr[6] =
		(src_val >> VIRT2BIT(src)) & VCPU_MASK32;
	
	/* restore EAX */
	thread_ctx->vcpu.gpr[7] =
		(src_val >> VIRT2BIT(src + 4)) & VCPU_MASK32;

}

/*
 * tag propagation (analysis function)
 *
 * save the tag values for all the 16-bit
 * general purpose registers into the memory
 *
 * NOTE: special case for PUSHA instruction
 *
 * @thread_ctx:	the thread context
 * @dst:	the destination memory address
 */
void r2m_save_opw(thread_ctx_t *thread_ctx, ADDRINT dst)
{
	/* save DI */
	*((uint16_t *)(bitmap + VIRT2BYTE(dst))) =
		(*((uint16_t *)(bitmap + VIRT2BYTE(dst))) & ~(WORD_MASK <<
							      VIRT2BIT(dst))) |
		((uint16_t)(thread_ctx->vcpu.gpr[0] & VCPU_MASK16) <<
		VIRT2BIT(dst));

	/* update the destination memory */
	dst += 2;

	/* save SI */
	*((uint16_t *)(bitmap + VIRT2BYTE(dst))) =
		(*((uint16_t *)(bitmap + VIRT2BYTE(dst))) & ~(WORD_MASK <<
							      VIRT2BIT(dst))) |
		((uint16_t)(thread_ctx->vcpu.gpr[1] & VCPU_MASK16) <<
		VIRT2BIT(dst));

	/* update the destination memory */
	dst += 2;

	/* save BP */
	*((uint16_t *)(bitmap + VIRT2BYTE(dst))) =
		(*((uint16_t *)(bitmap + VIRT2BYTE(dst))) & ~(WORD_MASK <<
							      VIRT2BIT(dst))) |
		((uint16_t)(thread_ctx->vcpu.gpr[2] & VCPU_MASK16) <<
		VIRT2BIT(dst));

	/* update the destination memory */
	dst += 2;

	/* save SP */
	*((uint16_t *)(bitmap + VIRT2BYTE(dst))) =
		(*((uint16_t *)(bitmap + VIRT2BYTE(dst))) & ~(WORD_MASK <<
							      VIRT2BIT(dst))) |
		((uint16_t)(thread_ctx->vcpu.gpr[3] & VCPU_MASK16) <<
		VIRT2BIT(dst));

	/* update the destination memory */
	dst += 2;

	/* save BX */
	*((uint16_t *)(bitmap + VIRT2BYTE(dst))) =
		(*((uint16_t *)(bitmap + VIRT2BYTE(dst))) & ~(WORD_MASK <<
							      VIRT2BIT(dst))) |
		((uint16_t)(thread_ctx->vcpu.gpr[4] & VCPU_MASK16) <<
		VIRT2BIT(dst));

	/* update the destination memory */
	dst += 2;

	/* save DX */
	*((uint16_t *)(bitmap + VIRT2BYTE(dst))) =
		(*((uint16_t *)(bitmap + VIRT2BYTE(dst))) & ~(WORD_MASK <<
							      VIRT2BIT(dst))) |
		((uint16_t)(thread_ctx->vcpu.gpr[5] & VCPU_MASK16) <<
		VIRT2BIT(dst));

	/* update the destination memory */
	dst += 2;

	/* save CX */
	*((uint16_t *)(bitmap + VIRT2BYTE(dst))) =
		(*((uint16_t *)(bitmap + VIRT2BYTE(dst))) & ~(WORD_MASK <<
							      VIRT2BIT(dst))) |
		((uint16_t)(thread_ctx->vcpu.gpr[6] & VCPU_MASK16) <<
		VIRT2BIT(dst));

	/* update the destination memory */
	dst += 2;

	/* save AX */
	*((uint16_t *)(bitmap + VIRT2BYTE(dst))) =
		(*((uint16_t *)(bitmap + VIRT2BYTE(dst))) & ~(WORD_MASK <<
							      VIRT2BIT(dst))) |
		((uint16_t)(thread_ctx->vcpu.gpr[7] & VCPU_MASK16) <<
		VIRT2BIT(dst));
}

/*
 * tag propagation (analysis function)
 *
 * save the tag values for all the 32-bit
 * general purpose registers into the memory
 *
 * NOTE: special case for PUSHAD instruction 
 *
 * @thread_ctx:	the thread context
 * @dst:	the destination memory address
 */
void r2m_save_opl(thread_ctx_t *thread_ctx, ADDRINT dst)
{

	/* save EDI */
	*((uint16_t *)(bitmap + VIRT2BYTE(dst))) =
		(*((uint16_t *)(bitmap + VIRT2BYTE(dst))) & ~(LONG_MASK <<
							      VIRT2BIT(dst))) |
		((uint16_t)(thread_ctx->vcpu.gpr[0] & VCPU_MASK32) <<
		VIRT2BIT(dst));

	/* update the destination memory address */
	dst += 4;

	/* save ESI */
	*((uint16_t *)(bitmap + VIRT2BYTE(dst))) =
		(*((uint16_t *)(bitmap + VIRT2BYTE(dst))) & ~(LONG_MASK <<
							      VIRT2BIT(dst))) |
		((uint16_t)(thread_ctx->vcpu.gpr[1] & VCPU_MASK32) <<
		VIRT2BIT(dst));
	
	/* update the destination memory address */
	dst += 4;

	/* save EBP */
	*((uint16_t *)(bitmap + VIRT2BYTE(dst))) =
		(*((uint16_t *)(bitmap + VIRT2BYTE(dst))) & ~(LONG_MASK <<
							      VIRT2BIT(dst))) |
		((uint16_t)(thread_ctx->vcpu.gpr[2] & VCPU_MASK32) <<
		VIRT2BIT(dst));
	
	/* update the destination memory address */
	dst += 4;

	/* save ESP */
	*((uint16_t *)(bitmap + VIRT2BYTE(dst))) =
		(*((uint16_t *)(bitmap + VIRT2BYTE(dst))) & ~(LONG_MASK <<
							      VIRT2BIT(dst))) |
		((uint16_t)(thread_ctx->vcpu.gpr[3] & VCPU_MASK32) <<
		VIRT2BIT(dst));

	/* update the destination memory address */
	dst += 4;

	/* save EBX */
	*((uint16_t *)(bitmap + VIRT2BYTE(dst))) =
		(*((uint16_t *)(bitmap + VIRT2BYTE(dst))) & ~(LONG_MASK <<
							      VIRT2BIT(dst))) |
		((uint16_t)(thread_ctx->vcpu.gpr[4] & VCPU_MASK32) <<
		VIRT2BIT(dst));
	
	/* update the destination memory address */
	dst += 4;

	/* save EDX */
	*((uint16_t *)(bitmap + VIRT2BYTE(dst))) =
		(*((uint16_t *)(bitmap + VIRT2BYTE(dst))) & ~(LONG_MASK <<
							      VIRT2BIT(dst))) |
		((uint16_t)(thread_ctx->vcpu.gpr[5] & VCPU_MASK32) <<
		VIRT2BIT(dst));
	
	/* update the destination memory address */
	dst += 4;

	/* save ECX */
	*((uint16_t *)(bitmap + VIRT2BYTE(dst))) =
		(*((uint16_t *)(bitmap + VIRT2BYTE(dst))) & ~(LONG_MASK <<
							      VIRT2BIT(dst))) |
		((uint16_t)(thread_ctx->vcpu.gpr[6] & VCPU_MASK32) <<
		VIRT2BIT(dst));
	
	/* update the destination memory address */
	dst += 4;
	
	/* save EAX */
	*((uint16_t *)(bitmap + VIRT2BYTE(dst))) =
		(*((uint16_t *)(bitmap + VIRT2BYTE(dst))) & ~(LONG_MASK <<
							      VIRT2BIT(dst))) |
		((uint16_t)(thread_ctx->vcpu.gpr[7] & VCPU_MASK32) <<
		VIRT2BIT(dst));

}



/*
 * instruction inspection (instrumentation function)
 *
 * analyze every instruction and instrument it
 * for propagating the tag bits accordingly
 *
 * @ins:	the instruction to be instrumented
 */

void
ins_inspect(idft_ins_t* ins , idft_context_t * context)
{

    /* 
	 * temporaries;
	 * source, destination, base, and index registers
	 */
    idft_reg_t reg_dst, reg_src, reg_base, reg_indx;


    /* use XED to decode the instruction and extract its opcode */
	xed_iclass_enum_t ins_indx = (xed_iclass_enum_t)EXE->INS_Opcode(ins, context);

	/* sanity check */
	if (unlikely(ins_indx <= XED_ICLASS_INVALID || 
				ins_indx >= XED_ICLASS_LAST)) {
		
		IDFT_LOG("unknown opcode %x\n", ins_indx);

		/* done */
		return;
	}

	/* analyze the instruction */
	switch(ins_indx){
		/* adc */
		case XED_ICLASS_ADC:
		/* add */
		case XED_ICLASS_ADD:
		/* and */
		case XED_ICLASS_AND:
		/* or */
		case XED_ICLASS_OR:
		/* xor */
		case XED_ICLASS_XOR:
		/* sbb */
		case XED_ICLASS_SBB:
		/* sub */
		case XED_ICLASS_SUB:
			/*
			 * the general format of these instructions
			 * is the following: dst {op}= src, where
			 * op can be +, -, &, |, etc. We tag the
			 * destination if the source is also taged
			 * (i.e., t[dst] |= t[src])
			 */
			/* 2nd operand is immediate; do nothing */
			if (EXE->INS_OperandIsImmediate(ins, context, 1))
				break;


			/* both operands are registers */
			if (EXE->INS_MemoryOperandCount(ins, context) == 0) {
				/* extract the operands */
				reg_dst = EXE->INS_OperandReg(ins, context, 0);
				reg_src = EXE->INS_OperandReg(ins, context, 1);

				/* 32-bit operands */
				if(EXE->REG_is_gr32(ins, context, reg_dst)){
					/* check for x86 clear register idiom */
					switch(ins_indx){
						/* xor, sub, sbb */
						case XED_ICLASS_XOR:
						case XED_ICLASS_SUB:
						case XED_ICLASS_SBB:
							/* same dst, src */
							if (reg_dst == reg_src) 
							{
								/* clear */
								EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE, 
								r_clrl, 
								1, (idft_reg_t) REG32_INDX( ins, context ,reg_dst) );
								break;

							}
						/* default behavior */
						default:
							/* 
							 * propagate the tag
							 * markings accordingly
							 */
							EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE, 
							r_clrl, 
							2, (idft_reg_t) REG32_INDX(ins, context ,reg_dst) , (idft_reg_t) REG32_INDX( ins, context ,reg_src) );


					}


				}
				/* 16-bit operands */
				else if(EXE->REG_is_gr16(ins, context, reg_dst)){
					/* check for x86 clear register idiom */
					switch (ins_indx) {
						/* xor, sub, sbb */
						case XED_ICLASS_XOR:
						case XED_ICLASS_SUB:
						case XED_ICLASS_SBB:
							/* same dst, src */
							if (reg_dst == reg_src) 
							{
								/* clear */
								EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE, 
								r_clrw, 
								1, (idft_reg_t)REG16_INDX(ins, context ,reg_dst));

								/* done */
								break;								
							}
						default:
						/* propagate tags accordingly */
							EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
								r2r_binary_opw,
								2,
								(idft_reg_t)REG16_INDX(reg_dst), (idft_reg_t)REG16_INDX(reg_src)
								);						
				}
			}
			/* 8-bit operands */
			else {
					/* check for x86 clear register idiom */
					switch (ins_indx) {
						/* xor, sub, sbb */
						case XED_ICLASS_XOR:
						case XED_ICLASS_SUB:
						case XED_ICLASS_SBB:
							/* same dst, src */
							if (reg_dst == reg_src) 
							{
								/* 8-bit upper */
								if (EXE->REG_is_Upper8(ins, context, reg_dst))
									/* clear */
									EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
									r_clrb_u,
									1,
									(idft_reg_t)REG8_INDX(reg_dst)
									);
								/* 8-bit lower */
								else
									/* clear */
									EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
										r_clrb_l,
										1,
									    (idft_reg_t)REG8_INDX(reg_dst)
										);

								/* done */
								break;
							}	
						/* default behavior */
						default:
							/* propagate tags accordingly */
							EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
							r2r_binary_opb_l,
							2,
							(idft_reg_t)REG8_INDX(reg_dst),
						    (idft_reg_t)REG8_INDX(reg_src)
							);



					}
			}

	
	}

}

    






