/*
 * kry_eval.c: spreadsheet evaluation primitives (cell addressing and
 * ulp-grace rounding), matching goffice's go-math / parse-util behavior.
 */

#include "kry_eval.h"
#include <math.h>
#include <string.h>

int
eval_col_name (int col, char *out)
{
	int c = col + 1;
	int n = 0;
	char buf[8];
	int i = 0;

	while (c > 0) {
		int rem = (c - 1) % 26;
		buf[n] = (char)('A' + rem);
		n++;
		c = (c - 1) / 26;
	}
	for (i = 0; i < n; i++)
		out[i] = buf[n - 1 - i];
	out[n] = 0;
	return n;
}

int
eval_sheet_name_needs_quote (const char *name)
{
	int i = 1;
	int c0 = name[0];

	if (c0 == 0)
		return 0;
	if ((c0 >= 'A' && c0 <= 'Z') || (c0 >= 'a' && c0 <= 'z')) {
		/* count letters; digits after letters are fine */
	} else if (c0 == '_' || c0 == '.') {
		/* unquoted */
	} else {
		return 1;
	}

	while (name[i] != 0) {
		int c = name[i];
		if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) {
			/* fine */
		} else if (c >= '0' && c <= '9') {
			/* fine unless pure-digit leading form like A1 */
		} else if (c == '.' || c == '_') {
			/* fine */
		} else {
			return 1;
		}
		i++;
	}
	return 0;
}

int
eval_sheet_prefix (const char *name, char *out)
{
	int n = 0;
	int i = 0;

	if (name[0] == 0) {
		out[0] = '!';
		out[1] = 0;
		return 1;
	}
	if (eval_sheet_name_needs_quote (name)) {
		out[0] = '\'';
		n = 1;
		while (name[i] != 0) {
			if (name[i] == '\'' && n < 250) {
				out[n] = '\'';
				n++;
			}
			if (n < 250) {
				out[n] = name[i];
				n++;
			}
			i++;
		}
		out[n] = '\'';
		n++;
	} else {
		while (name[i] != 0 && n < 250) {
			out[n] = name[i];
			n++;
			i++;
		}
	}
	out[n] = '!';
	n++;
	out[n] = 0;
	return n;
}

double
eval_sub_epsilon (double x)
{
	int e = 0;
	double mant;

	if (!isfinite (x) || x == 0)
		return x;
	mant = frexp (fabs (x), &e);
	if (x < 0)
		return -ldexp (mant + 2.220446049250313e-16 / 2, e);
	return ldexp (mant - 2.220446049250313e-16 / 2, e);
}

double
eval_add_epsilon (double x)
{
	int e = 0;
	double mant;

	if (!isfinite (x) || x == 0)
		return x;
	mant = frexp (fabs (x), &e);
	if (x < 0)
		return -ldexp (mant - 2.220446049250313e-16 / 2, e);
	return ldexp (mant + 2.220446049250313e-16 / 2, e);
}

double
eval_fake_floor (double x)
{
	if (x == floor (x))
		return x;
	return floor (eval_add_epsilon (x));
}

double
eval_fake_round (double x)
{
	double y;

	if (x == floor (x))
		return x;
	y = eval_fake_floor (fabs (x) + 0.5);
	return (x < 0) ? -y : y;
}

double
eval_fake_trunc (double x)
{
	if (x == floor (x))
		return x;
	if (x >= 0)
		return floor (eval_add_epsilon (x));
	return -floor (eval_add_epsilon (-x));
}

double
eval_fake_ceil (double x)
{
	if (x == floor (x))
		return x;
	return ceil (eval_sub_epsilon (x));
}
