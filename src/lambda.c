#define LAMBDA_IMPLEMENTATION
#define LAMBDA_DEFINITIONS
#ifdef __APPLE__
#define LAMBDA_USE_MPROTECT 1
#else
#define LAMBDA_USE_MPROTECT 0
#endif
#include "lambda.h"
