/*
 * File:
 *   abi.c
 * Author(s):
 *   Pascal Felber <pascal.felber@unine.ch>
 * Description:
 *   ABI for tinySTM.
 *
 * Copyright (c) 2007-2009.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, version 2
 * of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
/* for INTEL STM COMPILER, with binary compile using intel embedded STM */
/* Notes 
 * struct _ITM_transaction has a specific structure into Intel STM Compiler.
 * 0x00 : ?
 * 0x08 : ?
 * 0x10 : Pointer to function vtable
 *
 * struct vtable 
 * (glock_stats_vtable, oSTM_stats_vtable, phase2_oSTM_stats_vtable, pSTM_stats_vtable, glock_vtable, oSTM_vtable, phase2_oSTM_vtable, pSTM_vtable)
 * 0x00 : STM_abortTransaction
 * 0x08 : STM_rollbackTransaction
 * 0x10 : STM_commitTransaction
 * 0x18 : STM_tryCommitTransaction
 * 0x20 : STM_commitTransactionToId
 * 0x28 : STM_beginTransaction
 * 0x30 : initial_oSTM_RU1  
 * 0x38 : initial_oSTM_RU1 (with pSTM_vtable it becomes trivial_RU1, used for?)
 * 0x40 : trivial_RU1
 * 0x48 : STM_RfWU1
 * 0x50 : initial_oSTM_RU2
 * 0x58 : initial_oSTM_RU2
 * 0x60 : trivial_RU2
 * 0x68 : STM_RfWU2
 * 0x70 : initial_oSTM_RU4
 * 0x78 : initial_oSTM_RU4
 * 0x80 : trivial_RU4
 * 0x88 : STM_RfWU4
 * 0x90 : initial_oSTM_RU8
 * 0x98 : initial_oSTM_RU8
 * 0xA0 : trivial_RU8
 * 0xA8 : STM_RfWU8
 * 0xB0 : initial_oSTM_RF
 * 0xB8 : initial_oSTM_RF
 * 0xC0 : trivial_RF
 * 0xC8 : STM_RfWF
 * 0xD0 : initial_oSTM_RD
 * 0xD8 : initial_oSTM_RD
 * 0xE0 : trivial_RD
 * 0xE8 : STM_RfWD
 * 0xF0 : initial_oSTM_RE
 * 0xF8 : initial_oSTM_RE
 * 0x100: trivial_RE
 * 0x108: STM_RfWE
 * 0x110: initial_oSTM_RM64
 * 0x118: initial_oSTM_RM64
 * 0x120: trivial_RM64
 * 0x128: STM_RfWM64
 * 0x130: initial_oSTM_RM128
 * 0x138: initial_oSTM_RM128
 * 0x140: trivial_RM128
 * 0x148: STM_RfWM128
 * 0x150: initial_oSTM_RCF
 * 0x158: initial_oSTM_RCF
 * 0x160: trivial_RCF
 * 0x168: STM_RfWCF
 * 0x170: initial_oSTM_RCD
 * 0x178: initial_oSTM_RCD
 * 0x180: trivial_RCD
 * 0x188: STM_RfWCD
 * 0x190: initial_oSTM_RCE
 * 0x198: initial_oSTM_RCE
 * 0x1A0: trivial_RCE
 * 0x1A8: STM_RfWCE
 * 0x1B0: STM_WU1
 * 0x1B8: STM_WU1
 * 0x1C0: trivial_WU1
 * 0x1C8: STM_WU2
 * 0x1D0: STM_WU2
 * 0x1D8: trivial_WU2
 * 0x1E0: STM_WU4
 * 0x1E8: STM_WU4
 * 0x1F0: trivial_WU4
 * 0x1F8: STM_WU8
 * 0x200: STM_WU8
 * 0x208: trivial_WU8
 * 0x210: STM_WF
 * 0x218: STM_WF
 * 0x220: trivial_WF
 * 0x228: STM_WD
 * 0x230: STM_WD
 * 0x238: trivial_WD
 * 0x240: STM_WE
 * 0x248: STM_WE
 * 0x250: trivial_WE
 * 0x258: STM_WM64
 * 0x260: STM_WM64
 * 0x268: trivial_WM64
 * 0x270: STM_WM128
 * 0x278: STM_WM128
 * 0x280: trivial_WM128
 * 0x288: STM_WCF
 * 0x290: STM_WCF
 * 0x298: trivial_WCF
 * 0x2A0: STM_WCD
 * 0x2A8: STM_WCD
 * 0x2B0: trivial_WCD
 * 0x2B8: STM_WCE
 * 0x2C0: STM_WCE
 * 0x2C8: trivial_WCE
 * 0x2D0: oSTM_memcpyRnWt
 * 0x2D8: oSTM_memcpyRnWtaR
 * 0x2E0: oSTM_memcpyRnWtaW
 * 0x2E8: oSTM_memcpyRtWn
 * 0x2F0: oSTM_memcpyRtWt
 * 0x2F8: oSTM_memcpyRtWtaR
 * 0x300: oSTM_memcpyRtWtaW
 * 0x308: oSTM_memcpyRtaRWn
 * 0x310: oSTM_memcpyRtaRWt
 * 0x318: oSTM_memcpyRtaRWtaR
 * 0x320: oSTM_memcpyRtaRWtaW
 * 0x328: oSTM_memcpyRtaWWn
 * 0x330: oSTM_memcpyRtaWWt
 * 0x338: oSTM_memcpyRtaWWtaR
 * 0x340: oSTM_memcpyRtaWWtaW
 * 0x348: STM_LU1
 * 0x350: STM_LU2
 * 0x358: STM_LU4
 * 0x360: STM_LU8
 * 0x368: STM_LF
 * 0x370: STM_LD
 * 0x378: STM_LE
 * 0x380: STM_LM64
 * 0x388: STM_LM128
 * 0x390: STM_LCF
 * 0x398: STM_LCD
 * 0x3A0: STM_LCE
 * 0X3A8: STM_LB
 * 0x3B0: oSTM_memsetW
 * 0x3B8: oSTM_memsetWaR
 * 0x3C0: oSTM_memsetWaW
 * 0x3C8: oSTM_memmoveRnWt
 * 0x3D0: oSTM_memmoveRnWtaR
 * 0x3D8: oSTM_memmoveRnWtaW
 * 0x3E0: oSTM_memmoveRtWn
 * 0x3E8: oSTM_memmoveRtWt
 * 0x3F0: oSTM_memmoveRtWtaR
 * 0x3F8: oSTM_memmoveRtWtaW
 * 0x400: oSTM_memmoveRtaRWn
 * 0x408: oSTM_memmoveRtaRWt
 * 0x410: oSTM_memmoveRtaRWtaR
 * 0x418: oSTM_memmoveRtaRWtaW
 * 0x420: oSTM_memmoveRtaWWn
 * 0x428: oSTM_memmoveRtaWWt
 * 0x430: oSTM_memmoveRtaWWtaR
 * 0x438: oSTM_memmoveRtaWWtaW
 * ---
 */
static struct vtable {
/*0x00*/ void * (*abortTransaction)();
/*0x08*/ void * (*rollbackTransaction)();
/*0x10*/ void * (*commitTransaction)();
/*0x18*/ void * (*tryCommitTransaction)();
/*0x20*/ void * (*commitTransactionToId)();
/*0x28*/ void * (*beginTransaction)();
/*0x30*/ void * (*STM_RU1)();
/*0x38*/ void * (*STMX_RU1)();
/*0x40*/ void * (*trivial_RU1)();
/*0x48*/ void * (*RfWU1)();
/*0x50*/ void * (*STM_RU2)();
/*0x58*/ void * (*STMX_RU2)();
/*0x60*/ void * (*trivial_RU2)();
/*0x68*/ void * (*STM_RfWU2)();
/*0x70*/ void * (*STM_RU4)();
/*0x78*/ void * (*STMX_RU4)();
/*0x80*/ void * (*trivial_RU4)();
/*0x88*/ void * (*STM_RfWU4)();
/*0x90*/ void * (*STM_RU8)();
/*0x98*/ void * (*STMX_RU8)();
/*0xA0*/ void * (*trivial_RU8)();
/*0xA8*/ void * (*STM_RfWU8)();
/*0xB0*/ void * (*STM_RF)();
/*0xB8*/ void * (*STMX_RF)();
/*0xC0*/ void * (*trivial_RF)();
/*0xC8*/ void * (*STM_RfWF)();
/*0xD0*/ void * (*STM_RD)();
/*0xD8*/ void * (*STMX_RD)();
/*0xE0*/ void * (*trivial_RD)();
/*0xE8*/ void * (*STM_RfWD)();
/*0xF0*/ void * (*STM_RE)();
/*0xF8*/ void * (*STMX_RE)();
/*0x100*/ void * (*trivial_RE)();
/*0x108*/ void * (*STM_RfWE)();
/*0x110*/ void * (*STM_RM64)();
/*0x118*/ void * (*STMX_RM64)();
/*0x120*/ void * (*trivial_RM64)();
/*0x128*/ void * (*STM_RfWM64)();
/*0x130*/ void * (*STM_RM128)();
/*0x138*/ void * (*STMX_RM128)();
/*0x140*/ void * (*trivial_RM128)();
/*0x148*/ void * (*STM_RfWM128)();
/*0x150*/ void * (*STM_RCF)();
/*0x158*/ void * (*STMX_RCF)();
/*0x160*/ void * (*trivial_RCF)();
/*0x168*/ void * (*STM_RfWCF)();
/*0x170*/ void * (*STM_RCD)();
/*0x178*/ void * (*STMX_RCD)();
/*0x180*/ void * (*trivial_RCD)();
/*0x188*/ void * (*STM_RfWCD)();
/*0x190*/ void * (*STM_RCE)();
/*0x198*/ void * (*STMX_RCE)();
/*0x1A0*/ void * (*trivial_RCE)();
/*0x1A8*/ void * (*STM_RfWCE)();
/*0x1B0*/ void * (*STM_WU1)();
/*0x1B8*/ void * (*STMX_WU1)();
/*0x1C0*/ void * (*trivial_WU1)();
/*0x1C8*/ void * (*STM_WU2)();
/*0x1D0*/ void * (*STMX_WU2)();
/*0x1D8*/ void * (*trivial_WU2)();
/*0x1E0*/ void * (*STM_WU4)();
/*0x1E8*/ void * (*STMX_WU4)();
/*0x1F0*/ void * (*trivial_WU4)();
/*0x1F8*/ void * (*STM_WU8)();
/*0x200*/ void * (*STMX_WU8)();
/*0x208*/ void * (*trivial_WU8)();
/*0x210*/ void * (*STM_WF)();
/*0x218*/ void * (*STMX_WF)();
/*0x220*/ void * (*trivial_WF)();
/*0x228*/ void * (*STM_WD)();
/*0x230*/ void * (*STMX_WD)();
/*0x238*/ void * (*trivial_WD)();
/*0x240*/ void * (*STM_WE)();
/*0x248*/ void * (*STMX_WE)();
/*0x250*/ void * (*trivial_WE)();
/*0x258*/ void * (*STM_WM64)();
/*0x260*/ void * (*STMX_WM64)();
/*0x268*/ void * (*trivial_WM64)();
/*0x270*/ void * (*STM_WM128)();
/* 0x278: STM_WM128
 * 0x280: trivial_WM128
 * 0x288: STM_WCF
 * 0x290: STM_WCF
 * 0x298: trivial_WCF
 * 0x2A0: STM_WCD
 * 0x2A8: STM_WCD
 * 0x2B0: trivial_WCD
 * 0x2B8: STM_WCE
 * 0x2C0: STM_WCE
 * 0x2C8: trivial_WCE
 * 0x2D0: oSTM_memcpyRnWt
 * 0x2D8: oSTM_memcpyRnWtaR
 * 0x2E0: oSTM_memcpyRnWtaW
 * 0x2E8: oSTM_memcpyRtWn
 * 0x2F0: oSTM_memcpyRtWt
 * 0x2F8: oSTM_memcpyRtWtaR
 * 0x300: oSTM_memcpyRtWtaW
 * 0x308: oSTM_memcpyRtaRWn
 * 0x310: oSTM_memcpyRtaRWt
 * 0x318: oSTM_memcpyRtaRWtaR
 * 0x320: oSTM_memcpyRtaRWtaW
 * 0x328: oSTM_memcpyRtaWWn
 * 0x330: oSTM_memcpyRtaWWt
 * 0x338: oSTM_memcpyRtaWWtaR
 * 0x340: oSTM_memcpyRtaWWtaW
 * 0x348: STM_LU1
 * 0x350: STM_LU2
 * 0x358: STM_LU4
 * 0x360: STM_LU8
 * 0x368: STM_LF
 * 0x370: STM_LD
 * 0x378: STM_LE
 * 0x380: STM_LM64
 * 0x388: STM_LM128
 * 0x390: STM_LCF
 * 0x398: STM_LCD
 * 0x3A0: STM_LCE
 * 0X3A8: STM_LB
 * 0x3B0: oSTM_memsetW
 * 0x3B8: oSTM_memsetWaR
 * 0x3C0: oSTM_memsetWaW
 * 0x3C8: oSTM_memmoveRnWt
 * 0x3D0: oSTM_memmoveRnWtaR
 * 0x3D8: oSTM_memmoveRnWtaW
 * 0x3E0: oSTM_memmoveRtWn
 * 0x3E8: oSTM_memmoveRtWt
 * 0x3F0: oSTM_memmoveRtWtaR
 * 0x3F8: oSTM_memmoveRtWtaW
 * 0x400: oSTM_memmoveRtaRWn
 * 0x408: oSTM_memmoveRtaRWt
 * 0x410: oSTM_memmoveRtaRWtaR
 * 0x418: oSTM_memmoveRtaRWtaW
 * 0x420: oSTM_memmoveRtaWWn
 * 0x428: oSTM_memmoveRtaWWt
 * 0x430: oSTM_memmoveRtaWWtaR
 * 0x438: oSTM_memmoveRtaWWtaW
*/
} /*vtable_t*/ ;

static struct vtable vtable_tiny;


static struct vtable vtable_tiny = {  
  .abortTransaction      = _ITM_abortTransaction,
  .commitTransaction     = _ITM_commitTransaction,  
  .beginTransaction      = _ITM_beginTransaction,  
};

typedef struct {
  uint64_t dummy1;
  uint64_t dummy2;
  struct vtable * funcs;
} _ITM_transaction;

/* ################################################################### *
 * FUNCTIONS
 * ################################################################### */

_ITM_transaction *_ITM_getTransaction(void)
{
  struct stm_tx *tx = stm_current_tx();
#if 0
  _ITM_transaction *itx;
#endif
  if (tx == NULL) {
    /* Thread not initialized: must create transaction */
    tx = stm_init_thread();
  }

#if 0
  if defined __PIC__ && defined TM_INTEL
  /* TODO: for Intel STM Compiler, we need to set a pointer to vtable at offset 0x10 of _ITM_transaction */
  itx = (_ITM_transaction *)tx;
  itx->funcs = &vtable_tiny;
  return itx;
#endif
  return (_ITM_transaction *)tx;
}

/* The _ITM_beginTransaction is defined in assembly (context.S)  */

/*
 * Intel Def
 * uint32_t _ITM_beginTransaction(_ITM_transaction *td,
                               uint32_t __properties,
                               const _ITM_srcLocation *__src)
 * GNU GCC Def
 * uint32_t _ITM_beginTransaction(uint32_t, ...) REGPARM;
 */

void
_ITM_registerTMCloneTable (void *t, size_t size)
{
	printf("%s\n",__func__);
}

void _ITM_deregisterTMCloneTable (void * a) {
	printf("%s\n",__func__);
	
}

#if 0
extern void * _ITM_beginTransaction();
static struct vtable vtable_tiny = {  
  .abortTransaction      = _ITM_abortTransaction,
  .commitTransaction     = _ITM_commitTransaction,  
  .beginTransaction      = _ITM_beginTransaction,  
  .STM_RfWU4             = _ITM_RfWU4,
  .trivial_WU4           = _ITM_WU4,
};

#endif

/* TODO unknown function in ABI paper but exists in libitmdyn.so */
void _ITM_registerThreadFinalization(_ITM_transaction *td, void * __func, void * __arg)
{

}

