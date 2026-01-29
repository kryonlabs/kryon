/*
 * The authors of this software are Rob Pike and Ken Thompson.
 *              Copyright (c) 2002 by Lucent Technologies.
 * Permission to use, copy, modify, and distribute this software for any
 * purpose without fee is hereby granted, provided that this entire notice
 * is included in all copies of any software which is or includes a copy
 * or modification of this software and in all copies of the supporting
 * documentation for such software.
 * THIS SOFTWARE IS BEING PROVIDED "AS IS", WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTY.  IN PARTICULAR, NEITHER THE AUTHORS NOR LUCENT TECHNOLOGIES MAKE ANY
 * REPRESENTATION OR WARRANTY OF ANY KIND CONCERNING THE MERCHANTABILITY
 * OF THIS SOFTWARE OR ITS FITNESS FOR ANY PARTICULAR PURPOSE.
 */
#include "lib9.h"
#include "fmtdef.h"

static int
fmtRuneStrFlush(Fmt *f)
{
	Rune *s;
	int n;

	if(f->start == nil)
		return 0;
	n = (int)f->farg;
	n += 256;
	f->farg = (void*)n;
	s = f->start;
	f->start = realloc(s, n * sizeof(Rune));
	if(f->start == nil){
		free(s);
		f->to = nil;
		f->stop = nil;
		return 0;
	}
	f->to = (Rune*)f->start + ((Rune*)f->to - s);
	f->stop = (Rune*)f->start + n - 1;
	return 1;
}

int
runefmtstrinit(Fmt *f)
{
	int n;

	memset(f, 0, sizeof(*f));
	f->runes = 1;
	n = 32;
	f->start = malloc(n * sizeof(Rune));
	if(f->start == nil)
		return -1;
	f->to = f->start;
	f->stop = (Rune*)f->start + n - 1;
	f->flush = fmtRuneStrFlush;
	f->farg = (void*)n;
	f->nfmt = 0;
	return 0;
}

/*
 * print into an allocated Rune string buffer
 */
Rune*
runevsmprint(char *fmt, va_list args)
{
	Fmt f;
	int n;

	if(runefmtstrinit(&f) < 0)
		return nil;
	va_copy(f.args, args);
	n = dofmt(&f, fmt);
	va_end(f.args);
	if(n < 0){
		free(f.start);
		f.start = nil;
		return nil;
	}
	return runefmtstrflush(&f);
}
