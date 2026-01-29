#include "lib9.h"
#include <stdlib.h>
#include <string.h>

/*
 * Plan 9 malloc extensions for Linux
 */

/* mallocz - allocate size bytes and clear if clr is non-zero */
void*
mallocz(size_t size, int clr)
{
	void *p = malloc(size);
	if(p == nil)
		return nil;
	if(clr)
		memset(p, 0, size);
	return p;
}

/* msize - return size of malloc'd block (not accurately supported on glibc) */
size_t
msize(void *p)
{
	/* glibc malloc_size functions exist but are not portable */
	/* Return 0 as a safe default - this is used for debugging in Plan 9 */
	USED(p);
	return 0;
}

/* setmalloctag - set allocation tag for debugging (stub on Linux) */
void
setmalloctag(void *p, uintptr tag)
{
	USED(p);
	USED(tag);
	/* Not supported on glibc - stub for compatibility */
}

/* setrealloctag - set reallocation tag for debugging (stub on Linux) */
void
setrealloctag(void *p, uintptr tag)
{
	USED(p);
	USED(tag);
	/* Not supported on glibc - stub for compatibility */
}

/* getmalloctag - get allocation tag (stub on Linux) */
uintptr
getmalloctag(void *p)
{
	USED(p);
	return 0;
}

/* getrealloctag - get reallocation tag (stub on Linux) */
uintptr
getrealloctag(void *p)
{
	USED(p);
	return 0;
}

/* malloctopoolblock - get pool block (stub on Linux) */
void*
malloctopoolblock(void *p)
{
	USED(p);
	return nil;
}
