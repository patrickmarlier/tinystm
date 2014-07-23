
#if defined(__x86_64__)
#pragma message("Compiling for x86_64 architecure")
#include "arch_x86.S"
#elif defined(__i386__)
#pragma message("Compiling for i386 architecure")
#include "arch_x86.S"
#elif defined(__sparc__)
#error SPARC arch is not yet supported
#else
#error Unsupported architecture
#endif
