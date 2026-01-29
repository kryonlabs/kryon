#include	"lib9.h"

/*
 * Binary search for rune c in table t of n pairs (start, end).
 * Each entry is ne elements wide.
 * Returns pointer to matching entry or nil if not found.
 */
Rune*
_runebsearch(Rune c, Rune *t, int n, int ne)
{
	Rune *p;
	int m;

	while(n > 0) {
		m = n / 2;
		p = t + m * ne;
		if(c >= p[0]) {
			if(n == 1 || c <= p[1])
				return p;
			t = p + ne;
			n = n - m - 1;
		} else
			n = m;
	}
	return nil;
}

#include "runetypebody-6.2.0.h"
