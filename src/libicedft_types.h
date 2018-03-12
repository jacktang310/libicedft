#ifndef _LIBICEDFT_CORE

#define _LIBICEDFT_CORE

#include <stdint.h>
#include "libicedft_opcode.h"

#define IDFT_REG_TYPE uint32_t 

//the type represent number type which has the same length with vcpu register
#define idft_reg_t IDFT_REG_TYPE

//the type represent memory address type 
#define ADDRINT IDFT_REG_TYPE

typedef int BOOL; 

typedef enum{
  IDFT_IPOINT_BEFORE,
  IDFT_IPOINT_AFTER
  
}idft_point_t;


typedef enum{
  IARG_ADDRINT,  //Type: ADDRINT. Constant value (additional arg required).
  IARG_UINT32,  //Type: UINT32. Constant (additional integer arg required).
  IARG_UINT64,  //Type: UINT64. Constant (additional UINT64 arg required).
  IARG_MEMORYREAD_EA, //Type: ADDRINT. Effective address of a memory read, only valid if INS_IsMemoryRead is true and at IPOINT_BEFORE.
  IARG_MEMORYWRITE_EA,  //Type: ADDRINT. Effective address of a memory write, only valid at IPOINT_BEFORE. 
  IARG_THREAD_CONTEXT, // thread_ctx_t pointer
  IARG_REG_VALUE,
  IARG_FIRST_REP_ITERATION,
  IARG_LAST

}idft_arg_type_t;


typedef struct idft_ins
{

	xed_iclass_enum_t ins_indx;

  void* ins_content;    // this is provided by Executer which use the dft engine
}  idft_ins_t;


typedef uint32_t (*f_0_t)(idft_ins_t*, void * );
typedef idft_reg_t (*f_1_t)(idft_ins_t*, void *, idft_reg_t );

typedef char* (*f_0_r_s_t)(idft_ins_t*, void * );

typedef uint32_t (*f_f_t)(idft_ins_t*, void *,  uint32_t action, void* func,  uint32_t  arg_count, ... );




typedef struct idft_executer_api
{
  //get instruction's opcode
  //param 1: pointer to a instruction
  //param 2: idft_context_t context
  //return opcode in xed_iclass_enum_t
  f_0_t INS_Opcode;

  //check the instucion 's one operand is immediate
  //param 1: pointer to a instruction
  //param 2: idft_context_t context
  //param 3: which operand  (from 0)
  //return: 0: no   1: yes
  f_1_t INS_OperandIsImmediate;

  //check the instucion 's one operand is memory reference
  //param 1: pointer to a instruction
  //param 2: idft_context_t context
  //param 3: which operand  (from 0)
  //return: 0: no   1: yes
  f_1_t INS_OperandIsMemory;

  //check the instucion 's one operand is register reference
  //param 1: pointer to a instruction
  //param 2: idft_context_t context
  //param 3: which operand  (from 0)
  //return: 0: no   1: yes
  f_1_t INS_OperandIsReg;

  //get the instruction's memory reference operand count
  //param 1: pointer to a instruction
  //param 2: idft_context_t context  
  //return: the count of memory reference operand
  f_0_t INS_MemoryOperandCount;

  //get the instruction's one reg operand
  //param 1: pointer to a instruction
  //param 2: idft_context_t context  
  //param 3: which operand (from 0)
  //return: the executer reg id
  f_1_t INS_OperandReg;

  //get the instruction's one reg width
  //param 1: pointer to a instruction
  //param 2: idft_context_t context  
  //param 3: which operand (from 0)
  //return： the operand 's width
  f_1_t INS_OperandWidth;

  //add a call 
  //param 1: pointer to a instruction
  //param 2: idft_context_t context  
  //param 3: instruction before or after
  //param 4: dft 's call back
  //param 5: following variable argument list count
  //param 6: ...  
  f_f_t INS_InsertCall;

  //add a call which is called or not depent on condition: for handle CMOVZ
  //param 1: pointer to a instruction
  //param 2: idft_context_t context  
  //param 3: instruction before or after
  //param 4: dft 's call back
  //param 5: following variable argument list count
  //param 6: ...  
  f_f_t INS_InsertPredicatedCall;


  //Insert a call to funptr relative to an INS. If funptr returns a non-zero ADDRINT, then the immediately following "then" analysis call is executed.
  //param 1: pointer to a instruction
  //param 2: idft_context_t context  
  //param 3: instruction before or after
  //param 4: dft 's call back
  //param 5: following variable argument list count
  //param 6: ...  
  f_f_t INS_InsertIfCall;


  //insert a call to funptr relative to an INS. The function is called only if the immediately preceding "if" analysis call returns a non-zero value.
  //param 1: pointer to a instruction
  //param 2: idft_context_t context  
  //param 3: instruction before or after
  //param 4: dft 's call back
  //param 5: following variable argument list count
  //param 6: ...  
  f_f_t INS_InsertThenCall;
	
  // Insert a call to funptr relative to an INS. If funptr returns a non-zero ADDRINT and the instruction has a true predicate, then the immediately following "then" analysis call is executed. If the instruction is not predicated, then this function is identical to INS_InsertIfCall.
  //param 1: pointer to a instruction
  //param 2: idft_context_t context  
  //param 3: instruction before or after
  //param 4: dft 's call back
  //param 5: following variable argument list count
  //param 6: ...  
  f_f_t INS_InsertIfPredicatedCall;

  //Insert a call to funptr relative to an INS. The function is called only if the immediately preceding "if" analysis call returns a non-zero value and the instruction's predicate is true. See INS_InsertIfPredicatedCall for details of the semantics of mixing INS_InsertThenPredicatedCall with INS_InsertIfCall (and all the other possibilities).
  //param 1: pointer to a instruction
  //param 2: idft_context_t context  
  //param 3: instruction before or after
  //param 4: dft 's call back
  //param 5: following variable argument list count
  //param 6: ...  
  f_f_t INS_InsertThenPredicatedCall;

  //get executer reg id for save rep count
  //param 1: pointer to a instruction
  //param 2: idft_context_t context 
  //return: executer reg id for save rep count
  f_0_t INS_RepCountRegister;

  //get the instruction's disassemble string
  //param 1: pointer to a instruction
  //param 2: idft_context_t context 
  f_0_r_s_t INS_Disassemble;


  //get the instruction's write memory size
  //param 1: pointer to a instruction
  //param 2: idft_context_t context 
  f_0_t INS_MemoryWriteSize;


  //true if this operand is implied by the opcode (e.g. the stack write in a push instruction)
  //param 1: pointer to a instruction
  //param 2: idft_context_t context 
  //param 3: which operand (from 0)
  //return: 0: no   1: yes
  f_1_t INS_OperandIsImplicit;

  //true if this instruction is rep prefix
  //param 1: pointer to a instruction
  //param 2: idft_context_t context 
  //return: 0: no   1: yes
  f_0_t INS_RepPrefix;

  //The base register used in the instruction's memory operand, or REG_INVALID() if there is no base register.
  //param 1: pointer to a instruction
  //param 2: idft_context_t context 
  //return: executer reg id for memory base reg , or , REG_INVALID
  f_0_t INS_MemoryBaseReg;

  //get executer reg id for memory index register
  //param 1: pointer to a instruction
  //param 2: idft_context_t context 
  //return: executer reg id for memory index register
  f_0_t INS_MemoryIndexReg;


  //get invalid execute id
  //param 1: pointer to a instruction
  //param 2: idft_context_t context 
  //return: invalid executer reg id
  f_0_t REG_INVALID;


  //check the regsiter is 32 bit 
  //param 1: pointer to a instruction
  //param 2: idft_context_t context
  //param 3: the executer reg id    
  //return: 0： no , 1:yes
  f_1_t REG_is_gr32;

  //check the register is 16 bit 
  //param 1: pointer to a instruction
  //param 2: idft_context_t context
  //param 3: the executer reg id   
  //return: 0： no , 1:yes   
  f_1_t REG_is_gr16;


  //check the register is 8 bit 
  //param 1: pointer to a instruction
  //param 2: idft_context_t context
  //param 3: the executer reg id   
  //return: 0： no , 1:yes  
  f_1_t REG_is_gr8;

  //check the register is upper 8 register
  //param 1: pointer to a instruction
  //param 2: idft_context_t context
  //param 3: the executer reg id   
  //return: 0： no , 1:yes   
  f_1_t REG_is_Upper8;

  //check the register is lower 8 register
  //param 1: pointer to a instruction
  //param 2: idft_context_t context
  //param 3: the executer reg id   
  //return: 0： no , 1:yes   
  f_1_t REG_is_Lower8;

  //check the register is segment register
  //param 1: pointer to a instruction
  //param 2: idft_context_t context
  //param 3: the executer reg id   
  //return: 0： no , 1:yes   
  f_1_t REG_is_seg;


  //Executer 32bit reg to vcpu reg map
  //param 1: pointer to a instruction , which can is ignore
  //param 2: idft_context_t context
  //param 3: Executer reg 
  //return: the reg index in vcpu (see vcpu_ctx_t comments) 
  f_1_t REG32_INDX;

  //Executer 16bit reg to vcpu reg map
  //param 1: pointer to a instruction , which can is ignore
  //param 2: idft_context_t context
  //param 3: Executer reg 
  //return: the reg index in vcpu (see vcpu_ctx_t comments) 
  f_1_t REG16_INDX;

  //Executer 8bit reg to vcpu reg map
  //param 1: pointer to a instruction , which can is ignore
  //param 2: idft_context_t context
  //param 3: Executer reg 
  //return: the reg index in vcpu (see vcpu_ctx_t comments) 
  f_1_t REG8_INDX;

  //get executer AH reg id
  //param 1: pointer to a instruction , which can is ignore
  //param 2: idft_context_t context
  //return: the executer AH reg id 
  f_0_t REG_AH;

  //get executer AL reg id
  //param 1: pointer to a instruction , which can is ignore
  //param 2: idft_context_t context
  //return: the executer AL reg id 
  f_0_t REG_AL;

  //get executer AX reg id
  //param 1: pointer to a instruction , which can is ignore
  //param 2: idft_context_t context
  //return: the executer AX reg id 
  f_0_t REG_AX;

  //get executer EAX reg id
  //param 1: pointer to a instruction , which can is ignore
  //param 2: idft_context_t context
  //return: the executer EAX reg id 
  f_0_t REG_EAX;


  //get executer DX reg id
  //param 1: pointer to a instruction , which can is ignore
  //param 2: idft_context_t context
  //return: the executer DX reg id 
  f_0_t REG_DX;

  //get executer EDX reg id
  //param 1: pointer to a instruction , which can is ignore
  //param 2: idft_context_t context
  //return: the executer EDX reg id 
  f_0_t REG_EDX;

}idft_executer_api_t;


typedef struct idft_context 
{
  idft_executer_api_t*  executer_api;
  


}idft_context_t;







#endif


