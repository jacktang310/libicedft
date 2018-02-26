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


#include <errno.h>
#include <string.h>
// modify by menertry
// #include <unistd.h>
#include <stdlib.h>

#include "libicedft_api.h"
#include "libicedft_core.h"
#include "tagmap.h"
#include "branch_pred.h"


/*
 * initialization of the core tagging engine;
 * it must be called before using everything else
 *
 *
 * returns: 0 on success, 1 on error
 */
int
libdft_init( idft_executer_api_t* executer_api ,  idft_context_t ** pcontext)
{

    idft_context_t * context = NULL;

	/* initialize the tagmap; optimized branch */
	if (tagmap_alloc())
		/* tagmap initialization failed */
		return 1;

	// modify by menertry
    // context = malloc(sizeof(idft_context_t));
	context = (idft_context_t *)dr_global_alloc(sizeof(idft_context_t));

	*pcontext = context;

	/* success */
	return 0;
}

/*
 * stop the execution of the application inside the
 * tag-aware VM; the execution of the application
 * is not interrupted
 *
 * NOTE: it also performs the appropriate cleanup
 */
void
libdft_die(idft_context_t * context)
{
	/* deallocate the resources needed for the tagmap */
	tagmap_free();

	// modify by menertry
    // free(context);
	dr_global_free(context, sizeof(idft_context_t));
}


  //Executer reg to vcpu reg map
  //param 1: pointer to a instruction , which can be NULL
  //param 2: idft_context_t context
  //param 3: Executer reg 
  //return: the reg index in vcpu (see vcpu_ctx_t comments) 
uint32_t
REG32_INDX(idft_ins_t* ins , idft_context_t * context, idft_reg_t reg)
{

	uint32_t indx ;	
	
	indx =  (uint32_t) context->executer_api->REG32_INDX(NULL , context, reg);
	
	/* return the index */
	return indx;	
}




  //Executer 16bit reg to vcpu reg map
  //param 1: pointer to a instruction , which can be NULL
  //param 2: idft_context_t context
  //param 3: Executer reg 
  //return: the reg index in vcpu (see vcpu_ctx_t comments) 
uint32_t
REG16_INDX(idft_ins_t* ins , idft_context_t * context, idft_reg_t reg)
{
	uint32_t indx ;	

	indx = (uint32_t) context->executer_api->REG16_INDX(NULL , context, reg);

	return indx;
}

  //Executer 8bit reg to vcpu reg map
  //param 1: pointer to a instruction , which can be NULL
  //param 2: idft_context_t context
  //param 3: Executer reg 
  //return: the reg index in vcpu (see vcpu_ctx_t comments) 
uint32_t
REG8_INDX(idft_ins_t* ins , idft_context_t * context, idft_reg_t reg)
{

	uint32_t indx ;	

	indx = (uint32_t) context->executer_api->REG8_INDX(NULL , context, reg);

	return indx; 
}



