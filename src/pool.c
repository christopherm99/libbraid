#define POOL_IMPLEMENTATION
#define POOL_DEFINITIONS
#ifdef __APPLE__
#include <sys/mman.h>
#define POOL_USE_MPROTECT 1
#define POOL_MPROTECT_RESTORE PROT_READ | PROT_EXEC
#else
#define POOL_USE_MPROTECT 0
#endif
#include "pool.h"
