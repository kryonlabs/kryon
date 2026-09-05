/*
 * kry_quad.c: double-double arithmetic, compensated summation, and
 * quad-precision dense QR linear algebra.
 *
 * The two-sum/two-product splitting and the pow/exp/floor algorithms
 * follow the construction popularized by goffice's go-quad.c (itself
 * building on Dekker 1971).  The accumulator is Shewchuk's
 * growing-partials scheme.  Behavior is bit-exact reproducible C99.
 */

#include "kry_quad.h"
#include <math.h>
#include <float.h>
#include <string.h>

static const double QUAD_SPLIT_CONST = 134217729.0; /* 2^27 + 1 */

void
quad_init (Quad *r, double hv)
{
	r->h = hv;
	r->l = 0.0;
}

double
quad_val (const Quad *a)
{
	return a->h + a->l;
}

void
quad_neg (Quad *r, const Quad *a)
{
	r->h = -a->h;
	r->l = -a->l;
}

void
quad_zero (Quad *r)
{
	r->h = 0.0;
	r->l = 0.0;
}

void
quad_add (Quad *r, const Quad *a, const Quad *b)
{
	double rr = a->h + b->h;
	double s = fabs (a->h) > fabs (b->h)
		? a->h - rr + b->h + b->l + a->l
		: b->h - rr + a->h + a->l + b->l;
	r->h = rr + s;
	r->l = rr - r->h + s;
}

void
quad_sub (Quad *r, const Quad *a, const Quad *b)
{
	double rr = a->h - b->h;
	double s = fabs (a->h) > fabs (b->h)
		? a->h - rr - b->h - b->l + a->l
		: -b->h - rr + a->h + a->l - b->l;
	r->h = rr + s;
	r->l = rr - r->h + s;
}

static double
quad_split (double x_in, double *h, double *t)
{
	double x = x_in;
	double p = x * QUAD_SPLIT_CONST;
	if (!isfinite (p) && isfinite (x)) {
		x *= DBL_EPSILON;
		p = x * QUAD_SPLIT_CONST;
		*h = x - p + p;
		*t = x - *h;
		*h *= 1.0 / DBL_EPSILON;
		*t *= 1.0 / DBL_EPSILON;
	} else {
		*h = x - p + p;
		*t = x - *h;
	}
	return x;
}

void
quad_mul12 (Quad *r, double xv, double yv)
{
	double hx, tx, hy, ty, p, q;
	double x = quad_split (xv, &hx, &tx);
	double y = quad_split (yv, &hy, &ty);
	(void)x; (void)y;
	p = hx * hy;
	q = hx * ty + tx * hy;
	r->h = p + q;
	r->l = p - r->h + q + tx * ty;
}

void
quad_mul (Quad *r, const Quad *a, const Quad *b)
{
	Quad c;
	quad_mul12 (&c, a->h, b->h);
	c.l = a->h * b->l + a->l * b->h + c.l;
	r->h = c.h + c.l;
	r->l = c.h - r->h + c.l;
}

void
quad_div (Quad *r, const Quad *a, const Quad *b)
{
	double c_h = a->h / b->h;
	Quad u;
	quad_mul12 (&u, c_h, b->h);
	double c_l = (a->h - u.h - u.l + a->l - c_h * b->l) / b->h;
	r->h = c_h + c_l;
	r->l = c_h - r->h + c_l;
}

void
quad_sqrt (Quad *r, const Quad *a)
{
	if (a->h > 0) {
		double c_h = sqrt (a->h);
		Quad u;
		quad_mul12 (&u, c_h, c_h);
		double c_l = (a->h - u.h - u.l + a->l) * 0.5 / c_h;
		r->h = c_h + c_l;
		r->l = c_h - r->h + c_l;
	} else {
		r->h = 0.0;
		r->l = 0.0;
	}
}

void
quad_sqrt1pm1 (Quad *r, const Quad *a)
{
	Quad x0, x1, d;
	static const Quad one = { 1.0, 0.0 };

	quad_add (&x0, a, &one);
	quad_sqrt (&x0, &x0);
	quad_sub (&x0, &x0, &one);

	/* Newton step.  */
	quad_mul (&x1, &x0, &x0);
	quad_add (&x1, &x1, a);
	quad_add (&d, &x0, &one);
	quad_add (&d, &d, &d);
	quad_div (r, &x1, &d);
}

void
quad_scalbn (Quad *r, const Quad *a, int n)
{
	r->h = scalbn (a->h, n);
	r->l = scalbn (a->l, n);
}

int
quad_compare (const Quad *a, const Quad *b)
{
	int sa = a->h < 0;
	int sb = b->h < 0;
	Quad d;

	if (sa != sb)
		return sa ? -1 : 1;

	quad_sub (&d, a, b);
	if (d.h > 0) return 1;
	if (d.h == 0) return 0;
	return -1;
}

void
quad_floor (Quad *r, const Quad *a)
{
	Quad qh, ql, q, rr;
	static const Quad one = { 1.0, 0.0 };

	quad_init (&qh, floor (a->h));
	quad_init (&ql, floor (a->l));
	quad_add (&q, &qh, &ql);

	/* Due to dual floors, we might be off by one.  */
	quad_sub (&rr, a, &q);
	if (quad_val (&rr) < 0) {
		quad_sub (r, &q, &one);
	} else {
		quad_sub (&rr, &rr, &one);
		if (quad_val (&rr) < 0) {
			*r = q;
		} else {
			quad_add (r, &q, &one);
		}
	}
}

void
quad_rescale_base (Quad *x, double *e)
{
	int xe = ilogb (quad_val (x));
	if (xe != 0) {
		Quad qs;
		quad_init (&qs, scalbn (1.0, -xe));
		quad_mul (x, x, &qs);
		*e += xe;
	}
}

void
quad_pow_int (Quad *r, double *exp2, const Quad *x, const Quad *y)
{
	Quad xn;
	double xe = 0;
	double dy = quad_val (y);

	xn = *x;
	*exp2 = 0;
	quad_init (r, 1.0);
	quad_rescale_base (&xn, &xe);

	while (dy > 0) {
		if (fmod (dy, 2) != 0) {
			quad_mul (r, r, &xn);
			*exp2 += xe;
			quad_rescale_base (r, exp2);
			dy -= 1;
			if (dy == 0) break;
		}
		dy /= 2;
		quad_mul (&xn, &xn, &xn);
		xe *= 2;
		quad_rescale_base (&xn, &xe);
	}
}

void
quad_pow_frac (Quad *r, const Quad *x, const Quad *y)
{
	Quad qx, qr, qy = *y;
	double dy;
	int x1m;
	static const Quad one = { 1.0, 0.0 };

	x1m = fabs (quad_val (x)) >= 0.5;
	if (x1m)
		quad_sub (&qx, x, &one);
	else
		qx = *x;

	quad_init (&qr, 1.0);

	while ((dy = quad_val (&qy)) > 0) {
		quad_add (&qy, &qy, &qy);
		if (x1m) {
			quad_sqrt1pm1 (&qx, &qx);
			if (quad_val (&qx) == 0)
				break;
		} else {
			quad_sqrt (&qx, &qx);
			if (quad_val (&qx) >= 0.5) {
				x1m = 1;
				quad_sub (&qx, &qx, &one);
			}
		}
		if (dy >= 0.5) {
			Quad qp;
			quad_sub (&qy, &qy, &one);
			quad_mul (&qp, &qx, &qr);
			if (x1m) {
				quad_add (&qr, &qr, &qp);
			} else {
				qr = qp;
			}
		}
	}

	*r = qr;
}

void
quad_pow (Quad *r, double *expb, const Quad *x, const Quad *y)
{
	static const Quad one = { 1.0, 0.0 };
	static const Quad half = { 0.5, 0.0 };

	if (expb) *expb = 0;

	if (y->h == 0 || quad_compare (x, &one) == 0) {
		quad_init (r, 1.0);
		return;
	}
	if (x->h == 0 && y->h > 0) {
		quad_zero (r);
		return;
	}
	if (x->h != x->h) {
		r->h = x->h;
		r->l = 0;
		return;
	}
	if (y->h != y->h) {
		r->h = y->h;
		r->l = 0;
		return;
	}
	if (quad_compare (y, &one) == 0) {
		*r = *x;
		return;
	}
	if (x->h > 0 && quad_compare (y, &half) == 0) {
		quad_sqrt (r, x);
		return;
	}

	if (quad_val (y) < 0) {
		Quad my, zero, one2;
		quad_zero (&zero);
		quad_sub (&my, &zero, y);
		quad_pow (r, expb, x, &my);
		one2 = one;
		quad_div (r, &one2, r);
		if (expb) *expb = -*expb;
		return;
	} else {
		Quad qew, qef, yint, qf;
		double exp2ew = 0;

		quad_floor (&yint, y);
		quad_sub (&qf, y, &yint);
		quad_pow_int (&qew, &exp2ew, x, &yint);
		quad_pow_frac (&qef, x, &qf);
		quad_mul (r, &qew, &qef);
		if (expb) {
			*expb = exp2ew;
		} else {
			quad_scalbn (r, r, (int) exp2ew);
		}
	}
}

void
quad_exp (Quad *r, double *expb, const Quad *a)
{
	static const Quad quad_e = { 2.718281828459045, 1.4456468917292502e-16 };

	if (a->h != a->h) {
		r->h = a->h;
		r->l = 0;
		return;
	}
	quad_pow (r, expb, &quad_e, a);
}

/* ------------------------------------------------------------------ */

void
quad_acc_clear (QuadAcc *acc)
{
	acc->len = 0;
}

void
quad_acc_add (QuadAcc *acc, double x)
{
	unsigned ui = 0, uj;

	for (uj = 0; uj < (unsigned)acc->len; uj++) {
		double y = acc->partials[uj];
		double hi, lo;
		if (fabs (x) < fabs (y)) {
			double t = x;
			x = y;
			y = t;
		}
		hi = x + y;
		if (!isfinite (hi)) {
			x = hi;
			ui = 0;
			break;
		}
		lo = y - (hi - x);
		if (lo != 0) {
			acc->partials[ui] = lo;
			ui++;
		}
		x = hi;
	}
	acc->len = (int)ui + 1;
	acc->partials[ui] = x;
}

void
quad_acc_add_quad (QuadAcc *acc, const Quad *x)
{
	quad_acc_add (acc, x->h);
	quad_acc_add (acc, x->l);
}

double
quad_acc_value (const QuadAcc *acc)
{
	double sum = 0;
	int ui;
	for (ui = 0; ui < acc->len; ui++)
		sum += acc->partials[ui];
	return sum;
}

/* ------------------------------------------------------------------ */

QuadQR
quad_qr_new (const QuadMatrix *A)
{
	QuadQR qr;
	int qdet = 1;
	QuadMatrix R;

	memset (&qr, 0, sizeof (qr));
	memset (&R, 0, sizeof (R));
	int i, j, k;
	int m = A->m, n = A->n;
	Quad L, L2, L2p, s, p;
	Quad tmp[QUAD_MATRIX_MAX];

	qr.V.m = m; qr.V.n = n;
	qr.R.m = n; qr.R.n = n;
	R.m = m; R.n = n;

	for (i = 0; i < m; i++)
		for (j = 0; j < n; j++)
			R.data[i][j] = A->data[i][j];

	for (k = 0; k < n; k++) {
		quad_zero (&L2);
		quad_zero (&L2p);
		for (i = m - 1; i >= k; i--) {
			qr.V.data[i][k] = R.data[i][k];
			quad_mul (&s, &qr.V.data[i][k], &qr.V.data[i][k]);
			L2p = L2;
			quad_add (&L2, &L2, &s);
		}
		quad_sqrt (&L, &L2);
		if (quad_val (&qr.V.data[k][k]) < 0)
			quad_sub (&qr.V.data[k][k], &qr.V.data[k][k], &L);
		else
			quad_add (&qr.V.data[k][k], &qr.V.data[k][k], &L);

		/* Normalize v[k] to length 1.  */
		quad_mul (&s, &qr.V.data[k][k], &qr.V.data[k][k]);
		quad_add (&L2p, &L2p, &s);
		quad_sqrt (&L, &L2p);
		if (quad_val (&L) == 0)
			continue;
		for (i = k; i < m; i++)
			quad_div (&qr.V.data[i][k], &qr.V.data[i][k], &L);

		/* Householder matrices have determinant -1.  */
		qdet = -qdet;

		for (j = k; j < n; j++) {
			quad_zero (&tmp[j]);
			for (i = k; i < m; i++) {
				quad_mul (&p, &qr.V.data[i][k], &R.data[i][j]);
				quad_add (&tmp[j], &tmp[j], &p);
			}
		}

		for (j = k; j < n; j++) {
			for (i = k; i < m; i++) {
				quad_mul (&p, &qr.V.data[i][k], &tmp[j]);
				quad_add (&p, &p, &p);
				quad_sub (&R.data[i][j], &R.data[i][j], &p);
			}
		}

		for (i = k + 1; i < m; i++)
			quad_zero (&R.data[i][k]);
	}

	for (i = 0; i < n; i++)
		for (j = 0; j < n; j++)
			qr.R.data[i][j] = R.data[i][j];

	qr.qdet = qdet;
	return qr;
}

void
quad_qr_multiply_qt (QuadQR *qr, Quad *x)
{
	int i, k;

	for (k = 0; k < qr->V.n; k++) {
		Quad s, p;
		quad_zero (&s);
		for (i = k; i < qr->V.m; i++) {
			quad_mul (&p, &x[i], &qr->V.data[i][k]);
			quad_add (&s, &s, &p);
		}
		quad_add (&s, &s, &s);
		for (i = k; i < qr->V.m; i++) {
			quad_mul (&p, &s, &qr->V.data[i][k]);
			quad_sub (&x[i], &x[i], &p);
		}
	}
}

int
quad_matrix_back_solve (QuadMatrix *R, Quad *x, const Quad *b, int allow_degenerate)
{
	int i, j;
	int n = R->m;
	Quad d, Rii, p;

	for (i = n - 1; i >= 0; i--) {
		d = b[i];
		Rii = R->data[i][i];
		if (quad_val (&Rii) == 0) {
			if (allow_degenerate) {
				quad_zero (&x[i]);
				continue;
			} else {
				while (i >= 0) {
					quad_zero (&x[i]);
					i--;
				}
				return 1;
			}
		}
		for (j = i + 1; j < n; j++) {
			quad_mul (&p, &R->data[i][j], &x[j]);
			quad_sub (&d, &d, &p);
		}
		quad_div (&x[i], &d, &Rii);
	}
	return 0;
}

int
quad_matrix_fwd_solve (QuadMatrix *R, Quad *x, const Quad *b, int allow_degenerate)
{
	int i, j;
	int n = R->m;
	Quad d, Rii, p;

	for (i = 0; i < n; i++) {
		d = b[i];
		Rii = R->data[i][i];
		if (quad_val (&Rii) == 0) {
			if (allow_degenerate) {
				quad_zero (&x[i]);
				continue;
			} else {
				while (i < n) {
					quad_zero (&x[i]);
					i++;
				}
				return 1;
			}
		}
		for (j = 0; j < i; j++) {
			quad_mul (&p, &R->data[j][i], &x[j]);
			quad_sub (&d, &d, &p);
		}
		quad_div (&x[i], &d, &Rii);
	}
	return 0;
}

void
quad_matrix_eigen_range (QuadMatrix *A, double *emin, double *emax)
{
	int i;
	double abs_e = fabs (quad_val (&A->data[0][0]));
	*emin = abs_e;
	*emax = abs_e;
	for (i = 1; i < A->m; i++) {
		abs_e = fabs (quad_val (&A->data[i][i]));
		if (abs_e < *emin) *emin = abs_e;
		if (abs_e > *emax) *emax = abs_e;
	}
}

void
quad_matrix_determinant (QuadMatrix *A, Quad *res)
{
	QuadQR qr;
	Quad a, b;
	int i;

	if (A->m == 1) {
		*res = A->data[0][0];
		return;
	}
	if (A->m == 2) {
		quad_mul (&a, &A->data[0][0], &A->data[1][1]);
		quad_mul (&b, &A->data[1][0], &A->data[0][1]);
		quad_sub (res, &a, &b);
		return;
	}
	qr = quad_qr_new (A);
	quad_init (res, qr.qdet);
	for (i = 0; i < qr.R.n; i++)
		quad_mul (res, res, &qr.R.data[i][i]);
}

int
quad_matrix_inverse (QuadMatrix *A, double threshold, QuadMatrix *Z)
{
	QuadQR qr;
	int n = A->n;
	int i, k;
	int ok;
	double emin, emax;
	Quad x[QUAD_MATRIX_MAX];
	Quad qtk[QUAD_MATRIX_MAX];

	qr = quad_qr_new (A);
	Z->m = n;
	Z->n = n;
	quad_matrix_eigen_range (&qr.R, &emin, &emax);
	ok = emin > emax * threshold;
	for (k = 0; ok && k < n; k++) {
		for (i = 0; i < n; i++)
			quad_init (&qtk[i], i == k ? 1.0 : 0.0);
		quad_qr_multiply_qt (&qr, qtk);
		if (quad_matrix_back_solve (&qr.R, x, qtk, 0)) {
			ok = 0;
			break;
		}
		for (i = 0; i < n; i++)
			Z->data[i][k] = x[i];
	}
	return ok;
}

void
quad_matrix_multiply (QuadMatrix *C, const QuadMatrix *A, const QuadMatrix *B)
{
	int i, j, k;
	Quad p;

	for (i = 0; i < C->m; i++) {
		for (j = 0; j < C->n; j++) {
			Quad acc;
			quad_zero (&acc);
			for (k = 0; k < A->n; k++) {
				quad_mul (&p, &A->data[i][k], &B->data[k][j]);
				quad_add (&acc, &acc, &p);
			}
			C->data[i][j] = acc;
		}
	}
}

void
quad_matrix_transpose (QuadMatrix *A, const QuadMatrix *B)
{
	int i, j;
	for (i = 0; i < A->m; i++)
		for (j = 0; j < A->n; j++)
			A->data[i][j] = B->data[j][i];
}
