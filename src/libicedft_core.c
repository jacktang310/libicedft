#include "libicedft_core.h"
#include "libicedft_types.h"
#include "libicedft_util.h"
#include "libicedft_api.h"
#include "tagmap.h"

// add by menertry
#include "branch_pred.h"

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
 * extend the tag as follows: t[upper(eax)] = t[ax]
 *
 * NOTE: special case for the CWDE instruction
 *
 * @thread_ctx:	the thread context
 */
void _cwde(thread_ctx_t *thread_ctx)
{
	/* temporary tag value */
	size_t src_tag = thread_ctx->vcpu.gpr[7] & VCPU_MASK16;

	/* extension; 16-bit to 32-bit */
	src_tag |= (src_tag << 2);

	/* update the destination */
	thread_ctx->vcpu.gpr[7] = src_tag;

}

/*
 * tag propagation (analysis function)
 *
 * propagate and extend tag between a 16-bit 
 * register and an 8-bit register as t[dst] = t[upper(src)]
 *
 * NOTE: special case for MOVSX instruction
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source register index (VCPU)
 */
void _movsx_r2r_opwb_u(thread_ctx_t *thread_ctx, uint32_t dst, uint32_t src)
{
	/* temporary tag value */
	size_t src_tag = thread_ctx->vcpu.gpr[src] & (VCPU_MASK8 << 1);

	/* update the destination (xfer) */
	thread_ctx->vcpu.gpr[dst] =
		(thread_ctx->vcpu.gpr[dst] & ~VCPU_MASK16) | MAP_8H_16[src_tag];

}

/*
 * tag propagation (analysis function)
 *
 * propagate and extend tag between a 16-bit 
 * register and an 8-bit register as t[dst] = t[lower(src)]
 *
 * NOTE: special case for MOVSX instruction
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source register index (VCPU)
 */
void _movsx_r2r_opwb_l(thread_ctx_t *thread_ctx, uint32_t dst, uint32_t src)
{
	/* temporary tag value */
	size_t src_tag = 
		thread_ctx->vcpu.gpr[src] & VCPU_MASK8;
	
	/* update the destination (xfer) */
	thread_ctx->vcpu.gpr[dst] =
		(thread_ctx->vcpu.gpr[dst] & ~VCPU_MASK16) | MAP_8L_16[src_tag];

}

/*
 * tag propagation (analysis function)
 *
 * propagate and extend tag between a 32-bit register
 * and an upper 8-bit register as t[dst] = t[upper(src)]
 *
 * NOTE: special case for MOVSX instruction
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source register index (VCPU)
 */
void _movsx_r2r_oplb_u(thread_ctx_t *thread_ctx, uint32_t dst, uint32_t src)
{

	/* temporary tag value */
	size_t src_tag = thread_ctx->vcpu.gpr[src] & (VCPU_MASK8 << 1);

	/* update the destination (xfer) */
	thread_ctx->vcpu.gpr[dst] = MAP_8H_32[src_tag]; 
}

/*
 * tag propagation (analysis function)
 *
 * propagate and extend tag between a 32-bit register
 * and a lower 8-bit register as t[dst] = t[lower(src)]
 *
 * NOTE: special case for MOVSX instruction
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source register index (VCPU)
 */
void _movsx_r2r_oplb_l(thread_ctx_t *thread_ctx, uint32_t dst, uint32_t src)
{
	/* temporary tag value */
	size_t src_tag = thread_ctx->vcpu.gpr[src] & VCPU_MASK8;

	/* update the destination (xfer) */
	thread_ctx->vcpu.gpr[dst] = MAP_8L_32[src_tag]; 
}

/*
 * tag propagation (analysis function)
 *
 * propagate and extend tag between a 32-bit register
 * and a 16-bit register as t[dst] = t[src]
 *
 * NOTE: special case for MOVSX instruction
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source register index (VCPU)
 */
void _movsx_r2r_oplw(thread_ctx_t *thread_ctx, uint32_t dst, uint32_t src)
{
	/* temporary tag value */
	size_t src_tag = thread_ctx->vcpu.gpr[src] & VCPU_MASK16;

	/* extension; 16-bit to 32-bit */
	src_tag |= (src_tag << 2);

	/* update the destination (xfer) */
	thread_ctx->vcpu.gpr[dst] = src_tag;
}

/*
 * tag propagation (analysis function)
 *
 * propagate and extend tag between a 16-bit 
 * register and an 8-bit memory location as
 * t[dst] = t[src]
 *
 * NOTE: special case for MOVSX instruction
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source memory address
 */
void _movsx_m2r_opwb(thread_ctx_t *thread_ctx, uint32_t dst, ADDRINT src)
{
	/* temporary tag value */
	size_t src_tag = 
		(bitmap[VIRT2BYTE(src)] >> VIRT2BIT(src)) & VCPU_MASK8;
	
	/* update the destination (xfer) */ 
	thread_ctx->vcpu.gpr[dst] =
		(thread_ctx->vcpu.gpr[dst] & ~VCPU_MASK16) | MAP_8L_16[src_tag];

}

/*
 * tag propagation (analysis function)
 *
 * propagate and extend tag between a 32-bit 
 * register and an 8-bit memory location as
 * t[dst] = t[src]
 *
 * NOTE: special case for MOVSX instruction
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source memory address
 */
void _movsx_m2r_oplb(thread_ctx_t *thread_ctx, uint32_t dst, ADDRINT src)
{
	/* temporary tag value */
	size_t src_tag = 
		(bitmap[VIRT2BYTE(src)] >> VIRT2BIT(src)) & VCPU_MASK8;
	
	/* update the destination (xfer) */
	thread_ctx->vcpu.gpr[dst] = MAP_8L_32[src_tag];

}

/*
 * tag propagation (analysis function)
 *
 * propagate and extend tag between a 32-bit register
 * and a 16-bit memory location as t[dst] = t[src]
 *
 * NOTE: special case for MOVSX instruction
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source memory address 
 */
void _movsx_m2r_oplw(thread_ctx_t *thread_ctx, uint32_t dst, ADDRINT src)
{
	/* temporary tag value */
	size_t src_tag = 
		((*((uint16_t *)(bitmap + VIRT2BYTE(src))) >> VIRT2BIT(src)) &
		VCPU_MASK16);

	/* extension; 16-bit to 32-bit */
	src_tag |= (src_tag << 2);

	/* update the destination (xfer) */
	thread_ctx->vcpu.gpr[dst] = src_tag;
}

/*
 * tag propagation (analysis function)
 *
 * propagate and extend tag between a 16-bit 
 * register and an 8-bit register as t[dst] = t[upper(src)]
 *
 * NOTE: special case for MOVZX instruction
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source register index (VCPU)
 */
void _movzx_r2r_opwb_u(thread_ctx_t *thread_ctx, uint32_t dst, uint32_t src)
{
	/* temporary tag value */
	size_t src_tag =
		(thread_ctx->vcpu.gpr[src] & (VCPU_MASK8 << 1)) >> 1;

	/* update the destination (xfer) */
	thread_ctx->vcpu.gpr[dst] =
		(thread_ctx->vcpu.gpr[dst] & ~VCPU_MASK16) | src_tag;
}

/*
 * tag propagation (analysis function)
 *
 * propagate and extend tag between a 16-bit 
 * register and an 8-bit register as t[dst] = t[lower(src)]
 *
 * NOTE: special case for MOVZX instruction
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source register index (VCPU)
 */
void _movzx_r2r_opwb_l(thread_ctx_t *thread_ctx, uint32_t dst, uint32_t src)
{
	/* temporary tag value */
	size_t src_tag = 
		thread_ctx->vcpu.gpr[src] & VCPU_MASK8;
	
	/* update the destination (xfer) */
	thread_ctx->vcpu.gpr[dst] =
		(thread_ctx->vcpu.gpr[dst] & ~VCPU_MASK16) | src_tag;

}

/*
 * tag propagation (analysis function)
 *
 * propagate and extend tag between a 32-bit register
 * and an upper 8-bit register as t[dst] = t[upper(src)]
 *
 * NOTE: special case for MOVZX instruction
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source register index (VCPU)
 */
void _movzx_r2r_oplb_u(thread_ctx_t *thread_ctx, uint32_t dst, uint32_t src)
{
	/* temporary tag value */
	size_t src_tag =
		(thread_ctx->vcpu.gpr[src] & (VCPU_MASK8 << 1)) >> 1;

	/* update the destination (xfer) */
	thread_ctx->vcpu.gpr[dst] = src_tag; 

}

/*
 * tag propagation (analysis function)
 *
 * propagate and extend tag between a 32-bit register
 * and a lower 8-bit register as t[dst] = t[lower(src)]
 *
 * NOTE: special case for MOVZX instruction
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source register index (VCPU)
 */
void _movzx_r2r_oplb_l(thread_ctx_t *thread_ctx, uint32_t dst, uint32_t src)
{
	/* temporary tag value */
	size_t src_tag = thread_ctx->vcpu.gpr[src] & VCPU_MASK8;

	/* update the destination (xfer) */
	thread_ctx->vcpu.gpr[dst] = src_tag; 

}

/*
 * tag propagation (analysis function)
 *
 * propagate and extend tag between a 32-bit register
 * and a 16-bit register as t[dst] = t[src]
 *
 * NOTE: special case for MOVZX instruction
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source register index (VCPU)
 */
void _movzx_r2r_oplw(thread_ctx_t *thread_ctx, uint32_t dst, uint32_t src)
{
	/* temporary tag value */
	size_t src_tag = thread_ctx->vcpu.gpr[src] & VCPU_MASK16;

	/* update the destination (xfer) */
	thread_ctx->vcpu.gpr[dst] = src_tag;
}

/*
 * tag propagation (analysis function)
 *
 * propagate and extend tag between a 16-bit 
 * register and an 8-bit memory location as
 * t[dst] = t[src]
 *
 * NOTE: special case for MOVZX instruction
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source memory address
 */
void _movzx_m2r_opwb(thread_ctx_t *thread_ctx, uint32_t dst, ADDRINT src)
{
	/* temporary tag value */
	size_t src_tag = 
		(bitmap[VIRT2BYTE(src)] >> VIRT2BIT(src)) & VCPU_MASK8;
	
	/* update the destination (xfer) */ 
	thread_ctx->vcpu.gpr[dst] =
		(thread_ctx->vcpu.gpr[dst] & ~VCPU_MASK16) | src_tag;

}

/*
 * tag propagation (analysis function)
 *
 * propagate and extend tag between a 32-bit 
 * register and an 8-bit memory location as
 * t[dst] = t[src]
 *
 * NOTE: special case for MOVZX instruction
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source memory address
 */
void _movzx_m2r_oplb(thread_ctx_t *thread_ctx, uint32_t dst, ADDRINT src)
{
	/* temporary tag value */
	size_t src_tag = 
		(bitmap[VIRT2BYTE(src)] >> VIRT2BIT(src)) & VCPU_MASK8;
	
	/* update the destination (xfer) */
	thread_ctx->vcpu.gpr[dst] = src_tag;
}

/*
 * tag propagation (analysis function)
 *
 * propagate and extend tag between a 32-bit register
 * and a 16-bit memory location as t[dst] = t[src]
 *
 * NOTE: special case for MOVZX instruction
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source memory address 
 */
void _movzx_m2r_oplw(thread_ctx_t *thread_ctx, uint32_t dst, ADDRINT src)
{
	/* temporary tag value */
	size_t src_tag = 
		((*((uint16_t *)(bitmap + VIRT2BYTE(src))) >> VIRT2BIT(src)) &
		VCPU_MASK16);

	/* update the destination (xfer) */
	thread_ctx->vcpu.gpr[dst] = src_tag;

}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between two 32-bit
 * registers as t[EAX] = t[src]; return
 * the result of EAX == src and also
 * store the original tag value of
 * EAX in the scratch register
 *
 * NOTE: special case for the CMPXCHG instruction
 *
 * @thread_ctx:	the thread context
 * @dst_val:	EAX register value
 * @src:	source register index (VCPU)
 * @src_val:	source register value
 */
ADDRINT _cmpxchg_r2r_opl_fast(thread_ctx_t *thread_ctx, uint32_t dst_val, uint32_t src,
							uint32_t src_val)
{

	/* save the tag value of dst in the scratch register */
	thread_ctx->vcpu.gpr[8] = 
		thread_ctx->vcpu.gpr[7];
	
	/* update */
	thread_ctx->vcpu.gpr[7] =
		thread_ctx->vcpu.gpr[src];


	/* compare the dst and src values */
	return (dst_val == src_val);
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between two 32-bit 
 * registers as t[dst] = t[src]; restore the
 * value of EAX from the scratch register
 *
 * NOTE: special case for the CMPXCHG instruction
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source register index (VCPU)
 */
void _cmpxchg_r2r_opl_slow(thread_ctx_t *thread_ctx, uint32_t dst, uint32_t src)
{
	/* restore the tag value from the scratch register */
	thread_ctx->vcpu.gpr[7] = 
		thread_ctx->vcpu.gpr[8];
	
	/* update */
	thread_ctx->vcpu.gpr[dst] =
		thread_ctx->vcpu.gpr[src];
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between two 16-bit
 * registers as t[AX] = t[src]; return
 * the result of AX == src and also
 * store the original tag value of
 * AX in the scratch register
 *
 * NOTE: special case for the CMPXCHG instruction
 *
 * @thread_ctx:	the thread context
 * @dst_val:	AX register value
 * @src:	source register index (VCPU)
 * @src_val:	source register value
 */
ADDRINT _cmpxchg_r2r_opw_fast(thread_ctx_t *thread_ctx, uint16_t dst_val, uint32_t src,
						uint16_t src_val)
{

	/* save the tag value of dst in the scratch register */
	thread_ctx->vcpu.gpr[8] = 
		thread_ctx->vcpu.gpr[7];
	
	/* update */
	thread_ctx->vcpu.gpr[7] =
		(thread_ctx->vcpu.gpr[7] & ~VCPU_MASK16) |
		(thread_ctx->vcpu.gpr[src] & VCPU_MASK16);


	/* compare the dst and src values */
	return (dst_val == src_val);
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between two 16-bit 
 * registers as t[dst] = t[src]; restore the
 * value of AX from the scratch register
 *
 * NOTE: special case for the CMPXCHG instruction
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source register index (VCPU)
 */
void _cmpxchg_r2r_opw_slow(thread_ctx_t *thread_ctx, uint32_t dst, uint32_t src)
{
	/* restore the tag value from the scratch register */
	thread_ctx->vcpu.gpr[7] = 
		thread_ctx->vcpu.gpr[8];
	
	/* update */
	thread_ctx->vcpu.gpr[dst] =
		(thread_ctx->vcpu.gpr[dst] & ~VCPU_MASK16) |
		(thread_ctx->vcpu.gpr[src] & VCPU_MASK16);

}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between a 32-bit
 * register and a memory location
 * as t[EAX] = t[src]; return the result
 * of EAX == src and also store the
 * original tag value of EAX in
 * the scratch register
 *
 * NOTE: special case for the CMPXCHG instruction
 *
 * @thread_ctx:	the thread context
 * @dst_val:	destination register value
 * @src:	source memory address
 */
ADDRINT _cmpxchg_m2r_opl_fast(thread_ctx_t *thread_ctx, uint32_t dst_val, ADDRINT src)
{
	/* save the tag value of dst in the scratch register */
	thread_ctx->vcpu.gpr[8] = 
		thread_ctx->vcpu.gpr[7];
	
	/* update */
	thread_ctx->vcpu.gpr[7] =
		(*((uint16_t *)(bitmap + VIRT2BYTE(src))) >> VIRT2BIT(src)) &
		VCPU_MASK32;
	
	/* compare the dst and src values; the original values the tag bits */
	return (dst_val == *(uint32_t *)src);
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between a 32-bit 
 * register and a memory location
 * as t[dst] = t[src]; restore the value
 * of EAX from the scratch register
 *
 * NOTE: special case for the CMPXCHG instruction
 *
 * @thread_ctx:	the thread context
 * @dst:	destination memory address
 * @src:	source register index (VCPU)
 */
void _cmpxchg_r2m_opl_slow(thread_ctx_t *thread_ctx, ADDRINT dst, uint32_t src)
{
	/* restore the tag value from the scratch register */
	thread_ctx->vcpu.gpr[7] = 
		thread_ctx->vcpu.gpr[8];
	
	/* update */
	*((uint16_t *)(bitmap + VIRT2BYTE(dst))) =
		(*((uint16_t *)(bitmap + VIRT2BYTE(dst))) & ~(LONG_MASK <<
							      VIRT2BIT(dst))) |
		((uint16_t)(thread_ctx->vcpu.gpr[src] & VCPU_MASK32) <<
		VIRT2BIT(dst));
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between a 16-bit
 * register and a memory location
 * as t[AX] = t[src]; return the result
 * of AX == src and also store the
 * original tag value of AX in
 * the scratch register
 *
 * NOTE: special case for the CMPXCHG instruction
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @dst_val:	destination register value
 * @src:	source memory address
 */
ADDRINT _cmpxchg_m2r_opw_fast(thread_ctx_t *thread_ctx, uint16_t dst_val, ADDRINT src)
{

	/* save the tag value of dst in the scratch register */
	thread_ctx->vcpu.gpr[8] = 
		thread_ctx->vcpu.gpr[7];
	
	/* update */
	thread_ctx->vcpu.gpr[7] =
		(thread_ctx->vcpu.gpr[7] & ~VCPU_MASK16) |
		((*((uint16_t *)(bitmap + VIRT2BYTE(src))) >> VIRT2BIT(src)) &
		VCPU_MASK16);
	
	/* compare the dst and src values; the original values the tag bits */
	return (dst_val == *(uint16_t *)src);
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between a 16-bit 
 * register and a memory location
 * as t[dst] = t[src]; restore the value
 * of AX from the scratch register
 *
 * NOTE: special case for the CMPXCHG instruction
 *
 * @thread_ctx:	the thread context
 * @dst:	destination memory address
 * @src:	source register index (VCPU)
 * @res:	restore register index (VCPU)
 */
void _cmpxchg_r2m_opw_slow(thread_ctx_t *thread_ctx, ADDRINT dst, uint32_t src)
{

	/* restore the tag value from the scratch register */
	thread_ctx->vcpu.gpr[7] = 
		thread_ctx->vcpu.gpr[8];
	
	/* update */
	*((uint16_t *)(bitmap + VIRT2BYTE(dst))) =
		(*((uint16_t *)(bitmap + VIRT2BYTE(dst))) & ~(WORD_MASK <<
							      VIRT2BIT(dst))) |
		((uint16_t)(thread_ctx->vcpu.gpr[src] & VCPU_MASK16) <<
		VIRT2BIT(dst));

}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between two 8-bit 
 * registers as t[upper(dst)] = t[lower(src)]
 * and t[lower(src)] = t[upper(dst)]
 *
 * NOTE: special case for the XCHG instruction
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source register index (VCPU)
 */
void _xchg_r2r_opb_ul(thread_ctx_t *thread_ctx, uint32_t dst, uint32_t src)
{
	/* temporary tag value */
	size_t tmp_tag = thread_ctx->vcpu.gpr[dst] & (VCPU_MASK8 << 1);

	/* swap */
	thread_ctx->vcpu.gpr[dst] =
		 (thread_ctx->vcpu.gpr[dst] & ~(VCPU_MASK8 << 1)) |
		 ((thread_ctx->vcpu.gpr[src] & VCPU_MASK8) << 1);
	
	thread_ctx->vcpu.gpr[src] =
		 (thread_ctx->vcpu.gpr[src] & ~VCPU_MASK8) | (tmp_tag >> 1);
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between two 8-bit 
 * registers as t[lower(dst)] = t[upper(src)]
 * and t[upper(src)] = t[lower(dst)]
 *
 * NOTE: special case for the XCHG instruction
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source register index (VCPU)
 */
void _xchg_r2r_opb_lu(thread_ctx_t *thread_ctx, uint32_t dst, uint32_t src)
{
	/* temporary tag value */
	size_t tmp_tag = thread_ctx->vcpu.gpr[dst] & VCPU_MASK8;

	/* swap */
	thread_ctx->vcpu.gpr[dst] =
		(thread_ctx->vcpu.gpr[dst] & ~VCPU_MASK8) | 
		((thread_ctx->vcpu.gpr[src] & (VCPU_MASK8 << 1)) >> 1);
	
	thread_ctx->vcpu.gpr[src] =
	 (thread_ctx->vcpu.gpr[src] & ~(VCPU_MASK8 << 1)) | (tmp_tag << 1);

}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between two 8-bit 
 * registers as t[upper(dst)] = t[upper(src)]
 * and t[upper(src)] = t[upper(dst)]
 *
 * NOTE: special case for the XCHG instruction
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source register index (VCPU)
 */
void _xchg_r2r_opb_u(thread_ctx_t *thread_ctx, uint32_t dst, uint32_t src)
{

	/* temporary tag value */
	size_t tmp_tag = thread_ctx->vcpu.gpr[dst] & (VCPU_MASK8 << 1);

	/* swap */
	thread_ctx->vcpu.gpr[dst] =
		(thread_ctx->vcpu.gpr[dst] & ~(VCPU_MASK8 << 1)) |
		(thread_ctx->vcpu.gpr[src] & (VCPU_MASK8 << 1));
	
	thread_ctx->vcpu.gpr[src] =
		(thread_ctx->vcpu.gpr[src] & ~(VCPU_MASK8 << 1)) | tmp_tag;

}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between two 8-bit 
 * registers as t[lower(dst)] = t[lower(src)]
 * and t[lower(src)] = t[lower(dst)]
 *
 * NOTE: special case for the XCHG instruction
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source register index (VCPU)
 */
void _xchg_r2r_opb_l(thread_ctx_t *thread_ctx, uint32_t dst, uint32_t src)
{
	/* temporary tag value */
	size_t tmp_tag = thread_ctx->vcpu.gpr[dst] & VCPU_MASK8; 

	/* swap */
	thread_ctx->vcpu.gpr[dst] =
		(thread_ctx->vcpu.gpr[dst] & ~VCPU_MASK8) |
		(thread_ctx->vcpu.gpr[src] & VCPU_MASK8);
	
	thread_ctx->vcpu.gpr[src] =
		(thread_ctx->vcpu.gpr[src] & ~VCPU_MASK8) | tmp_tag;
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between two 16-bit 
 * registers as t[dst] = t[src]
 * and t[src] = t[dst]
 *
 * NOTE: special case for the XCHG instruction
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source register index (VCPU)
 */
void _xchg_r2r_opw(thread_ctx_t *thread_ctx, uint32_t dst, uint32_t src)
{
	/* temporary tag value */
	size_t tmp_tag = thread_ctx->vcpu.gpr[dst] & VCPU_MASK16; 

	/* swap */
	thread_ctx->vcpu.gpr[dst] =
		(thread_ctx->vcpu.gpr[dst] & ~VCPU_MASK16) |
		(thread_ctx->vcpu.gpr[src] & VCPU_MASK16);
	
	thread_ctx->vcpu.gpr[src] =
		(thread_ctx->vcpu.gpr[src] & ~VCPU_MASK16) | tmp_tag;
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between an upper 8-bit 
 * register and a memory location as
 * t[dst] = t[src] and t[src] = t[dst]
 * (dst is a register)
 *
 * NOTE: special case for the XCHG instruction
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source memory address
 */
void _xchg_m2r_opb_u(thread_ctx_t *thread_ctx, uint32_t dst, ADDRINT src)
{
	/* temporary tag value */
	size_t tmp_tag = thread_ctx->vcpu.gpr[dst] & (VCPU_MASK8 << 1);

	/* swap */
	thread_ctx->vcpu.gpr[dst] =
		(thread_ctx->vcpu.gpr[dst] & ~(VCPU_MASK8 << 1)) |
		(((bitmap[VIRT2BYTE(src)] >> VIRT2BIT(src)) << 1) &
		(VCPU_MASK8 << 1));

	bitmap[VIRT2BYTE(src)] =
		(bitmap[VIRT2BYTE(src)] & ~(BYTE_MASK << VIRT2BIT(src))) |
		((tmp_tag >> 1) << VIRT2BIT(src));
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between a lower 8-bit 
 * register and a memory location as
 * t[dst] = t[src] and t[src] = t[dst]
 * (dst is a register)
 *
 * NOTE: special case for the XCHG instruction
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source memory address
 */
void _xchg_m2r_opb_l(thread_ctx_t *thread_ctx, uint32_t dst, ADDRINT src)
{
	/* temporary tag value */
	size_t tmp_tag = thread_ctx->vcpu.gpr[dst] & VCPU_MASK8;
	
	/* swap */
	thread_ctx->vcpu.gpr[dst] =
		(thread_ctx->vcpu.gpr[dst] & ~VCPU_MASK8) |
		((bitmap[VIRT2BYTE(src)] >> VIRT2BIT(src)) & VCPU_MASK8);
	
	bitmap[VIRT2BYTE(src)] =
		(bitmap[VIRT2BYTE(src)] & ~(BYTE_MASK << VIRT2BIT(src))) |
		(tmp_tag << VIRT2BIT(src));

}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between a 16-bit 
 * register and a memory location as
 * t[dst] = t[src] and t[src] = t[dst]
 * (dst is a register)
 *
 * NOTE: special case for the XCHG instruction
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source memory address
 */
void _xchg_m2r_opw(thread_ctx_t *thread_ctx, uint32_t dst, ADDRINT src)
{
	/* temporary tag value */
	size_t tmp_tag = thread_ctx->vcpu.gpr[dst] & VCPU_MASK16;

	/* swap */	
	thread_ctx->vcpu.gpr[dst] =
		(thread_ctx->vcpu.gpr[dst] & ~VCPU_MASK16) |
		((*((uint16_t *)(bitmap + VIRT2BYTE(src))) >> VIRT2BIT(src)) &
		VCPU_MASK16);

	*((uint16_t *)(bitmap + VIRT2BYTE(src))) =
		(*((uint16_t *)(bitmap + VIRT2BYTE(src))) & ~(WORD_MASK <<
							      VIRT2BIT(src))) |
		((uint16_t)(tmp_tag) < VIRT2BIT(src));

}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between a 32-bit 
 * register and a memory location as
 * t[dst] = t[src] and t[src] = t[dst]
 * (dst is a register)
 *
 * NOTE: special case for the XCHG instruction
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source memory address
 */
void _xchg_m2r_opl(thread_ctx_t *thread_ctx, uint32_t dst, ADDRINT src)
{
	/* temporary tag value */
	size_t tmp_tag = thread_ctx->vcpu.gpr[dst];
	
	/* swap */
	thread_ctx->vcpu.gpr[dst] =
		(*((uint16_t *)(bitmap + VIRT2BYTE(src))) >> VIRT2BIT(src)) &
		VCPU_MASK32;
	
	*((uint16_t *)(bitmap + VIRT2BYTE(src))) =
		(*((uint16_t *)(bitmap + VIRT2BYTE(src))) & ~(LONG_MASK <<
							      VIRT2BIT(src))) |
		((uint16_t)(tmp_tag) << VIRT2BIT(src));

}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between two 8-bit 
 * registers as t[upper(dst)] |= t[lower(src)]
 * and t[lower(src)] = t[upper(dst)]
 *
 * NOTE: special case for the XADD instruction
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source register index (VCPU)
 */
void _xadd_r2r_opb_ul(thread_ctx_t *thread_ctx, uint32_t dst, uint32_t src)
{
	/* temporary tag value */
	size_t tmp_tag = thread_ctx->vcpu.gpr[dst] & (VCPU_MASK8 << 1);

	/* swap */
	thread_ctx->vcpu.gpr[dst] =
		 (thread_ctx->vcpu.gpr[dst] & (VCPU_MASK8 << 1)) |
		 ((thread_ctx->vcpu.gpr[src] & VCPU_MASK8) << 1);
	
	thread_ctx->vcpu.gpr[src] =
		 (thread_ctx->vcpu.gpr[src] & ~VCPU_MASK8) | (tmp_tag >> 1);
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between two 8-bit 
 * registers as t[lower(dst)] |= t[upper(src)]
 * and t[upper(src)] = t[lower(dst)]
 *
 * NOTE: special case for the XADD instruction
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source register index (VCPU)
 */
void _xadd_r2r_opb_lu(thread_ctx_t *thread_ctx, uint32_t dst, uint32_t src)
{
	/* temporary tag value */
	size_t tmp_tag = thread_ctx->vcpu.gpr[dst] & VCPU_MASK8;

	/* swap */
	thread_ctx->vcpu.gpr[dst] =
		(thread_ctx->vcpu.gpr[dst] & VCPU_MASK8) | 
		((thread_ctx->vcpu.gpr[src] & (VCPU_MASK8 << 1)) >> 1);
	
	thread_ctx->vcpu.gpr[src] =
	 (thread_ctx->vcpu.gpr[src] & ~(VCPU_MASK8 << 1)) | (tmp_tag << 1);
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between two 8-bit 
 * registers as t[upper(dst)] |= t[upper(src)]
 * and t[upper(src)] = t[upper(dst)]
 *
 * NOTE: special case for the XADD instruction
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source register index (VCPU)
 */
void _xadd_r2r_opb_u(thread_ctx_t *thread_ctx, uint32_t dst, uint32_t src)
{
	/* temporary tag value */
	size_t tmp_tag = thread_ctx->vcpu.gpr[dst] & (VCPU_MASK8 << 1);

	/* swap */
	thread_ctx->vcpu.gpr[dst] =
		(thread_ctx->vcpu.gpr[dst] & (VCPU_MASK8 << 1)) |
		(thread_ctx->vcpu.gpr[src] & (VCPU_MASK8 << 1));
	
	thread_ctx->vcpu.gpr[src] =
		(thread_ctx->vcpu.gpr[src] & ~(VCPU_MASK8 << 1)) | tmp_tag;

}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between two 8-bit 
 * registers as t[lower(dst)] |= t[lower(src)]
 * and t[lower(src)] = t[lower(dst)]
 *
 * NOTE: special case for the XADD instruction
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source register index (VCPU)
 */
void _xadd_r2r_opb_l(thread_ctx_t *thread_ctx, uint32_t dst, uint32_t src)
{
	/* temporary tag value */
	size_t tmp_tag = thread_ctx->vcpu.gpr[dst] & VCPU_MASK8; 

	/* swap */
	thread_ctx->vcpu.gpr[dst] =
		(thread_ctx->vcpu.gpr[dst] & VCPU_MASK8) |
		(thread_ctx->vcpu.gpr[src] & VCPU_MASK8);
	
	thread_ctx->vcpu.gpr[src] =
		(thread_ctx->vcpu.gpr[src] & ~VCPU_MASK8) | tmp_tag;

}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between two 16-bit 
 * registers as t[dst] |= t[src]
 * and t[src] = t[dst]
 *
 * NOTE: special case for the XADD instruction
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source register index (VCPU)
 */
void _xadd_r2r_opw(thread_ctx_t *thread_ctx, uint32_t dst, uint32_t src)
{
	/* temporary tag value */
	size_t tmp_tag = thread_ctx->vcpu.gpr[dst] & VCPU_MASK16; 

	/* swap */
	thread_ctx->vcpu.gpr[dst] =
		(thread_ctx->vcpu.gpr[dst] & VCPU_MASK16) |
		(thread_ctx->vcpu.gpr[src] & VCPU_MASK16);
	
	thread_ctx->vcpu.gpr[src] =
		(thread_ctx->vcpu.gpr[src] & ~VCPU_MASK16) | tmp_tag;

}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between an upper 8-bit 
 * register and a memory location as
 * t[dst] |= t[src] and t[src] = t[dst]
 * (dst is a register)
 *
 * NOTE: special case for the XADD instruction
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source memory address
 */
void _xadd_m2r_opb_u(thread_ctx_t *thread_ctx, uint32_t dst, ADDRINT src)
{
	/* temporary tag value */
	size_t tmp_tag = thread_ctx->vcpu.gpr[dst] & (VCPU_MASK8 << 1);

	/* swap */
	thread_ctx->vcpu.gpr[dst] =
		(thread_ctx->vcpu.gpr[dst] & (VCPU_MASK8 << 1)) |
		(((bitmap[VIRT2BYTE(src)] >> VIRT2BIT(src)) << 1) &
		(VCPU_MASK8 << 1));

	bitmap[VIRT2BYTE(src)] =
		(bitmap[VIRT2BYTE(src)] & ~(BYTE_MASK << VIRT2BIT(src))) |
		((tmp_tag >> 1) << VIRT2BIT(src));

}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between a lower 8-bit 
 * register and a memory location as
 * t[dst] |= t[src] and t[src] = t[dst]
 * (dst is a register)
 *
 * NOTE: special case for the XADD instruction
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source memory address
 */
void _xadd_m2r_opb_l(thread_ctx_t *thread_ctx, uint32_t dst, ADDRINT src)
{
	/* temporary tag value */
	size_t tmp_tag = thread_ctx->vcpu.gpr[dst] & VCPU_MASK8;
	
	/* swap */
	thread_ctx->vcpu.gpr[dst] =
		(thread_ctx->vcpu.gpr[dst] & VCPU_MASK8) |
		((bitmap[VIRT2BYTE(src)] >> VIRT2BIT(src)) & VCPU_MASK8);
	
	bitmap[VIRT2BYTE(src)] =
		(bitmap[VIRT2BYTE(src)] & ~(BYTE_MASK << VIRT2BIT(src))) |
		(tmp_tag << VIRT2BIT(src));

}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between a 16-bit 
 * register and a memory location as
 * t[dst] |= t[src] and t[src] = t[dst]
 * (dst is a register)
 *
 * NOTE: special case for the XADD instruction
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source memory address
 */
void _xadd_m2r_opw(thread_ctx_t *thread_ctx, uint32_t dst, ADDRINT src)
{
	/* temporary tag value */
	size_t tmp_tag = thread_ctx->vcpu.gpr[dst] & VCPU_MASK16;

	/* swap */	
	thread_ctx->vcpu.gpr[dst] =
		(thread_ctx->vcpu.gpr[dst] & VCPU_MASK16) |
		((*((uint16_t *)(bitmap + VIRT2BYTE(src))) >> VIRT2BIT(src)) &
		VCPU_MASK16);

	*((uint16_t *)(bitmap + VIRT2BYTE(src))) =
		(*((uint16_t *)(bitmap + VIRT2BYTE(src))) & ~(WORD_MASK <<
							      VIRT2BIT(src))) |
		((uint16_t)(tmp_tag) < VIRT2BIT(src));
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between a 32-bit 
 * register and a memory location as
 * t[dst] |= t[src] and t[src] = t[dst]
 * (dst is a register)
 *
 * NOTE: special case for the XADD instruction
 *
 * @thread_ctx:	the thread context
 * @dst:	destination register index (VCPU)
 * @src:	source memory address
 */
void _xadd_m2r_opl(thread_ctx_t *thread_ctx, uint32_t dst, ADDRINT src)
{
	/* temporary tag value */
	size_t tmp_tag = thread_ctx->vcpu.gpr[dst];
	
	/* swap */
	thread_ctx->vcpu.gpr[dst] =
		(*((uint16_t *)(bitmap + VIRT2BYTE(src))) >> VIRT2BIT(src)) &
		VCPU_MASK32;
	
	*((uint16_t *)(bitmap + VIRT2BYTE(src))) =
		(*((uint16_t *)(bitmap + VIRT2BYTE(src))) & (LONG_MASK <<
							      VIRT2BIT(src))) |
		((uint16_t)(tmp_tag) << VIRT2BIT(src));
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between three 16-bit 
 * registers as t[dst] = t[base] | t[index]
 *
 * NOTE: special case for the LEA instruction
 *
 * @thread_ctx: the thread context
 * @dst:        destination register
 * @base:       base register
 * @index:      index register
 */
void _lea_r2r_opw(thread_ctx_t *thread_ctx,
		uint32_t dst,
		uint32_t base,
		uint32_t index)
{
	/* update the destination */
	thread_ctx->vcpu.gpr[dst] =
		((thread_ctx->vcpu.gpr[dst] & ~VCPU_MASK16) |
		(thread_ctx->vcpu.gpr[base] & VCPU_MASK16) |
		(thread_ctx->vcpu.gpr[index] & VCPU_MASK16));
}

/*
 * tag propagation (analysis function)
 *
 * propagate tag between three 32-bit 
 * registers as t[dst] = t[base] | t[index]
 *
 * NOTE: special case for the LEA instruction
 *
 * @thread_ctx: the thread context
 * @dst:        destination register
 * @base:       base register
 * @index:      index register
 */
void _lea_r2r_opl(thread_ctx_t *thread_ctx,
		uint32_t dst,
		uint32_t base,
		uint32_t index)
{
	/* update the destination */
	thread_ctx->vcpu.gpr[dst] =
		thread_ctx->vcpu.gpr[base] | thread_ctx->vcpu.gpr[index];
}


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
								3,
								IARG_THREAD_CONTEXT,
								IARG_UINT32, 
								(uint32_t) REG32_INDX( ins, context ,reg_dst) );
								break;

							}
						/* default behavior */
						default:
							/* 
							 * propagate the tag
							 * markings accordingly
							 */
							EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE, 
							r2r_binary_opl, 
							5,
							IARG_THREAD_CONTEXT,
							IARG_UINT32,
							(uint32_t) REG32_INDX(ins, context ,reg_dst),
							IARG_UINT32,
							(uint32_t) REG32_INDX( ins, context ,reg_src) 
							);


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
								3,
								IARG_THREAD_CONTEXT,
								IARG_UINT32, 
								(uint32_t)REG16_INDX(ins, context ,reg_dst)
								);
								/* done */
								break;								
							}
						default:
						/* propagate tags accordingly */
							EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
								r2r_binary_opw, 
								5,
								IARG_THREAD_CONTEXT,
								IARG_UINT32,
								(uint32_t)REG16_INDX(ins, context, reg_dst),
								IARG_UINT32,
								(uint32_t)REG16_INDX(ins, context, reg_src)
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
									3,
									IARG_THREAD_CONTEXT,
									IARG_UINT32,
									(uint32_t)REG8_INDX(ins, context, reg_dst)
									);
								/* 8-bit lower */
								else
									/* clear */
									EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
										r_clrb_l, 
										3,
										IARG_THREAD_CONTEXT,
										IARG_UINT32,
									    (idft_reg_t)REG8_INDX(ins, context, reg_dst)
										);

								/* done */
								break;
							}	
						/* default behavior */
						default:
							/* propagate tags accordingly */
							if(EXE->REG_is_Lower8(ins, context, reg_dst) &&
									EXE->REG_is_Lower8(ins, context, reg_src))
								/* lower 8-bit registers */
								EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
									r2r_binary_opb_l,
									5,
									IARG_THREAD_CONTEXT,
									IARG_UINT32,
									(uint32_t)REG8_INDX(ins, context, reg_dst),
									IARG_UINT32,
						    		(uint32_t)REG8_INDX(ins, context, reg_src)
									);
							else if (EXE->REG_is_Upper8(ins, context, reg_dst) &&
									EXE->REG_is_Upper8(ins, context, reg_src))
								/* upper 8-bit registers */
								EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
									r2r_binary_opb_u, 
									5,
									IARG_THREAD_CONTEXT,
									IARG_UINT32,
									(uint32_t)REG8_INDX(ins, context, reg_dst),
									IARG_UINT32,
						            (uint32_t)REG8_INDX(ins, context, reg_src)
									);	
							else if (EXE->REG_is_Lower8(ins, context, reg_dst))
								/* 
						 		* destination register is a
						 		* lower 8-bit register and
						 		* source register is an upper
						 		* 8-bit register
						 		*/
								EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE, 
									r2r_binary_opb_lu, 
									5,
									IARG_THREAD_CONTEXT,
									IARG_UINT32,
									(uint32_t)REG8_INDX( ins, context, reg_dst),
									IARG_UINT32,
									(uint32_t)REG8_INDX(ins, context, reg_src)
									); 
							else
								/* 
						 		* destination register is an
						 		* upper 8-bit register and
						 		* source register is a lower
						 		* 8-bit register
						 		*/
								EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
									r2r_binary_opb_ul, 
									5,
									IARG_THREAD_CONTEXT,
									IARG_UINT32,
									(uint32_t)REG8_INDX(ins, context, reg_dst),
									IARG_UINT32,
						            (uint32_t)REG8_INDX(ins, context, reg_src)
									);
					}
			}

		}
		/* 
		* 2nd operand is memory;
		* we optimize for that case, since most
		* instructions will have a register as
		* the first operand -- leave the result
		* into the reg and use it later
		*/
		else if (EXE->INS_OperandIsMemory(ins, context, 1)) {
			/* extract the register operand */
			reg_dst = EXE->INS_OperandReg(ins, context, 0);

			/* 32-bit operands */
			if (EXE->REG_is_gr32(ins, context, reg_dst))
				/* propagate the tag accordingly */
				EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
						m2r_binary_opl, 
						4,
						IARG_THREAD_CONTEXT,
						IARG_UINT32,
						(uint32_t)REG32_INDX(ins, context, reg_dst),
						IARG_MEMORYREAD_EA
						);
			/* 16-bit operands */
			else if (EXE->REG_is_gr16(ins, context,reg_dst))
				/* propagate the tag accordingly */
				EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
						m2r_binary_opw,
						4,
						IARG_THREAD_CONTEXT,
						IARG_UINT32,
						(uint32_t)REG16_INDX(ins, context, reg_dst),
						IARG_MEMORYREAD_EA
						);
			/* 8-bit operand (upper) */
			else if (EXE->REG_is_Upper8(ins, context,reg_dst))
				/* propagate the tag accordingly */
				EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
					m2r_binary_opb_u,
					4, 
					IARG_THREAD_CONTEXT,
					IARG_UINT32,
					(uint32_t)REG8_INDX(ins, context,reg_dst),
					IARG_MEMORYREAD_EA
					);
			/* 8-bit operand (lower) */
			else
				/* propagate the tag accordingly */
				EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
						m2r_binary_opb_l,
						4, 
						IARG_THREAD_CONTEXT,
						IARG_UINT32,
						(uint32_t)REG8_INDX(ins, context, reg_dst),
						IARG_MEMORYREAD_EA
						);
		
		}
		/* 1st operand is memory */
		else {
			/* extract the register operand */
			reg_src = EXE->INS_OperandReg(ins, context, 1);

			/* 32-bit operands */
			if (EXE->REG_is_gr32(ins, context, reg_src))
				/* propagate the tag accordingly */
				EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
						r2m_binary_opl,
						4, 
						IARG_THREAD_CONTEXT,
						IARG_MEMORYWRITE_EA,
						IARG_UINT32,
						(uint32_t)REG32_INDX(ins, context, reg_src)
						);
			/* 16-bit operands */
			else if (EXE->REG_is_gr16(ins, context,reg_src))
				/* propagate the tag accordingly */
				EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
						r2m_binary_opw,
						4,
						IARG_THREAD_CONTEXT,
						IARG_MEMORYWRITE_EA, 
						IARG_UINT32,
						(uint32_t)REG16_INDX(ins, context, reg_src)
						);
			/* 8-bit operand (upper) */
			else if (EXE->REG_is_Upper8(ins, context,reg_src))
				/* propagate the tag accordingly */
				EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
						r2m_binary_opb_u,
						4,
						IARG_THREAD_CONTEXT, 
						IARG_MEMORYWRITE_EA,
						IARG_UINT32,
						(uint32_t)REG8_INDX(ins, context, reg_src)
						);
			/* 8-bit operand (lower) */
			else
				/* propagate the tag accordingly */
				EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
						r2m_binary_opb_l,
						4, 
						IARG_THREAD_CONTEXT,
						IARG_MEMORYWRITE_EA,
						IARG_UINT32,
						(uint32_t)REG8_INDX(ins, context, reg_src)
						);
		}

		/* done */
		break;
	
		/* bsf */
		case XED_ICLASS_BSF:
		/* bsr */
		case XED_ICLASS_BSR:
		/* mov */
		case XED_ICLASS_MOV:
			/*
			 * the general format of these instructions
			 * is the following: dst = src. We move the
			 * tag of the source to the destination
			 * (i.e., t[dst] = t[src])
			 */
			/* 
			 * 2nd operand is immediate or segment register;
			 * clear the destination
			 *
			 * NOTE: When the processor moves a segment register
			 * into a 32-bit general-purpose register, it assumes
			 * that the 16 least-significant bits of the
			 * general-purpose register are the destination or
			 * source operand. If the register is a destination
			 * operand, the resulting value in the two high-order
			 * bytes of the register is implementation dependent.
			 * For the Pentium 4, Intel Xeon, and P6 family
			 * processors, the two high-order bytes are filled with
			 * zeros; for earlier 32-bit IA-32 processors, the two
			 * high order bytes are undefined.
			 */
			if (EXE->INS_OperandIsImmediate(ins, context, 1) ||
				(EXE->INS_OperandIsReg(ins, context, 1) &&
				EXE->REG_is_seg( ins, context, EXE->INS_OperandReg(ins, context, 1)))) {
					/* destination operand is a memory address */
					if (EXE->INS_OperandIsMemory(ins, context, 0)) {
						/* clear n-bytes */
						switch (EXE->INS_OperandWidth(ins, context, 0)) {
							/* 4 bytes */
							case MEM_LONG_LEN:
								/* propagate the tag accordingly */
								EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE, 
									tagmap_clrl,
									1,
									IARG_MEMORYWRITE_EA
									);
								break;
							/* 2 bytes */
							case MEM_WORD_LEN:
								/* propagate the tag accordingly */
								EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
								tagmap_clrw, 
								1,
								IARG_MEMORYWRITE_EA
								);

								/* done */
								break;
							/* 1 byte */
							case MEM_BYTE_LEN:
								/* propagate the tag accordingly */
								// modify by menertry
								EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
									tagmap_clrb,
									1,
									IARG_MEMORYWRITE_EA
								);
								/* done */
								break;
							default:
								IDFT_LOG("unhandled operand： %s\n",EXE->INS_Disassemble(ins, context) );
								/* done */
								return;
						}

					}
					/* destination operand is a register */
					else if (EXE->INS_OperandIsReg(ins, context, 0)) {
						/* extract the operand */
						reg_dst = EXE->INS_OperandReg(ins, context, 0);

						/* 32-bit operand */
						if (EXE->REG_is_gr32(ins, context,reg_dst))
						/* propagate the tag accordingly */
							EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
							r_clrl,
							3,
							IARG_THREAD_CONTEXT,
							IARG_UINT32, 
							(uint32_t)REG32_INDX( ins, context, reg_dst)
							);
						/* 16-bit operand */
						else if (EXE->REG_is_gr16(ins, context, reg_dst))
						/* propagate the tag accordingly */
							// modify by menertry
							EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
							r_clrw,
							3,
							IARG_THREAD_CONTEXT,
							IARG_UINT32, 
							(uint32_t)REG16_INDX(ins, context, reg_dst)
							);
						/* 8-bit operand (upper) */
						// modify by menertry
						else if (EXE->REG_is_Upper8(ins, context, reg_dst))
							/* propagate the tag accordingly */
							// modify by menertry
							EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
								r_clrb_u,
								3,
								IARG_THREAD_CONTEXT,
								IARG_UINT32,
								(uint32_t)REG8_INDX(ins, context,reg_dst)
							);
						/* 8-bit operand (lower) */
						else
						/* propagate the tag accordingly */
							// modify by menertry
							EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
							r_clrb_l,
							3,
							IARG_THREAD_CONTEXT,
							IARG_UINT32, 
							(uint32_t)REG8_INDX(ins, context, reg_dst)
							);
					}
				}
				/* both operands are registers */
				else if (EXE->INS_MemoryOperandCount(ins, context) == 0) {
					/* extract the operands */
					reg_dst = EXE->INS_OperandReg(ins, context,  0);
					reg_src = EXE->INS_OperandReg(ins, context, 1);

					/* 32-bit operands */
					if (EXE->REG_is_gr32(ins, context, reg_dst))
						/* propagate the tag accordingly */
						EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
							r2r_xfer_opl,
							5,
							IARG_THREAD_CONTEXT,
							IARG_UINT32, 
							(uint32_t)REG32_INDX(ins, context, reg_dst),
							IARG_UINT32, 
							(uint32_t)REG32_INDX(ins, context, reg_src)
							);
					/* 16-bit operands */
					else if (EXE->REG_is_gr16(ins, context, reg_dst))
						/* propagate tag accordingly */
						EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
							r2r_xfer_opw,
							5,
							IARG_THREAD_CONTEXT,
							IARG_UINT32, 
							(uint32_t)REG16_INDX(ins, context,reg_dst),
							IARG_UINT32, 
							(uint32_t)REG16_INDX(ins, context,reg_src)
							);
					/* 8-bit operands */
					else if (EXE->REG_is_gr8(ins, context, reg_dst)) {
						/* propagate tag accordingly */
						if (EXE->REG_is_Lower8(ins, context, reg_dst) &&
							EXE->REG_is_Lower8(ins, context, reg_src))
							/* lower 8-bit registers */
							// modify by menertry
							EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
							r2r_xfer_opb_l,
							5,
							IARG_THREAD_CONTEXT,
							IARG_UINT32, 
							(uint32_t)REG8_INDX( ins, context, reg_dst),
							IARG_UINT32, 
							(uint32_t) REG8_INDX(ins, context, reg_src)
							);
						else if(EXE->REG_is_Upper8(ins, context,  reg_dst) &&
							EXE->REG_is_Upper8(ins, context, reg_src))
							/* upper 8-bit registers */
							// modify by menertry
							EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
							r2r_xfer_opb_u,
							5,
						 	IARG_THREAD_CONTEXT,
							IARG_UINT32, 
							(uint32_t)REG8_INDX(ins, context, reg_dst),
							IARG_UINT32, 
							(uint32_t)REG8_INDX(ins, context, reg_src)
							);
						// modify by menertry
						else if (EXE->REG_is_Lower8(ins, context, reg_dst))
							/* 
						 	* destination register is a
						 	* lower 8-bit register and
						 	* source register is an upper
						 	* 8-bit register
						 	*/
							EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
							r2r_xfer_opb_lu,
							5, 
							IARG_THREAD_CONTEXT,
							IARG_UINT32, 
							(uint32_t)REG8_INDX(ins, context,reg_dst),
							IARG_UINT32, 
							(uint32_t)REG8_INDX(ins, context,reg_src)
							);
						else
							/* 
						 	* destination register is an
						 	* upper 8-bit register and
						 	* source register is a lower
						 	* 8-bit register
						 	*/	

							EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
							r2r_xfer_opb_ul,
							5,
							IARG_THREAD_CONTEXT,
							IARG_UINT32,
							(uint32_t)REG8_INDX(ins, context,reg_dst),
							IARG_UINT32, 
							(uint32_t)REG8_INDX(ins, context, reg_src)
							);

					}

				}
				/* 
			 	* 2nd operand is memory;
			 	* we optimize for that case, since most
			 	* instructions will have a register as
			 	* the first operand -- leave the result
			 	* into the reg and use it later
			 	*/
				else if (EXE->INS_OperandIsMemory(ins, context,1)) { 
					/* extract the register operand */
					reg_dst = EXE->INS_OperandReg(ins, context, 0);

					/* 32-bit operands */
					if (EXE->REG_is_gr32(ins, context, reg_dst))
						/* propagate the tag accordingly */
						EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
						m2r_xfer_opl,
						4, 
						IARG_THREAD_CONTEXT,
						IARG_UINT32, 
						(uint32_t)REG32_INDX(ins, context, reg_dst),
						IARG_MEMORYREAD_EA
						);
					/* 16-bit operands */
					else if (EXE->REG_is_gr16(ins, context, reg_dst))
						/* propagate the tag accordingly */
						EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
						m2r_xfer_opw,
						4,
						IARG_THREAD_CONTEXT,
						IARG_UINT32, 
						(uint32_t)REG16_INDX(ins, context, reg_dst),
						IARG_MEMORYREAD_EA
						);
					/* 8-bit operands (upper) */
					// modify by menertry
					else if (EXE->REG_is_Upper8(ins, context, reg_dst)) 
						/* propagate the tag accordingly */
						EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
						m2r_xfer_opb_u,
						4, 
						IARG_THREAD_CONTEXT,
						IARG_UINT32, 
						(uint32_t)REG8_INDX(ins, context, reg_dst),
						IARG_MEMORYREAD_EA
						);
					/* 8-bit operands (lower) */
					else
						/* propagate the tag accordingly */
						EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
						m2r_xfer_opb_l,
						4,
						IARG_THREAD_CONTEXT,
						IARG_UINT32, 
						(uint32_t)REG8_INDX(ins, context,reg_dst),
						IARG_MEMORYREAD_EA
						);
				}
				/* 1st operand is memory */
				else {
					/* extract the register operand */
					reg_src = EXE->INS_OperandReg(ins, context, 1);

					/* 32-bit operands */
					if (EXE->REG_is_gr32(ins, context, reg_src))
						/* propagate the tag accordingly */
						EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
						r2m_xfer_opl,
						4, 
						IARG_THREAD_CONTEXT,
						IARG_MEMORYWRITE_EA,
						IARG_UINT32, 
						(uint32_t)REG32_INDX(ins, context, reg_src)
						);
					/* 16-bit operands */
					else if (EXE->REG_is_gr16(ins, context,reg_src))
						/* propagate the tag accordingly */
						EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
						r2m_xfer_opw,
						4, 
						IARG_THREAD_CONTEXT,
						IARG_MEMORYWRITE_EA,
						IARG_UINT32, 
						(uint32_t)REG16_INDX(ins, context,  reg_src)
						);
					/* 8-bit operands (upper) */
					// modify by menertry
					else if (EXE->REG_is_Upper8(ins, context, reg_src))
						/* propagate the tag accordingly */
						EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
						r2m_xfer_opb_u,
						4, 
						IARG_THREAD_CONTEXT,
						IARG_MEMORYWRITE_EA,
						IARG_UINT32, 
						(uint32_t)REG8_INDX(ins, context, reg_src)
						);
					/* 8-bit operands (lower) */
					else
						/* propagate the tag accordingly */
						EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
						r2m_xfer_opb_l,
						4, 
						IARG_THREAD_CONTEXT,
						IARG_MEMORYWRITE_EA,
						IARG_UINT32, 
						(uint32_t)REG8_INDX(ins, context, reg_src)
						);
				}
				/* done */
				break;
			/* conditional movs */
			case XED_ICLASS_CMOVB:
			case XED_ICLASS_CMOVBE:
			case XED_ICLASS_CMOVL:
			case XED_ICLASS_CMOVLE:
			case XED_ICLASS_CMOVNB:
			case XED_ICLASS_CMOVNBE:
			case XED_ICLASS_CMOVNL:
			case XED_ICLASS_CMOVNLE:
			case XED_ICLASS_CMOVNO:
			case XED_ICLASS_CMOVNP:
			case XED_ICLASS_CMOVNS:
			case XED_ICLASS_CMOVNZ:
			case XED_ICLASS_CMOVO:
			case XED_ICLASS_CMOVP:
			case XED_ICLASS_CMOVS:
			case XED_ICLASS_CMOVZ:	 	
				/*
				* the general format of these instructions
				* is the following: dst = src iff cond. We
				* move the tag of the source to the destination
				* iff the corresponding condition is met
				* (i.e., t[dst] = t[src])
				*/
				/* both operands are registers */
				if (EXE->INS_MemoryOperandCount(ins , context) == 0) {
					/* extract the operands */
					reg_dst = EXE->INS_OperandReg(ins, context,  0);
					reg_src = EXE->INS_OperandReg(ins, context, 1);

					/* 32-bit operands */
					if (EXE->REG_is_gr32(ins, context, reg_dst))
						/* propagate the tag accordingly */
						EXE->INS_InsertPredicatedCall(ins, context, IDFT_IPOINT_BEFORE,
							r2r_xfer_opl,
							5, 
							IARG_THREAD_CONTEXT,
							IARG_UINT32, 
							(uint32_t)REG32_INDX(ins, context, reg_dst),
							IARG_UINT32, 
							(uint32_t)REG32_INDX(ins, context, reg_src)
							);
					/* 16-bit operands */
					else 
						/* propagate tag accordingly */
						EXE->INS_InsertPredicatedCall(ins, context, IDFT_IPOINT_BEFORE,
							r2r_xfer_opw,
							5, 
							IARG_THREAD_CONTEXT,
							IARG_UINT32, 
							(uint32_t)REG16_INDX(ins, context, reg_dst),
							IARG_UINT32, 
							(uint32_t)REG16_INDX(ins, context, reg_src)
							);
			
				}
				/* 
				* 2nd operand is memory;
				* we optimize for that case, since most
				* instructions will have a register as
				* the first operand -- leave the result
				* into the reg and use it later
				*/
				else {    
					/* extract the register operand */
					reg_dst = EXE->INS_OperandReg(ins, context,  0);

					/* 32-bit operands */
					if (EXE->REG_is_gr32(ins, context,reg_dst))
						/* propagate the tag accordingly */
						// modify by menertry
						EXE->INS_InsertPredicatedCall(ins,  context, IDFT_IPOINT_BEFORE,
							m2r_xfer_opl,
							4,
							IARG_THREAD_CONTEXT,
							IARG_UINT32, 
							(uint32_t)REG32_INDX(ins, context, reg_dst),
							IARG_MEMORYREAD_EA
							);
					/* 16-bit operands */
					else
						/* propagate the tag accordingly */
						// modify by menertry
						EXE->INS_InsertPredicatedCall(ins,  context, IDFT_IPOINT_BEFORE,
							m2r_xfer_opw,
							4,
							IARG_THREAD_CONTEXT,
							IARG_UINT32, 
							(uint32_t)REG16_INDX(ins, context, reg_dst),
							IARG_MEMORYREAD_EA
							);
				}

				/* done */
				break;

			/* 
			* cbw;
			* move the tag associated with AL to AH
			*
			* NOTE: sign extension generates data that
			* are dependent to the source operand
			*/
			case XED_ICLASS_CBW:
				/* propagate the tag accordingly */
				// modify by menertry
				EXE->INS_InsertCall(ins,  context, IDFT_IPOINT_BEFORE,
					r2r_xfer_opb_ul,
					5, 
					IARG_THREAD_CONTEXT,
					IARG_UINT32, 
					(uint32_t)REG8_INDX(ins, context, EXE->REG_AH(ins, context)),
					IARG_UINT32,
					(uint32_t)REG8_INDX(ins, context, EXE->REG_AL(ins, context))
					);
				/* done */
				break;
			/*
			* cwd;
			* move the tag associated with AX to DX
			*
			* NOTE: sign extension generates data that
			* are dependent to the source operand
			*/
			case XED_ICLASS_CWD:
				/* propagate the tag accordingly */
				// modify by menertry
				EXE->INS_InsertCall(ins,  context, IDFT_IPOINT_BEFORE,
					r2r_xfer_opw,
					5,
					IARG_THREAD_CONTEXT,
					IARG_UINT32, 
					(uint32_t)REG16_INDX(ins, context, EXE->REG_DX(ins, context)),
					IARG_UINT32, 
					(uint32_t)REG16_INDX(ins, context, EXE->REG_AX(ins, context))
					);
				
				/* done */
				break;
			/* 
			* cwde;
			* move the tag associated with AX to EAX
			*
			* NOTE: sign extension generates data that
			* are dependent to the source operand
			*/
			case XED_ICLASS_CWDE:
				/* propagate the tag accordingly */
				// modify by menertry
				EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
					_cwde,
					1, 
					IARG_THREAD_CONTEXT
					);

				/* done */
				break;
			/*
			* cdq;
			* move the tag associated with EAX to EDX
			*
			* NOTE: sign extension generates data that
			* are dependent to the source operand
			*/
			case XED_ICLASS_CDQ:
				/* propagate the tag accordingly */
				// modify by menertry
				EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
					r2r_xfer_opl,
					5, 
					IARG_THREAD_CONTEXT,
					IARG_UINT32, 
					(uint32_t)REG32_INDX(ins, context, EXE->REG_EDX(ins, context)),
					IARG_UINT32, 
					(uint32_t)REG32_INDX(ins, context, EXE->REG_EAX(ins, context))
					);

				/* done */
				break;
			/* 
			* movsx;
			*
			* NOTE: sign extension generates data that
			* are dependent to the source operand
			*/
			case XED_ICLASS_MOVSX:
				/*
				* the general format of these instructions
				* is the following: dst = src. We move the
				* tag of the source to the destination
				* (i.e., t[dst] = t[src]) and we extend the
				* tag bits accordingly
				*/
				/* both operands are registers */
				if (EXE->INS_MemoryOperandCount(ins, context) == 0) {
					/* extract the operands */
					reg_dst = EXE->INS_OperandReg(ins, context, 0);
					reg_src = EXE->INS_OperandReg(ins, context, 1);

					/* 16-bit & 8-bit operands */
					if (EXE->REG_is_gr16(ins, context, reg_dst)) {
						/* upper 8-bit */
						if (EXE->REG_is_Upper8(ins, context,reg_src))
							/* propagate the tag accordingly */
							EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
								_movsx_r2r_opwb_u,
								5, 
								IARG_THREAD_CONTEXT,
								IARG_UINT32, 
								(uint32_t)REG16_INDX( ins, context, reg_dst),
								IARG_UINT32, 
								(uint32_t)REG8_INDX( ins, context, reg_src)
								);
						else
							/* propagate the tag accordingly */
							EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
								_movsx_r2r_opwb_l,
								5, 
								IARG_THREAD_CONTEXT,
								IARG_UINT32, 
								(uint32_t)REG16_INDX( ins, context,  reg_dst),
								IARG_UINT32, 
								(uint32_t)REG8_INDX( ins, context, reg_src)
								);
					}
					/* 32-bit & 16-bit operands */
					else if (EXE->REG_is_gr16(ins, context, reg_src))
						/* propagate the tag accordingly */
						EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
							_movsx_r2r_oplw,
							5, 
							IARG_THREAD_CONTEXT,
							IARG_UINT32, 
							(uint32_t)REG32_INDX(ins, context, reg_dst),
							IARG_UINT32, 
							(uint32_t)REG16_INDX(ins, context, reg_src)
							);
					/* 32-bit & 8-bit operands (upper 8-bit) */
					else if (EXE->REG_is_Upper8(ins, context, reg_src))
						/* propagate the tag accordingly */
						EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
							_movsx_r2r_oplb_u,
							5, 
							IARG_THREAD_CONTEXT,
							IARG_UINT32, 
							(uint32_t)REG32_INDX(ins, context, reg_dst),
							IARG_UINT32, 
							(uint32_t)REG8_INDX(ins, context,reg_src)
							);	
					/* 32-bit & 8-bit operands (lower 8-bit) */
					else
						/* propagate the tag accordingly */
						EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
							_movsx_r2r_oplb_l,
							5, 
							IARG_THREAD_CONTEXT,
							IARG_UINT32, 
							(uint32_t)REG32_INDX(ins, context,reg_dst),
							IARG_UINT32, 
							(uint32_t)REG8_INDX(ins, context,reg_src)
							);
				}
				/* 2nd operand is memory */
				else {
					/* extract the operands */
					reg_dst = EXE->INS_OperandReg(ins, context, 0);

					/* 16-bit & 8-bit operands */
					// modify by menertry
					if (EXE->REG_is_gr16(ins, context, reg_dst))
						/* propagate the tag accordingly */
						EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
							_movsx_m2r_opwb,
							4, 
							IARG_THREAD_CONTEXT,
							IARG_UINT32, 
							(uint32_t)REG16_INDX(ins, context, reg_dst),
							IARG_MEMORYREAD_EA
							);
					/* 32-bit & 16-bit operands */
					else if (EXE->INS_MemoryWriteSize(ins, context) ==
						BIT2BYTE(MEM_WORD_LEN))
						/* propagate the tag accordingly */
						EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
							_movsx_m2r_oplw,
							4, 
							IARG_THREAD_CONTEXT,
							IARG_UINT32, 
							(uint32_t)REG32_INDX(ins, context, reg_dst),
							IARG_MEMORYREAD_EA
							);
					/* 32-bit & 8-bit operands */
					else
						/* propagate the tag accordingly */
						EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
							_movsx_m2r_oplb,
							4, 
							IARG_THREAD_CONTEXT,
							IARG_UINT32, 
							(uint32_t)REG32_INDX(ins, context, reg_dst),
							IARG_MEMORYREAD_EA
							);


				}

				/* done */
				break;

			/*
			* movzx;
			*
			* NOTE: zero extension always results in
			* clearing the tags associated with the
			* higher bytes of the destination operand
			*/
			case XED_ICLASS_MOVZX:
				/*
				* the general format of these instructions
				* is the following: dst = src. We move the
				* tag of the source to the destination
				* (i.e., t[dst] = t[src]) and we extend the
				* tag bits accordingly
				*/
				/* both operands are registers */	
				if (EXE->INS_MemoryOperandCount(ins, context) == 0) {
					/* extract the operands */
					reg_dst = EXE->INS_OperandReg(ins, context, 0);
					reg_src = EXE->INS_OperandReg(ins, context, 1);

					/* 16-bit & 8-bit operands */
					if (EXE->REG_is_gr16(ins, context, reg_dst)) {
						/* upper 8-bit */
						if (EXE->REG_is_Upper8(ins, context, reg_src))
							/* propagate the tag accordingly */
							EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
								_movzx_r2r_opwb_u,
								5, 
								IARG_THREAD_CONTEXT,
								IARG_UINT32, 
								(uint32_t)REG16_INDX(ins, context, reg_dst),
								IARG_UINT32, 
								(uint32_t)REG8_INDX(ins, context, reg_src)
								);
						else
							/* propagate the tag accordingly */
							EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
								_movzx_r2r_opwb_l,
								5, 
								IARG_THREAD_CONTEXT,
								IARG_UINT32, 
								(uint32_t)REG16_INDX(ins, context, reg_dst),
								IARG_UINT32, 
								(uint32_t)REG8_INDX(ins, context, reg_src)
								);
					}
					/* 32-bit & 16-bit operands */
					else if (EXE->REG_is_gr16(ins, context, reg_src))
						/* propagate the tag accordingly */
						EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
							_movzx_r2r_oplw,
							5, 
							IARG_THREAD_CONTEXT,
							IARG_UINT32, 
							(uint32_t)REG32_INDX(ins, context, reg_dst),
							IARG_UINT32, 
							(uint32_t) REG16_INDX(ins, context, reg_src)
							);
					/* 32-bit & 8-bit operands (upper 8-bit) */
					else if (EXE->REG_is_Upper8(ins, context, reg_src))
						/* propagate the tag accordingly */
						EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
							_movzx_r2r_oplb_u,
							5, 
							IARG_THREAD_CONTEXT,
							IARG_UINT32, 
							(uint32_t)REG32_INDX(ins, context, reg_dst),
							IARG_UINT32, 
							(uint32_t)REG8_INDX(ins, context,  reg_src)
							);
					/* 32-bit & 8-bit operands (lower 8-bit) */
					else
						/* propagate the tag accordingly */
						EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
							_movzx_r2r_oplb_l,
							5, 
							IARG_THREAD_CONTEXT,
							IARG_UINT32, 
							(uint32_t)REG32_INDX(ins, context, reg_dst),
							IARG_UINT32, 
							(uint32_t)REG8_INDX(ins, context, reg_src)
							);

				}
				/* 2nd operand is memory */
				else {
					/* extract the operands */
					reg_dst = EXE->INS_OperandReg(ins, context, 0);

					/* 16-bit & 8-bit operands */
					if (EXE->REG_is_gr16(ins, context, reg_dst))
						/* propagate the tag accordingly */
						EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
							_movzx_m2r_opwb,
							4, 
							IARG_THREAD_CONTEXT,
							IARG_UINT32, 
							(uint32_t)REG16_INDX(ins, context, reg_dst),
							IARG_MEMORYREAD_EA
							);
					/* 32-bit & 16-bit operands */
					else if (EXE->INS_MemoryWriteSize(ins, context) ==
						BIT2BYTE(MEM_WORD_LEN))
						/* propagate the tag accordingly */
						EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
							_movzx_m2r_oplw,
							4, 
							IARG_THREAD_CONTEXT,
							IARG_UINT32, 
							(uint32_t)REG32_INDX(ins, context, reg_dst),
							IARG_MEMORYREAD_EA
							);
					/* 32-bit & 8-bit operands */
					else
						/* propagate the tag accordingly */
						EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
							_movzx_m2r_oplb,
							4, 
							IARG_THREAD_CONTEXT,
							IARG_UINT32, 
							(uint32_t)REG32_INDX(ins, context, reg_dst),
							IARG_MEMORYREAD_EA
							);
				}
				/* done */
				break;
			/* div */
			case XED_ICLASS_DIV:
			/* idiv */
			case XED_ICLASS_IDIV:
			/* mul */
			case XED_ICLASS_MUL:
				/*
				* the general format of these brain-dead and
				* totally corrupted instructions is the following:
				* dst1:dst2 {*, /}= src. We tag the destination
				* operands if the source is also taged
				* (i.e., t[dst1]:t[dst2] |= t[src])
				*/
				/* memory operand */	
				if (EXE->INS_OperandIsMemory(ins, context, 0))
				/* differentiate based on the memory size */
					switch (EXE->INS_MemoryWriteSize(ins, context)) {
						/* 4 bytes */
						case BIT2BYTE(MEM_LONG_LEN):
							/* propagate the tag accordingly */
							EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
								m2r_ternary_opl,
								2,
								IARG_THREAD_CONTEXT,
								IARG_MEMORYREAD_EA
								);

							/* done */
							break;
						/* 2 bytes */
						case BIT2BYTE(MEM_WORD_LEN):
							/* propagate the tag accordingly */
							EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
								m2r_ternary_opw,
								2, 
								IARG_THREAD_CONTEXT,
								IARG_MEMORYREAD_EA
								);

								/* done */
								break;
						/* 1 byte */
						case BIT2BYTE(MEM_BYTE_LEN):
						default:
							/* propagate the tag accordingly */
							EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
								m2r_ternary_opb,
								2, 
								IARG_THREAD_CONTEXT,
								IARG_MEMORYREAD_EA
								);

								/* done */
								break;
					}
				/* register operand */
				else {
					/* extract the operand */
					reg_src = EXE->INS_OperandReg(ins, context, 0);

					/* 32-bit operand */
					if (EXE->REG_is_gr32(ins, context, reg_src))
						/* propagate the tag accordingly */
						EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
							r2r_ternary_opl,
							3,
							IARG_THREAD_CONTEXT,
							IARG_UINT32, 
							(uint32_t)REG32_INDX(ins, context,reg_src)
							);
					/* 16-bit operand */
					else if (EXE->REG_is_gr16(ins, context, reg_src))
						/* propagate the tag accordingly */
						EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
							r2r_ternary_opw,
							3, 
							IARG_THREAD_CONTEXT,
							IARG_UINT32, 
							(uint32_t)REG16_INDX(ins, context, reg_src)
							);
					/* 8-bit operand (upper) */
					else if (EXE->REG_is_Upper8(ins, context, reg_src))
						/* propagate the tag accordingly */
						EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
							r2r_ternary_opb_u,
							3, 
							IARG_THREAD_CONTEXT,
							IARG_UINT32, 
							(uint32_t)REG8_INDX(ins, context, reg_src)
							);
					/* 8-bit operand (lower) */
					else 
						/* propagate the tag accordingly */
						EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
							r2r_ternary_opb_l,
							3, 
							IARG_THREAD_CONTEXT,
							IARG_UINT32, 
							(uint32_t)REG8_INDX(ins, context, reg_src)
							);
				}
				/* done */
				break;
			/*
			* imul;
			* I'm still wondering how brain-damaged the
			* ISA architect should be in order to come
			* up with something so ugly as the IMUL 
			* instruction
			*/
			case XED_ICLASS_IMUL:
				/* one-operand form */
				if (EXE->INS_OperandIsImplicit(ins, context, 1)) {
					/* memory operand */
					if (EXE->INS_OperandIsMemory(ins, context, 0))
						/* differentiate based on the memory size */
						switch (EXE->INS_MemoryWriteSize(ins, context)) {
							/* 4 bytes */
							case BIT2BYTE(MEM_LONG_LEN):
								/* propagate the tag accordingly */
								EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
									m2r_ternary_opl,
									2, 
									IARG_THREAD_CONTEXT,
									IARG_MEMORYREAD_EA
									);

								/* done */
								break;
							/* 2 bytes */
							case BIT2BYTE(MEM_WORD_LEN):
								/* propagate the tag accordingly */
								EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
									m2r_ternary_opw,
									2, 
									IARG_THREAD_CONTEXT,
									IARG_MEMORYREAD_EA
									);

								/* done */
								break;
							/* 1 byte */
							case BIT2BYTE(MEM_BYTE_LEN):
							default:
								/* propagate the tag accordingly */
								EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
									m2r_ternary_opb,
									2, 
									IARG_THREAD_CONTEXT,
									IARG_MEMORYREAD_EA
									);

									/* done */
								break;
							
						}
					/* register operand */
					else{
						/* extract the operand */
						reg_src = EXE->INS_OperandReg(ins, context,  0);

						/* 32-bit operand */
						if (EXE->REG_is_gr32(ins, context, reg_src))
							/* propagate the tag accordingly */
							EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
								r2r_ternary_opl,
								3, 
								IARG_THREAD_CONTEXT,
								IARG_UINT32, 
								(uint32_t)REG32_INDX(ins, context, reg_src)
								);
						/* 16-bit operand */
						else if (EXE->REG_is_gr16(ins, context, reg_src))
							/* propagate the tag accordingly */
							EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
								r2r_ternary_opw,
								3, 
								IARG_THREAD_CONTEXT,
								IARG_UINT32, 
								(uint32_t)REG16_INDX(ins, context,reg_src)
								);
						/* 8-bit operand (upper) */
						else if (EXE->REG_is_Upper8(ins, context, reg_src))
							/* propagate the tag accordingly */
							EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
								r2r_ternary_opb_u,
								3, 
								IARG_THREAD_CONTEXT,
								IARG_UINT32, 
								(uint32_t)REG8_INDX(ins, context,reg_src)
								);
						/* 8-bit operand (lower) */
						else
							/* propagate the tag accordingly */
							EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
								r2r_ternary_opb_l,
								3, 
								IARG_THREAD_CONTEXT,
								IARG_UINT32, 
								(uint32_t)REG8_INDX(ins, context, reg_src)
								);
					}						
				}
				/* two/three-operands form */
				else {
					/* 2nd operand is immediate; do nothing */
					if (EXE->INS_OperandIsImmediate(ins, context,  1))
						break;

					/* both operands are registers */
					if (EXE->INS_MemoryOperandCount(ins, context) == 0) {
						/* extract the operands */
						reg_dst = EXE->INS_OperandReg(ins, context, 0);
						reg_src = EXE->INS_OperandReg(ins, context, 1);

						/* 32-bit operands */
						if (EXE->REG_is_gr32(ins, context,reg_dst))
						/* propagate the tag accordingly */
							EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
								r2r_binary_opl,
								5, 
								IARG_THREAD_CONTEXT,
								IARG_UINT32, 
								(uint32_t)REG32_INDX(ins, context,reg_dst),
								IARG_UINT32, 
								(uint32_t)REG32_INDX(ins, context,reg_src)
								);
						/* 16-bit operands */
						else
						/* propagate tag accordingly */
							EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
								r2r_binary_opw,
								5, 
								IARG_THREAD_CONTEXT,
								IARG_UINT32, 
								(uint32_t)REG16_INDX(ins, context, reg_dst),
								IARG_UINT32, 
								(uint32_t)REG16_INDX(ins, context, reg_src)
								);

					}
					/* 
					* 2nd operand is memory;
					* we optimize for that case, since most
					* instructions will have a register as
					* the first operand -- leave the result
					* into the reg and use it later
					*/
					else {
						/* extract the register operand */
						reg_dst = EXE->INS_OperandReg(ins,context,  0);

						/* 32-bit operands */
						if (EXE->REG_is_gr32(ins,context,reg_dst))
						/* propagate the tag accordingly */
							EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
								m2r_binary_opl,
								4, 
								IARG_THREAD_CONTEXT,
								IARG_UINT32, 
								(uint32_t)REG32_INDX(ins, context, reg_dst),
								IARG_MEMORYREAD_EA
								);
						/* 16-bit operands */
						else
						/* propagate the tag accordingly */
							EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
								m2r_binary_opw,
								4, 
								IARG_THREAD_CONTEXT,
								IARG_UINT32, 
								(uint32_t)REG16_INDX(ins, context, reg_dst),
								IARG_MEMORYREAD_EA
								);
					}
				}
				/* done */
				break;
			/* conditional sets */
			case XED_ICLASS_SETB:
			case XED_ICLASS_SETBE:
			case XED_ICLASS_SETL:
			case XED_ICLASS_SETLE:
			case XED_ICLASS_SETNB:
			case XED_ICLASS_SETNBE:
			case XED_ICLASS_SETNL:
			case XED_ICLASS_SETNLE:
			case XED_ICLASS_SETNO:
			case XED_ICLASS_SETNP:
			case XED_ICLASS_SETNS:
			case XED_ICLASS_SETNZ:
			case XED_ICLASS_SETO:
			case XED_ICLASS_SETP:
			case XED_ICLASS_SETS:
			case XED_ICLASS_SETZ:
				/*
				* clear the tag information associated with the
				* destination operand
				*/
				/* register operand */
				if (EXE->INS_MemoryOperandCount(ins , context) == 0) {
					/* extract the operand */
					reg_dst = EXE->INS_OperandReg(ins, context, 0);

					/* 8-bit operand (upper) */
					if (EXE->REG_is_Upper8(ins , context, reg_dst))	
						/* propagate tag accordingly */
						EXE->INS_InsertPredicatedCall(ins, context, IDFT_IPOINT_BEFORE,
								r_clrb_u,
							3, 
							IARG_THREAD_CONTEXT,
							IARG_UINT32, 
							(uint32_t)REG8_INDX(ins, context,  reg_dst)
							);
					/* 8-bit operand (lower) */
					else 
						/* propagate tag accordingly */
						EXE->INS_InsertPredicatedCall(ins, context, IDFT_IPOINT_BEFORE,
								r_clrb_l,
							3, 
							IARG_THREAD_CONTEXT,
							IARG_UINT32, 
							(uint32_t)REG8_INDX(ins, context, reg_dst)
							);
				}
				/* memory operand */
				else
					/* propagate the tag accordingly */
					EXE->INS_InsertPredicatedCall(ins, context, IDFT_IPOINT_BEFORE,
						tagmap_clrb,
						1,
						IARG_MEMORYWRITE_EA
						);

				/* done */
				break;
			/* 
			* stmxcsr;
			* clear the destination operand (register only)
			*/
			case XED_ICLASS_STMXCSR:
				/* propagate tag accordingly */
				EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
					tagmap_clrl,
					1,
					IARG_MEMORYWRITE_EA
					);
			
				/* done */
				break;
			/* smsw */
			case XED_ICLASS_SMSW:
			/* str */
			case XED_ICLASS_STR:
				/*
				* clear the tag information associated with
				* the destination operand
				*/
				/* register operand */
				if (EXE->INS_MemoryOperandCount(ins, context) == 0) {
					/* extract the operand */
					reg_dst = EXE->INS_OperandReg(ins, context, 0);

					/* 16-bit register */
					if (EXE->REG_is_gr16(ins, context, reg_dst))
						/* propagate tag accordingly */
						EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
							r_clrw,
							3, 
							IARG_THREAD_CONTEXT,
							IARG_UINT32, 
							(uint32_t)REG16_INDX(ins, context, reg_dst)
							);
					/* 32-bit register */
					else 
						/* propagate tag accordingly */
						EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
							r_clrl,
							3, 
							IARG_THREAD_CONTEXT,
							IARG_UINT32, 
							(uint32_t)REG32_INDX(ins, context, reg_dst)
							);
				}
				/* memory operand */
				else
					/* propagate tag accordingly */
					EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
						tagmap_clrw,
						1,
						IARG_MEMORYWRITE_EA
						);

				/* done */
				break;
			/* 
			* lar;
			* clear the destination operand (register only)
			*/
			case XED_ICLASS_LAR:
				/* extract the 1st operand */
				reg_dst = EXE->INS_OperandReg(ins, context ,0);

				/* 16-bit register */
				if (EXE->REG_is_gr16(ins, context, reg_dst))
					/* propagate tag accordingly */
					// modify by menertry
					EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
						r_clrw,
						3, 
						IARG_THREAD_CONTEXT,
						IARG_UINT32, 
						(uint32_t)REG16_INDX(ins, context,  reg_dst)
						);
				/* 32-bit register */
				else
					/* propagate tag accordingly */
					EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
						r_clrl,
						3, 
						IARG_THREAD_CONTEXT,
						IARG_UINT32, 
						(uint32_t)REG32_INDX(ins, context, reg_dst)
						);

				/* done */
				break;
			/* rdpmc */
			case XED_ICLASS_RDPMC:
			/* rdtsc */
			case XED_ICLASS_RDTSC:
				/*
				* clear the tag information associated with
				* EAX and EDX
				*/
				/* propagate tag accordingly */
				EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
					r_clrl2,
					1, 
					IARG_THREAD_CONTEXT
					);

				/* done */
				break;
			/* 
			* cpuid;
			* clear the tag information associated with
			* EAX, EBX, ECX, and EDX 
			*/
			case XED_ICLASS_CPUID:
				/* propagate tag accordingly */
				EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
					r_clrl4,
					1,
					IARG_THREAD_CONTEXT
					);

				/* done */
				break;
			/* 
			* lahf;
			* clear the tag information of AH
			*/
			case XED_ICLASS_LAHF:
				/* propagate tag accordingly */
				EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
					r_clrb_u,
					3, 
					IARG_THREAD_CONTEXT,
					IARG_UINT32, 
					(uint32_t)REG8_INDX(ins, context, EXE->REG_AH(ins, context))
					);

				/* done */
				break;
			/* 
			* cmpxchg;
			* t[dst] = t[src] iff EAX/AX/AL == dst, else
			* t[EAX/AX/AL] = t[dst] -- yes late-night coding again
			* and I'm really tired to comment this crap...
			*/
			case XED_ICLASS_CMPXCHG:
				/* both operands are registers */
				if (EXE->INS_MemoryOperandCount(ins, context) == 0) {
					/* extract the operands */
					reg_dst = EXE->INS_OperandReg(ins, context, 0);
					reg_src = EXE->INS_OperandReg(ins, context, 1);
					/* 32-bit operands */
					if (EXE->REG_is_gr32(ins, context, reg_dst)) {
						/* propagate tag accordingly; fast path */
						EXE->INS_InsertIfCall(ins, context, IDFT_IPOINT_BEFORE,
							_cmpxchg_r2r_opl_fast,
							7, 
							IARG_THREAD_CONTEXT,
							IARG_REG_VALUE, 
							EXE->REG_EAX(ins, context),
							IARG_UINT32, 
							(uint32_t)REG32_INDX( ins, context ,reg_dst),
							IARG_REG_VALUE, 
							reg_dst
							);
						/* propagate tag accordingly; slow path */
						EXE->INS_InsertThenCall(ins, context, IDFT_IPOINT_BEFORE,
							_cmpxchg_r2r_opl_slow,
							5,
							IARG_THREAD_CONTEXT,
							IARG_UINT32, 
							(uint32_t)REG32_INDX( ins, context ,reg_dst),
							IARG_UINT32, 
							(uint32_t)REG32_INDX(ins, context ,reg_src)
							);					
					}
					/* 16-bit operands */
					else if (EXE->REG_is_gr16(ins, context, reg_dst)) {
					/* propagate tag accordingly; fast path */
						EXE->INS_InsertIfCall(ins, context, IDFT_IPOINT_BEFORE,
						_cmpxchg_r2r_opw_fast,
						7,
						IARG_THREAD_CONTEXT,
						IARG_REG_VALUE, 
						EXE->REG_AX(ins, context),
						IARG_UINT32, 
						(uint32_t)REG16_INDX(ins, context, reg_dst),
						IARG_REG_VALUE, 
						reg_dst
						);	
					/* propagate tag accordingly; slow path */
						// modify by menertry
						EXE->INS_InsertThenCall(ins, context, IDFT_IPOINT_BEFORE,
							_cmpxchg_r2r_opw_slow,
							5, 
							IARG_THREAD_CONTEXT,
							IARG_UINT32, 
							(uint32_t)REG16_INDX(ins, context, reg_dst),
							IARG_UINT32, 
							(uint32_t) REG16_INDX(ins, context,  reg_src)
							);	
					}
					/* 8-bit operands */
					else
						IDFT_LOG("unhandled operand： %s\n",EXE->INS_Disassemble(ins, context) );
				}
				/* 1st operand is memory */
				else {
					/* extract the operand */
					// modify by menertry
					reg_src = EXE->INS_OperandReg(ins, context, 1);
					
					/* 32-bit operands */
					if (EXE->REG_is_gr32(ins, context, reg_src)) {
					/* propagate tag accordingly; fast path */
						EXE->INS_InsertIfCall(ins, context, IDFT_IPOINT_BEFORE,
							_cmpxchg_m2r_opl_fast,
							4,
							IARG_THREAD_CONTEXT,
							IARG_REG_VALUE, 
							EXE->REG_EAX(ins, context),
							IARG_MEMORYREAD_EA
							);
					/* propagate tag accordingly; slow path */
						EXE->INS_InsertThenCall(ins, context, IDFT_IPOINT_BEFORE,
							_cmpxchg_r2m_opl_slow,
							4, 
							IARG_THREAD_CONTEXT,
							IARG_MEMORYWRITE_EA,
							IARG_UINT32, 
							(uint32_t)REG32_INDX( ins, context, reg_src)
							);
					}
					/* 16-bit operands */
					else if (EXE->REG_is_gr16(ins, context, reg_src)) {
					/* propagate tag accordingly; fast path */
						EXE->INS_InsertIfCall(ins, context, IDFT_IPOINT_BEFORE,
							_cmpxchg_m2r_opw_fast,
							4, 
							IARG_THREAD_CONTEXT,
							IARG_REG_VALUE, 
							EXE->REG_EAX(ins, context),
							IARG_MEMORYREAD_EA
							);	
					/* propagate tag accordingly; slow path */
						EXE->INS_InsertThenCall(ins, context, IDFT_IPOINT_BEFORE,
							_cmpxchg_r2m_opw_slow,
							4, 
							IARG_THREAD_CONTEXT,
							IARG_MEMORYWRITE_EA,
							IARG_UINT32, 
							EXE->REG16_INDX(ins, context, reg_src)
							);
					}
					/* 8-bit operands */
					else
						IDFT_LOG("unhandled operand： %s\n",EXE->INS_Disassemble(ins, context) );
						
				}
				/* done */
				break;

			/* 
			* xchg;
			* exchange the tag information of the two operands
			*/
			case XED_ICLASS_XCHG:
				/* both operands are registers */
				if (EXE->INS_MemoryOperandCount(ins,context) == 0) {
					/* extract the operands */
					reg_dst = EXE->INS_OperandReg(ins, context,  0);
					reg_src = EXE->INS_OperandReg(ins,context, 1);

					/* 32-bit operands */
					if (EXE->REG_is_gr32(ins,context, reg_dst)) {
						/* propagate the tag accordingly */
						EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
							r2r_xfer_opl,
							5, 
							IARG_THREAD_CONTEXT,
							IARG_UINT32, 
							8,
							IARG_UINT32, 
							REG32_INDX(ins, context,  reg_dst)
							);
						EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
							r2r_xfer_opl,
							5, 
							IARG_THREAD_CONTEXT,
							IARG_UINT32, 
							(uint32_t)REG32_INDX(ins, context, reg_dst),
							IARG_UINT32, 
							(uint32_t)REG32_INDX(ins, context, reg_src)
							);
						EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
							r2r_xfer_opl,
							5, 
							IARG_THREAD_CONTEXT,
							IARG_UINT32, 
							(uint32_t)REG32_INDX(ins, context, reg_src),
							IARG_UINT32, 
							8
							);
					}
					/* 16-bit operands */
					else if (EXE->REG_is_gr16(ins, context,reg_dst))
						/* propagate tag accordingly */
						EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
							_xchg_r2r_opw,
							5, 
							IARG_THREAD_CONTEXT,
							IARG_UINT32, 
							(uint32_t)REG16_INDX(ins, context,reg_dst),
							IARG_UINT32, 
							(uint32_t)REG16_INDX(ins, context,reg_src)
							);
					/* 8-bit operands */
					else if (EXE->REG_is_gr8(ins, context, reg_dst)) {
						/* propagate tag accordingly */
						if (EXE->REG_is_Lower8(ins, context, reg_dst) &&
							EXE->REG_is_Lower8(ins, context,reg_src))
							/* lower 8-bit registers */
							EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
								_xchg_r2r_opb_l,
								5, 
								IARG_THREAD_CONTEXT,
								IARG_UINT32, 
								(uint32_t)REG8_INDX(ins, context,reg_dst),
								IARG_UINT32, 
								(uint32_t)REG8_INDX(ins, context, reg_src)
								);
						else if(EXE->REG_is_Upper8(ins, context,reg_dst) &&
							EXE->REG_is_Upper8(ins, context,reg_src))	
							/* upper 8-bit registers */
							EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
								_xchg_r2r_opb_u,
								5, 
								IARG_THREAD_CONTEXT,
								IARG_UINT32, 
								(uint32_t)REG8_INDX(ins, context,reg_dst),
								IARG_UINT32, 
								(uint32_t)REG8_INDX(ins, context,reg_src)
								);
						else if (EXE->REG_is_Lower8(ins, context, reg_dst))
							/* 
							* destination register is a
							* lower 8-bit register and
							* source register is an upper
							* 8-bit register
							*/
							EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
								_xchg_r2r_opb_lu,
								5, 
								IARG_THREAD_CONTEXT,
								IARG_UINT32, 
								(uint32_t)REG8_INDX(ins, context, reg_dst),
								IARG_UINT32, 
								(uint32_t)REG8_INDX(ins, context,reg_src)
								);
						else
							/* 
							* destination register is an
							* upper 8-bit register and
							* source register is a lower
							* 8-bit register
							*/
							EXE->INS_InsertCall(ins,  context, IDFT_IPOINT_BEFORE,
								_xchg_r2r_opb_ul,
								5, 
								IARG_THREAD_CONTEXT,
								IARG_UINT32, 
								(uint32_t)REG8_INDX(ins, context, reg_dst),
								IARG_UINT32, 
								(uint32_t)REG8_INDX(ins, context, reg_src)
								);
					}
				}
				/* 
				* 2nd operand is memory;
				* we optimize for that case, since most
				* instructions will have a register as
				* the first operand -- leave the result
				* into the reg and use it later
				*/
				else if (EXE->INS_OperandIsMemory(ins, context, 1)) {
					/* extract the register operand */
					reg_dst = EXE->INS_OperandReg(ins, context, 0);

					/* 32-bit operands */
					if (EXE->REG_is_gr32(ins, context, reg_dst))
						/* propagate the tag accordingly */
						EXE->INS_InsertCall(ins,  context, IDFT_IPOINT_BEFORE,
							_xchg_m2r_opl,
							4, 
							IARG_THREAD_CONTEXT,
							IARG_UINT32, 
							(uint32_t)REG32_INDX(ins, context, reg_dst),
							IARG_MEMORYREAD_EA
							);
					/* 16-bit operands */
					else if (EXE->REG_is_gr16(ins, context, reg_dst))
						/* propagate the tag accordingly */
						EXE->INS_InsertCall(ins,  context, IDFT_IPOINT_BEFORE,
							_xchg_m2r_opw,
							4, 
							IARG_THREAD_CONTEXT,
							IARG_UINT32, 
							(uint32_t)REG16_INDX(ins,  context, reg_dst),
							IARG_MEMORYREAD_EA
							);
					/* 8-bit operands (upper) */
					else if (EXE->REG_is_Upper8(ins, context, reg_dst))
						/* propagate the tag accordingly */
						EXE->INS_InsertCall(ins,  context, IDFT_IPOINT_BEFORE,
							_xchg_m2r_opb_u,
							4, 
							IARG_THREAD_CONTEXT,
							IARG_UINT32, 
							(uint32_t)REG8_INDX(ins,  context, reg_dst),
							IARG_MEMORYREAD_EA
							);
					/* 8-bit operands (lower) */
					else
						/* propagate the tag accordingly */
						EXE->INS_InsertCall(ins,  context, IDFT_IPOINT_BEFORE,
							_xchg_m2r_opb_l,
							4, 
							IARG_THREAD_CONTEXT,
							IARG_UINT32, 
							(uint32_t)REG8_INDX(ins,  context, reg_dst),
							IARG_MEMORYREAD_EA
							);
				}
				/* 1st operand is memory */
				else {
					/* extract the register operand */
				reg_src = EXE->INS_OperandReg(ins,  context,  1);

				/* 32-bit operands */
				if (EXE->REG_is_gr32(ins,  context,  reg_src))
					/* propagate the tag accordingly */
					EXE->INS_InsertCall(ins,  context, IDFT_IPOINT_BEFORE,
						_xchg_m2r_opl,
						4, 
						IARG_THREAD_CONTEXT,
						IARG_UINT32, 
						(uint32_t)REG32_INDX(ins,  context, reg_src),
						IARG_MEMORYWRITE_EA
						);
				/* 16-bit operands */
				else if (EXE->REG_is_gr16(ins,  context, reg_src))
					/* propagate the tag accordingly */
					EXE->INS_InsertCall(ins,  context, IDFT_IPOINT_BEFORE,
						_xchg_m2r_opw,
						4, 
						IARG_THREAD_CONTEXT,
						IARG_UINT32, 
						(uint32_t)REG16_INDX(ins,  context,reg_src),
						IARG_MEMORYWRITE_EA
						);
				/* 8-bit operands (upper) */
				else if (EXE->REG_is_Upper8(ins,  context, reg_src))
					/* propagate the tag accordingly */
					EXE->INS_InsertCall(ins,  context, IDFT_IPOINT_BEFORE,
						_xchg_m2r_opb_u,
						4, 
						IARG_THREAD_CONTEXT,
						IARG_UINT32, 
						(uint32_t)REG8_INDX(ins,  context, reg_src),
						IARG_MEMORYWRITE_EA
						);
				/* 8-bit operands (lower) */
				else
					/* propagate the tag accordingly */
					// modify by menertry
					EXE->INS_InsertCall(ins,  context, IDFT_IPOINT_BEFORE,
						_xchg_m2r_opb_l,
						4, 
						IARG_THREAD_CONTEXT,
						IARG_UINT32, 
						(uint32_t)REG8_INDX(ins,  context, reg_src),
						IARG_MEMORYWRITE_EA
						);
				}
				/* done */
				break;
			/* 
			* xadd;
			* xchg + add. We instrument this instruction  using the tag
			* logic of xchg and add (see above)
			*/
			case XED_ICLASS_XADD:
				/* both operands are registers */
				if (EXE->INS_MemoryOperandCount(ins, context) == 0) {
					/* extract the operands */
					reg_dst = EXE->INS_OperandReg(ins, context,  0);
					reg_src = EXE->INS_OperandReg(ins, context, 1);

					/* 32-bit operands */
					if (EXE->REG_is_gr32(ins, context, reg_dst)) {
						/* propagate the tag accordingly */
						EXE->INS_InsertCall(ins,  context, IDFT_IPOINT_BEFORE,
							r2r_xfer_opl,
							5, 
							IARG_THREAD_CONTEXT,
							IARG_UINT32, 
							8,
							IARG_UINT32, 
							(uint32_t)REG32_INDX(ins,  context, reg_dst)
							);
						EXE->INS_InsertCall(ins,  context, IDFT_IPOINT_BEFORE,
							r2r_xfer_opl,
							5, 
							IARG_THREAD_CONTEXT,
							IARG_UINT32, 
							(uint32_t)REG32_INDX(ins,  context, reg_dst),
							IARG_UINT32, 
							(uint32_t)REG32_INDX(ins,  context, reg_src)
							);
						EXE->INS_InsertCall(ins,  context, IDFT_IPOINT_BEFORE,
							r2r_xfer_opl,
							5, 
							IARG_THREAD_CONTEXT,
							IARG_UINT32, 
							(uint32_t)REG32_INDX(ins,  context, reg_src),
							IARG_UINT32, 
							8
							);
						EXE->INS_InsertCall(ins,  context, IDFT_IPOINT_BEFORE,
							r2r_binary_opl,
							5,
							IARG_THREAD_CONTEXT,
							IARG_UINT32, 
							(uint32_t)REG32_INDX(ins,  context, reg_dst),
							IARG_UINT32, 
							(uint32_t)REG32_INDX(ins,  context, reg_src)
							);
						
					}
					/* 16-bit operands */
					else if (EXE->REG_is_gr16(ins,  context, reg_dst))
						/* propagate tag accordingly */
						EXE->INS_InsertCall(ins,  context, IDFT_IPOINT_BEFORE,
							_xadd_r2r_opw,
							5, 
							IARG_THREAD_CONTEXT,
							IARG_UINT32, 
							(uint32_t)REG16_INDX(ins,  context,reg_dst),
							IARG_UINT32, 
							(uint32_t)REG16_INDX(ins,  context,reg_src)
							);
					/* 8-bit operands */
					else if (EXE->REG_is_gr8(ins,  context, reg_dst)) {
						/* propagate tag accordingly */
						if (EXE->REG_is_Lower8(ins,  context,reg_dst) &&
							EXE->REG_is_Lower8(ins,  context, reg_src))
							/* lower 8-bit registers */
							EXE->INS_InsertCall(ins,  context, IDFT_IPOINT_BEFORE,
								_xadd_r2r_opb_l,
								5, 
								IARG_THREAD_CONTEXT,
								IARG_UINT32, 
								(uint32_t)REG8_INDX(ins,  context, reg_dst),
								IARG_UINT32, 
								(uint32_t)REG8_INDX(ins,  context,reg_src)
								);
						else if(EXE->REG_is_Upper8(ins,  context,reg_dst) &&
							EXE->REG_is_Upper8(ins,  context, reg_src))
							/* upper 8-bit registers */
								// modify by menertry
								EXE->INS_InsertCall(ins,  context, IDFT_IPOINT_BEFORE,
								_xadd_r2r_opb_u,
								5, 
								IARG_THREAD_CONTEXT,
								IARG_UINT32, 
								(uint32_t)REG8_INDX(ins,  context, reg_dst),
								IARG_UINT32, 
								(uint32_t)REG8_INDX(ins,  context, reg_src)
								);
						else if (EXE->REG_is_Lower8(ins,  context,reg_dst))
							/* 
							* destination register is a
							* lower 8-bit register and
							* source register is an upper
							* 8-bit register
							*/
							EXE->INS_InsertCall(ins,  context, IDFT_IPOINT_BEFORE,
								_xadd_r2r_opb_lu,
								5, 
								IARG_THREAD_CONTEXT,
								IARG_UINT32, 
								(uint32_t)REG8_INDX(ins,  context,reg_dst),
								IARG_UINT32, 
								(uint32_t)REG8_INDX(ins,  context, reg_src)
								);
						else
							/* 
							* destination register is an
							* upper 8-bit register and
							* source register is a lower
							* 8-bit register
							*/
							EXE->INS_InsertCall(ins,  context, IDFT_IPOINT_BEFORE,
								_xadd_r2r_opb_ul,
								5,
								IARG_THREAD_CONTEXT,
								IARG_UINT32, 
								(uint32_t)REG8_INDX(ins,  context, reg_dst),
								IARG_UINT32, 
								(uint32_t)REG8_INDX(ins,  context, reg_src)
								);
					}
					
				}
				/* 1st operand is memory */
				else {
					/* extract the register operand */
					reg_src = EXE->INS_OperandReg(ins,  context,  1);

					/* 32-bit operands */
					if (EXE->REG_is_gr32(ins,  context, reg_src))
						/* propagate the tag accordingly */
						EXE->INS_InsertCall(ins,  context, IDFT_IPOINT_BEFORE,
							_xadd_m2r_opl,
							4, 
							IARG_THREAD_CONTEXT,
							IARG_UINT32, 
							(uint32_t)REG32_INDX(ins,  context, reg_src),
							IARG_MEMORYWRITE_EA
							);
					/* 16-bit operands */
					else if (EXE->REG_is_gr16(ins,  context, reg_src))
						/* propagate the tag accordingly */
						EXE->INS_InsertCall(ins,  context, IDFT_IPOINT_BEFORE,
							_xadd_m2r_opw,
							4, 
							IARG_THREAD_CONTEXT,
							IARG_UINT32, 
							(uint32_t)REG16_INDX(ins,  context, reg_src),
							IARG_MEMORYWRITE_EA
							);
					/* 8-bit operand (upper) */
					else if (EXE->REG_is_Upper8(ins,  context,reg_src))
						/* propagate the tag accordingly */
						EXE->INS_InsertCall(ins,  context, IDFT_IPOINT_BEFORE,
							_xadd_m2r_opb_u,
							4, 
							IARG_THREAD_CONTEXT,
							IARG_UINT32, 
							(uint32_t)REG8_INDX(ins,  context,reg_src),
							IARG_MEMORYWRITE_EA
							);
					/* 8-bit operand (lower) */
					else
						/* propagate the tag accordingly */
						EXE->INS_InsertCall(ins,  context, IDFT_IPOINT_BEFORE,
							_xadd_m2r_opb_l,
							4, 
							IARG_THREAD_CONTEXT,
							IARG_UINT32, 
							(uint32_t)REG8_INDX(ins,  context, reg_src),
							IARG_MEMORYWRITE_EA
							);

				}
				/* done */
				break;
			/* xlat; similar to a mov between a memory location and AL */
			case XED_ICLASS_XLAT:
				/* propagate the tag accordingly */
				EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
					m2r_xfer_opb_l,
					4, 
					IARG_THREAD_CONTEXT,
					IARG_UINT32, 
					(uint32_t)REG8_INDX(ins,  context,  EXE->REG_AL(ins,  context)),
					IARG_MEMORYREAD_EA
					);
				/* done */
				break;
			/* lodsb; similar to a mov between a memory location and AL */
			case XED_ICLASS_LODSB:	
				/* propagate the tag accordingly */
				EXE->INS_InsertPredicatedCall(ins, context, IDFT_IPOINT_BEFORE,
					m2r_xfer_opb_l,
					4, 
					IARG_THREAD_CONTEXT,
					IARG_UINT32, 
					(uint32_t)REG8_INDX(ins,  context, EXE->REG_AL(ins,  context)),
					IARG_MEMORYREAD_EA
					);

				/* done */
				break;
			/* lodsw; similar to a mov between a memory location and AX */
			case XED_ICLASS_LODSW:
				/* propagate the tag accordingly */
				EXE->INS_InsertPredicatedCall(ins, context, IDFT_IPOINT_BEFORE,
					m2r_xfer_opw,
					4, 
					IARG_THREAD_CONTEXT,
					IARG_UINT32,
					(uint32_t)REG16_INDX(ins,  context, EXE->REG_AX(ins,  context)),
					IARG_MEMORYREAD_EA
					);

				/* done */
				break;
			/* lodsd; similar to a mov between a memory location and EAX */
			case XED_ICLASS_LODSD:
				/* propagate the tag accordingly */
				EXE->INS_InsertPredicatedCall(ins, context, IDFT_IPOINT_BEFORE,
					m2r_xfer_opl,
					4, 
					IARG_THREAD_CONTEXT,
					IARG_UINT32, 
					(uint32_t)REG32_INDX(ins,  context, EXE->REG_EAX(ins,  context)),
					IARG_MEMORYREAD_EA
					);

				/* done */
				break;
			/* 
			* stosb;
			* the opposite of lodsb; however, since the instruction can
			* also be prefixed with 'rep', the analysis code moves the
			* tag information, accordingly, only once (i.e., before the
			* first repetition) -- typically this will not lead in
			* inlined code
			*/
			case XED_ICLASS_STOSB:
				/* the instruction is rep prefixed */
				if (EXE->INS_RepPrefix(ins, context)) {
					/* propagate the tag accordingly */
					EXE->INS_InsertIfPredicatedCall(ins, context, IDFT_IPOINT_BEFORE,
						rep_predicate,
						1,
						IARG_FIRST_REP_ITERATION
						);
					EXE->INS_InsertThenPredicatedCall(ins, context, IDFT_IPOINT_BEFORE,
						r2m_xfer_opbn,
						6, 
						IARG_THREAD_CONTEXT,
						IARG_MEMORYWRITE_EA,
						IARG_REG_VALUE, 
						EXE->INS_RepCountRegister(ins, context),
						IARG_REG_VALUE, 
						EXE->INS_OperandReg(ins, context, 4)
						);
					
				}
				/* no rep prefix */
				else 
					/* the instruction is not rep prefixed */
					EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
						r2m_xfer_opb_l,
						4, 
						IARG_THREAD_CONTEXT,
						IARG_MEMORYWRITE_EA,
						IARG_UINT32, 
						(uint32_t)REG8_INDX(ins, context, EXE->REG_AL(ins, context))
						);

				/* done */
				break;
			/* 
			* stosw; 
			* the opposite of lodsw; however, since the instruction can
			* also be prefixed with 'rep', the analysis code moves the
			* tag information, accordingly, only once (i.e., before the
			* first repetition) -- typically this will not lead in
			* inlined code
			*/
			case XED_ICLASS_STOSW:
				/* the instruction is rep prefixed */
				if (EXE->INS_RepPrefix(ins, context)) {
					/* propagate the tag accordingly */
					EXE->INS_InsertIfPredicatedCall(ins, context, IDFT_IPOINT_BEFORE,
						rep_predicate,
						1,
						IARG_FIRST_REP_ITERATION
						);
					EXE->INS_InsertThenPredicatedCall(ins,  context, IDFT_IPOINT_BEFORE,
						r2m_xfer_opwn,
						6, 
						IARG_THREAD_CONTEXT,
						IARG_MEMORYWRITE_EA,
						IARG_REG_VALUE, 
						EXE->INS_RepCountRegister(ins ,  context),
						IARG_REG_VALUE, 
						EXE->INS_OperandReg(ins, context, 4)
						);
				}
				/* no rep prefix */
				else
					/* the instruction is not rep prefixed */
					EXE->INS_InsertCall(ins,  context, IDFT_IPOINT_BEFORE,
						r2m_xfer_opw,
						4, 
						IARG_THREAD_CONTEXT,
						IARG_MEMORYWRITE_EA,
						IARG_UINT32, 
						(uint32_t)REG16_INDX(ins, context, EXE->REG_AX(ins ,  context))
						);

				/* done */
				break;
			/* 
			* stosd;
			* the opposite of lodsd; however, since the instruction can
			* also be prefixed with 'rep', the analysis code moves the
			* tag information, accordingly, only once (i.e., before the
			* first repetition) -- typically this will not lead in
			* inlined code
			*/
			case XED_ICLASS_STOSD:
				/* the instruction is rep prefixed */
				if (EXE->INS_RepPrefix(ins, context)) {
					/* propagate the tag accordingly */
					EXE->INS_InsertIfPredicatedCall(ins,  context, IDFT_IPOINT_BEFORE,
						rep_predicate,
						1,
						IARG_FIRST_REP_ITERATION
						);
					
					EXE->INS_InsertThenPredicatedCall(ins,  context, IDFT_IPOINT_BEFORE,
						r2m_xfer_opln,
						6, 
						IARG_THREAD_CONTEXT,
						IARG_MEMORYWRITE_EA,
						IARG_REG_VALUE, 
						EXE->INS_RepCountRegister(ins, context),
						IARG_REG_VALUE, 
						EXE->INS_OperandReg(ins , context, 4)
						);
				}
				/* no rep prefix */
				else
					EXE->INS_InsertCall(ins,  context, IDFT_IPOINT_BEFORE,
						r2m_xfer_opl,
						4,
						IARG_THREAD_CONTEXT,
						IARG_MEMORYWRITE_EA,
						IARG_UINT32, 
						(uint32_t)REG32_INDX( ins, context, EXE->REG_EAX(ins,  context))
						);

				/* done */
				break;
			/* movsd */
			case XED_ICLASS_MOVSD:
				/* propagate the tag accordingly */
				EXE->INS_InsertPredicatedCall(ins,  context, IDFT_IPOINT_BEFORE,
					m2m_xfer_opl,
					2,
					IARG_MEMORYWRITE_EA,
					IARG_MEMORYREAD_EA
					);

				/* done */
				break;
			/* movsw */
			case XED_ICLASS_MOVSW:
				/* propagate the tag accordingly */
				EXE->INS_InsertPredicatedCall(ins,  context, IDFT_IPOINT_BEFORE,
					m2m_xfer_opw,
					2,
					IARG_MEMORYWRITE_EA,
					IARG_MEMORYREAD_EA
					);

				/* done */
				break;
			/* movsb */
			case XED_ICLASS_MOVSB:
				/* propagate the tag accordingly */
				EXE->INS_InsertPredicatedCall(ins,  context, IDFT_IPOINT_BEFORE,
					m2m_xfer_opb,
					2,
					IARG_MEMORYWRITE_EA,
					IARG_MEMORYREAD_EA
					);

				/* done */
				break;
			/* sal */
			case XED_ICLASS_SALC:
				/* propagate the tag accordingly */
				EXE->INS_InsertCall(ins,  context, IDFT_IPOINT_BEFORE,
					r_clrb_l,
					3, 
					IARG_THREAD_CONTEXT,
					IARG_UINT32, 
					(uint32_t)REG8_INDX( ins,  context, EXE->REG_AL(ins,  context))
					);

				/* done */
				break;
			/* TODO: shifts are not handled (yet) */
			/* rcl */
			case XED_ICLASS_RCL:
			/* rcr */        
			case XED_ICLASS_RCR:
			/* rol */        
			case XED_ICLASS_ROL:
			/* ror */        
			case XED_ICLASS_ROR:
			/* sal/shl */
			case XED_ICLASS_SHL:
			/* sar */
			case XED_ICLASS_SAR:
			/* shr */
			case XED_ICLASS_SHR:
			/* shld */
			case XED_ICLASS_SHLD:
			case XED_ICLASS_SHRD:

				/* done */
				break;
			/* pop; mov equivalent (see above) */
			case XED_ICLASS_POP: 
				/* register operand */
				if (EXE->INS_OperandIsReg(ins, context,  0)) {
					/* extract the operand */
					reg_dst = EXE->INS_OperandReg(ins, context, 0);

					/* 32-bit operand */
					if (EXE->REG_is_gr32(ins, context,reg_dst))
						/* propagate the tag accordingly */
						EXE->INS_InsertCall(ins,  context, IDFT_IPOINT_BEFORE,
							m2r_xfer_opl,
							4, 
							IARG_THREAD_CONTEXT,
							IARG_UINT32, 
							(uint32_t)REG32_INDX(ins,  context, reg_dst),
							IARG_MEMORYREAD_EA
							);
					/* 16-bit operand */
					else
						/* propagate the tag accordingly */
						EXE->INS_InsertCall(ins,  context, IDFT_IPOINT_BEFORE,
							m2r_xfer_opw,
							4, 
							IARG_THREAD_CONTEXT,
							IARG_UINT32, 
							(uint32_t)REG16_INDX(ins,  context,  reg_dst),
							IARG_MEMORYREAD_EA
							);
				}
				/* memory operand */
				else if (EXE->INS_OperandIsMemory(ins, context, 0)) {
					/* 32-bit operand */
					if (EXE->INS_MemoryWriteSize(ins, context) ==
							BIT2BYTE(MEM_LONG_LEN))
						/* propagate the tag accordingly */
						EXE->INS_InsertCall(ins,  context, IDFT_IPOINT_BEFORE,
							m2m_xfer_opl,
							2,
							IARG_MEMORYWRITE_EA,
							IARG_MEMORYREAD_EA
							);
					/* 16-bit operand */
					else
						/* propagate the tag accordingly */
						EXE->INS_InsertCall(ins,  context, IDFT_IPOINT_BEFORE,
							m2m_xfer_opw,
							2,
							IARG_MEMORYWRITE_EA,
							IARG_MEMORYREAD_EA
							);

				}
				/* done */
				break;
			/* push; mov equivalent (see above) */
			case XED_ICLASS_PUSH:
				/* register operand */
				if (EXE->INS_OperandIsReg(ins, context, 0)) {
					/* extract the operand */
					reg_src = EXE->INS_OperandReg(ins, context, 0);

					/* 32-bit operand */
					if (EXE->REG_is_gr32(ins, context, reg_src))
						/* propagate the tag accordingly */
						EXE->INS_InsertCall(ins,  context, IDFT_IPOINT_BEFORE,
							r2m_xfer_opl,
							4,
							IARG_THREAD_CONTEXT,
							IARG_MEMORYWRITE_EA,
							IARG_UINT32, 
							(uint32_t)REG32_INDX(ins,  context, reg_src)
							);
					/* 16-bit operand */
					else
						/* propagate the tag accordingly */
						EXE->INS_InsertCall(ins,  context, IDFT_IPOINT_BEFORE,
							r2m_xfer_opw,
							4, 
							IARG_THREAD_CONTEXT,
							IARG_MEMORYWRITE_EA,
							IARG_UINT32, 
							(uint32_t)REG16_INDX(ins,  context,reg_src)
							);
				}
				/* memory operand */
				else if (EXE->INS_OperandIsMemory(ins,  context, 0)) {
					/* 32-bit operand */
					if (EXE->INS_MemoryWriteSize(ins,context) ==
							BIT2BYTE(MEM_LONG_LEN))
						/* propagate the tag accordingly */
						EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
							m2m_xfer_opl,
							2,
							IARG_MEMORYWRITE_EA,
							IARG_MEMORYREAD_EA
							);
					/* 16-bit operand */
					else
						/* propagate the tag accordingly */
						EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
							m2m_xfer_opw,
							2,
							IARG_MEMORYWRITE_EA,
							IARG_MEMORYREAD_EA
							);
				}
				/* immediate or segment operand; clean */
				else {
					/* clear n-bytes */
					switch (EXE->INS_OperandWidth(ins, context, 0)) {
						/* 4 bytes */
						case MEM_LONG_LEN:
							/* propagate the tag accordingly */
							EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
								tagmap_clrl,
								1,
								IARG_MEMORYWRITE_EA
								);

								/* done */
								break;
						/* 2 bytes */
						case MEM_WORD_LEN:
							/* propagate the tag accordingly */
							EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
								tagmap_clrw,
								1,
								IARG_MEMORYWRITE_EA
								);

								/* done */
								break;
						/* 1 byte */
						case MEM_BYTE_LEN:
							/* propagate the tag accordingly */
							EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
								tagmap_clrb,
								1,
								IARG_MEMORYWRITE_EA
								);

								/* done */
								break;
						/* make the compiler happy */
						default:
							/* done */
							break;
					}
				}

				/* done */
				break;
			/* popa;
			* similar to pop but for all the 16-bit
			* general purpose registers
			*/
			case XED_ICLASS_POPA:
				/* propagate the tag accordingly */
				EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
					m2r_restore_opw,
					2, 
					IARG_THREAD_CONTEXT,
					IARG_MEMORYREAD_EA
					);

				/* done */
				break;
			/* popad; 
			* similar to pop but for all the 32-bit
			* general purpose registers
			*/
			case XED_ICLASS_POPAD:
				/* propagate the tag accordingly */
				EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
					m2r_restore_opl,
					2, 
					IARG_THREAD_CONTEXT,
					IARG_MEMORYREAD_EA
					);

				/* done */
				break;
			/* pusha; 
			* similar to push but for all the 16-bit
			* general purpose registers
			*/
			case XED_ICLASS_PUSHA:
				/* propagate the tag accordingly */
				EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
					r2m_save_opw,
					2, 
					IARG_THREAD_CONTEXT,
					IARG_MEMORYWRITE_EA
					);

				/* done */
				break;
			/* pushad; 
			* similar to push but for all the 32-bit
			* general purpose registers
			*/
			case XED_ICLASS_PUSHAD:
				/* propagate the tag accordingly */
				EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
					r2m_save_opl,
					2, 
					IARG_THREAD_CONTEXT,
					IARG_MEMORYWRITE_EA
					);

				/* done */
				break;
			/* pushf; clear a memory word (i.e., 16-bits) */
			case XED_ICLASS_PUSHF:
				/* propagate the tag accordingly */
				EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
					tagmap_clrw,
					1,
					IARG_MEMORYWRITE_EA
					);

				/* done */
				break;
			/* pushfd; clear a double memory word (i.e., 32-bits) */
			case XED_ICLASS_PUSHFD:
				/* propagate the tag accordingly */
				EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
					tagmap_clrl,
					1,
					IARG_MEMORYWRITE_EA
					);

				/* done */
				break;
			/* call (near); similar to push (see above) */
			case XED_ICLASS_CALL_NEAR:
				/* relative target */
				if (EXE->INS_OperandIsImmediate(ins, context, 0)) {
					/* 32-bit operand */
					if (EXE->INS_OperandWidth(ins, context, 0) == MEM_LONG_LEN)
						/* propagate the tag accordingly */
						EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
							tagmap_clrl,
							1,
							IARG_MEMORYWRITE_EA
							);
					/* 16-bit operand */
					else
						/* propagate the tag accordingly */
						EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
							tagmap_clrw,
							1,
							IARG_MEMORYWRITE_EA
							);
				}
				/* absolute target; register */
				else if (EXE->INS_OperandIsReg(ins, context, 0)) {
					/* extract the source register */
					// modify by menertry
					reg_src = EXE->INS_OperandReg(ins, context, 0);

					/* 32-bit operand */
					if (EXE->REG_is_gr32(ins, context, reg_src))
						/* propagate the tag accordingly */
						EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
							tagmap_clrl,
							1,
							IARG_MEMORYWRITE_EA
							);
					/* 16-bit operand */
					else
						/* propagate the tag accordingly */
						EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
							tagmap_clrw,
							1,
							IARG_MEMORYWRITE_EA
							);
				}
				/* absolute target; memory */
				else {
					/* 32-bit operand */
					if (EXE->INS_OperandWidth(ins, context, 0) == MEM_LONG_LEN)
						/* propagate the tag accordingly */
						EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
							tagmap_clrl,
							1,
							IARG_MEMORYWRITE_EA
							);
					/* 16-bit operand */
					else
						/* propagate the tag accordingly */
						EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
							tagmap_clrw,
							1,
							IARG_MEMORYWRITE_EA
							);
				}
				/* done */
				break;
			/* 
			* leave;
			* similar to a mov between ESP/SP and EBP/BP, and a pop
			*/
			case XED_ICLASS_LEAVE:
				/* extract the operands */
				// modify by menertry
				reg_dst = EXE->INS_OperandReg(ins, context,  3);
				reg_src = EXE->INS_OperandReg(ins, context, 2);

				/* 32-bit operands */	
				if (EXE->REG_is_gr32(ins, context, reg_dst)) {
					/* propagate the tag accordingly */
					EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
						r2r_xfer_opl,
						5,
						IARG_THREAD_CONTEXT,
						IARG_UINT32, 
						(uint32_t)REG32_INDX(ins, context, reg_dst),
						IARG_UINT32, 
						(uint32_t)REG32_INDX(ins, context, reg_src)
						);
					EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
						m2r_xfer_opl,
						4,
						IARG_THREAD_CONTEXT,
						IARG_UINT32, 
						(uint32_t)REG32_INDX(ins, context, reg_src),
						IARG_MEMORYREAD_EA
						);
				}
				/* 16-bit operands */
				else {
					/* propagate the tag accordingly */
					EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
						r2r_xfer_opw,
						5, 
						IARG_THREAD_CONTEXT,
						IARG_UINT32, 
						(uint32_t)REG16_INDX(ins, context, reg_dst),
						IARG_UINT32, 
						(uint32_t)REG16_INDX(ins, context,reg_src)
						);
					EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
						m2r_xfer_opw,
						4, 
						IARG_THREAD_CONTEXT,
						IARG_UINT32, 
						(uint32_t)REG16_INDX(ins, context, reg_src),
						IARG_MEMORYREAD_EA
						);
				}
				/* done */
				break;	
			/* lea */
			case XED_ICLASS_LEA:
				/*
				* the general format of this instruction
				* is the following: dst = src_base | src_indx.
				* We move the tags of the source base and index
				* registers to the destination
				* (i.e., t[dst] = t[src_base] | t[src_indx])
				*/

				/* extract the operands */
				reg_base	= EXE->INS_MemoryBaseReg(ins, context);
				reg_indx	= EXE->INS_MemoryIndexReg(ins, context);
				reg_dst		= EXE->INS_OperandReg(ins, context, 0);

				/* no base or index register; clear the destination */
				if (reg_base == EXE->REG_INVALID(ins, context) &&
						reg_indx == EXE->REG_INVALID(ins, context)) {
					
					/* 32-bit operands */
					if (EXE->REG_is_gr32(ins, context, reg_dst))
						/* clear */
						EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
							r_clrl,
							3, 
							IARG_THREAD_CONTEXT,
							IARG_UINT32,
							(uint32_t)REG32_INDX(ins, context, reg_dst)
							);
					/* 16-bit operands */
					else 
						/* clear */
						EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
							r_clrw,
							3, 
							IARG_THREAD_CONTEXT,
							IARG_UINT32,
							(uint32_t)REG16_INDX(ins, context,reg_dst)
							);

				}
				/* base register exists; no index register */
				// modify by menertry
				if (reg_base != EXE->REG_INVALID(ins, context) &&
						reg_indx == EXE->REG_INVALID(ins, context)) {
					/* 32-bit operands */
					if (EXE->REG_is_gr32(ins, context,reg_dst))
						/* propagate the tag accordingly */
						EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
							r2r_xfer_opl,
							5, 
							IARG_THREAD_CONTEXT,
							IARG_UINT32, 
							(uint32_t)REG32_INDX(ins, context,reg_dst),
							IARG_UINT32, 
							(uint32_t)REG32_INDX(ins, context,reg_base)
							);
					/* 16-bit operands */
					else 
						/* propagate tag accordingly */
						EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
							r2r_xfer_opw,
							5, 
							IARG_THREAD_CONTEXT,
							IARG_UINT32, 
							(uint32_t)REG16_INDX(ins, context, reg_dst),
							IARG_UINT32, 
							(uint32_t)REG16_INDX(ins, context, reg_base)
							);
				}
				/* index register exists; no base register */
				if (reg_base == EXE->REG_INVALID(ins, context) &&
						reg_indx != EXE->REG_INVALID(ins, context)) {
					/* 32-bit operands */
					if (EXE->REG_is_gr32(ins, context ,reg_dst))
						/* propagate the tag accordingly */
						EXE->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE,
							r2r_xfer_opl,
							5, 
							IARG_THREAD_CONTEXT,
							IARG_UINT32, 
							(uint32_t)REG32_INDX(ins, context, reg_dst),
							IARG_UINT32, 
							(uint32_t)REG32_INDX(ins, context, reg_indx)
							);
					/* 16-bit operands */
					else
						/* propagate tag accordingly */
						EXE->INS_InsertCall(ins,  context, IDFT_IPOINT_BEFORE,
							r2r_xfer_opw,
							5, 
							IARG_THREAD_CONTEXT,
							IARG_UINT32, 
							(uint32_t)REG16_INDX(ins, context, reg_dst),
							IARG_UINT32, 
							(uint32_t)REG16_INDX(ins, context, reg_indx)
							);
				}
				/* base and index registers exist */
				// modify by menertry
				if (reg_base != EXE->REG_INVALID(ins,  context) &&
						reg_indx != EXE->REG_INVALID(ins,  context)) {
					/* 32-bit operands */
					if (EXE->REG_is_gr32(ins,  context, reg_dst))
						/* propagate the tag accordingly */
						EXE->INS_InsertCall(ins,  context, IDFT_IPOINT_BEFORE,
							_lea_r2r_opl,
							7, 
							IARG_THREAD_CONTEXT,
							IARG_UINT32, 
							(uint32_t)REG32_INDX(ins, context, reg_dst),
							IARG_UINT32, 
							(uint32_t)REG32_INDX(ins, context, reg_base),
							IARG_UINT32, 
							(uint32_t)REG32_INDX(ins, context, reg_indx)
							);
					/* 16-bit operands */
					else
						/* propagate the tag accordingly */
						EXE->INS_InsertCall(ins,  context, IDFT_IPOINT_BEFORE,
							_lea_r2r_opw,
							7, 
							IARG_THREAD_CONTEXT,
							IARG_UINT32, 
							(uint32_t)REG16_INDX(ins, context, reg_dst),
							IARG_UINT32, 
							(uint32_t)REG16_INDX(ins, context, reg_base),
							IARG_UINT32, 
							(uint32_t)REG16_INDX(ins, context,reg_indx)
							);			
				}
				/* done */
				break;
			/* cmpxchg */
			case XED_ICLASS_CMPXCHG8B:
			/* enter */
			case XED_ICLASS_ENTER:
				IDFT_LOG("unhandled operand： %s\n",EXE->INS_Disassemble(ins, context) );

				/* done */
				break;

			/* 
			* default handler
			*/
			default:
			/* (void)fprintf(stdout, "%s\n",
				INS_Disassemble(ins).c_str()); */
			break;
		}
}	


    






