#ifndef DFCOMPAT_H
#define DFCOMPAT_H

#define _GNU_SOURCE

#include <sys/types.h>

#ifndef __DECONST
#define __DECONST(type, var)    ((type)(uintptr_t)(const void *)(var))
#endif

#ifndef HAVE_STRLCPY
size_t strlcpy(char *, const char *, size_t);
#endif

#ifndef HAVE_REALLOCF
void *reallocf(void *, size_t);
#endif

#ifdef __APPLE__ & __MACH__
#define HAVE_GETPROGNAME 1
#define st_atim st_atimespec
#define st_mtim st_mtimespec
#define st_ctim st_ctimespec
#endif

#ifndef HAVE_GETPROGNAME
const char *getprogname(void);
#endif

#endif /* DFCOMPAT_H */
