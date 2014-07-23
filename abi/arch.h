#ifndef _ARCH_H
#define _ARCH_H

/* Set the calling convention */
#if defined(__x86_64__)
#define _ITM_CALL_CONVENTION
#elif defined(__i386__)
#define _ITM_CALL_CONVENTION __attribute__((regparm(2)))
#else
#error Unknow calling convention
// NOTE: windows use __fastcall calling convention
#endif

#endif /* _ARCH_H */
