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

typedef uint32_t (*f_f_t)(idft_ins_t*, void *,  idft_reg_t action, void* func,  uint32_t  arg_count, ... );




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


  //get the instruction's disassemble string
  //param 1: pointer to a instruction
  //param 2: idft_context_t context 
  f_0_r_s_t INS_Disassemble;


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








}idft_executer_api_t;


typedef struct idft_context 
{
  idft_executer_api_t*  executer_api;
  


}idft_context_t;







#endif


