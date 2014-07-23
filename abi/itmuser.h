#if !defined(_ITMUSER_H)
# define _ITMUSER_H

# ifdef __cplusplus
extern "C"
{
# endif                          /* __cplusplus */

/* FIXME Check with Pascal if we use the unmodified intel file or? */

#include <stddef.h>
#include <stdint.h>

/* FIXME Use tm_macros.h */
#if (defined(_TM))
#  define TM_PURE __declspec (tm_pure)
#else
#  define TM_PURE 
#endif

#if defined(__x86_64__)
#define _ITM_CALL_CONVENTION
#elif defined(__i386__)
#define _ITM_CALL_CONVENTION __attribute__((regparm(2)))
#else
#error Unknow calling convention
// NOTE: windows use __fastcall calling convention
#endif

struct _ITM_transactionS;
//! Opaque transaction descriptor.
typedef struct _ITM_transactionS _ITM_transaction;

typedef void (_ITM_CALL_CONVENTION * _ITM_userUndoFunction)(void *);
typedef void (_ITM_CALL_CONVENTION * _ITM_userCommitFunction)(void *);

//! Opaque transaction tag
typedef uint32 _ITM_transactionId;

typedef enum 
{
    outsideTransaction = 0,  
    inRetryableTransaction,
    inIrrevocableTransaction
} _ITM_howExecuting;

TM_PURE
extern _ITM_transaction * _ITM_CALL_CONVENTION _ITM_getTransaction (void);

TM_PURE
extern _ITM_howExecuting _ITM_CALL_CONVENTION _ITM_inTransaction (_ITM_transaction * __td);

TM_PURE 
extern int _ITM_CALL_CONVENTION _ITM_getThreadnum(void) ;

TM_PURE 
extern void _ITM_CALL_CONVENTION _ITM_addUserCommitAction (_ITM_transaction * __td, 
                                                       _ITM_userCommitFunction __commit,
                                                       _ITM_transactionId resumingTransactionId,
                                                       void * __arg);

TM_PURE 
extern void _ITM_CALL_CONVENTION _ITM_addUserUndoAction (_ITM_transaction * __td, 
                                                       const _ITM_userUndoFunction __undo, void * __arg);

#define _ITM_noTransactionId (1) // Id for non-transactional code.

TM_PURE 
extern _ITM_transactionId _ITM_CALL_CONVENTION _ITM_getTransactionId(_ITM_transaction * __td);

TM_PURE
extern void _ITM_CALL_CONVENTION _ITM_dropReferences (_ITM_transaction * __td, const void * __start, size_t __size);

TM_PURE
extern void _ITM_CALL_CONVENTION _ITM_userError (const char *errString, int exitCode);

# ifdef __cplusplus
} /* extern "C" */
# endif                          /* __cplusplus */
#endif /* defined (_ITMUSER_H) */
