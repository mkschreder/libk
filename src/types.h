#pragma once

#include <inttypes.h>
#include <stddef.h>
#include <string.h>

#define __init __attribute__ ((constructor))
#define __exit __attribute__ ((destructor))

#ifndef __unused
#define __unused __attribute__((unused))
#endif

#ifdef AVR
typedef long ssize_t; 
#endif


