#include "libicedft_types.h"
#include "libicedft_util.h"
#include "libicedft_api.h"




/* 
 * REG-to-VCPU map;
 * get the register index in the VCPU structure
 * given a PIN register (32-bit regs)
 *
 * @reg:	the PIN register
 * returns:	the index of the register in the VCPU
 */
uint32_t
REG32_INDX(idft_ins_t* ins , idft_context_t * context, idft_reg_t reg)
{
	/* result; for the 32-bit registers the mapping is easy */

	idft_reg_t reg_eax;
	uint32_t indx ;	
	
	reg_eax =  context->executer_api->REG_eax(ins , context);
	indx = reg - (reg_eax -GPR_EAX);
	
	/* 
	 * sanity check;
	 * unknown registers are mapped to the scratch
	 * register of the VCPU
	 */
	if (unlikely(indx > GPR_NUM))
		indx = GPR_NUM;
	
	/* return the index */
	return indx;	
}




void r_clrl(thread_ctx_t *thread_ctx, uint32_t reg)
{

	thread_ctx->vcpu.gpr[reg] = 0;

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
	xed_iclass_enum_t ins_indx = (xed_iclass_enum_t)context->executer_api->INS_Opcode(ins, context);

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
			if (context->executer_api->INS_OperandIsImmediate(ins, context, 1))
				break;


			/* both operands are registers */
			if (context->executer_api->INS_MemoryOperandCount(ins, context) == 0) {
				/* extract the operands */
				reg_dst = context->executer_api->INS_OperandReg(ins, context, 0);
				reg_src = context->executer_api->INS_OperandReg(ins, context, 1);

				/* 32-bit operands */
				if(context->executer_api->REG_is_gr32(ins, context, reg_dst)){
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
								context->executer_api->INS_InsertCall(ins, context, IDFT_IPOINT_BEFORE, r_clrl, 1,  REG32_INDX( ins, context ,reg_dst) );


							}



					}


				}


			}

	
	}

}

    






