#ifndef _LIBICEDFT_CORE

#define _LIBICEDFT_CORE

#include <stdint.h>
#include "libicedft_opcode.h"

#define IDFT_REG_TYPE uint32_t 


#define idft_reg_t IDFT_REG_TYPE

typedef enum{
  IDFT_IPOINT_BEFORE,
  IDFT_IPOINT_AFTER
  
}idft_point_t;


typedef struct idft_ins
{

	xed_iclass_enum_t ins_indx;

  void* ins_content;    // this is provided by Executer which use the dft engine

 

}  idft_ins_t;


typedef uint32_t (*f_0_t)(idft_ins_t*, void * );
typedef idft_reg_t (*f_1_t)(idft_ins_t*, void *, idft_reg_t );

typedef uint32_t (*f_f_t)(idft_ins_t*, void *,  idft_reg_t , void* func, uint32_t  arg_count, ... );




typedef struct idft_executer_api
{
  f_0_t INS_Opcode;
  f_1_t INS_OperandIsImmediate;
  f_0_t INS_MemoryOperandCount;
  f_1_t INS_OperandReg;
  f_f_t INS_InsertCall;

  f_1_t REG_is_gr32;

  f_0_t REG_eax;


}idft_executer_api_t;


typedef struct idft_context 
{
  idft_executer_api_t*  executer_api;
  


}idft_context_t;







#endif


