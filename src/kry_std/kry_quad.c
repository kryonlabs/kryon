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

/* ------------------------------------------------------------------ */
/* Constants as correctly-rounded double-double values (base-2 digits
 * of pi/e/ln2/sqrt2, matching go_quad_constant8's normalization). */
static const Quad QUAD_PI     = { 3.141592653589793, 1.2246467991473532e-16 };
const Quad QUAD_2PI_EXPORT = { 6.283185307179586, 2.4492935982947064e-16 };
const Quad QUAD_PI_EXPORT  = { 3.141592653589793, 1.2246467991473532e-16 };
static const Quad QUAD_PIHALF = { 1.5707963267948966, 6.123233995736766e-17 };
static const Quad QUAD_LN2    = { 0.6931471805599453, 2.3190468138462996e-17 };
static const Quad QUAD_SQRT2  = { 1.4142135623730951, -9.667293313452913e-17 };

void
quad_log (Quad *res, const Quad *a)
{
	double da = quad_val (a);

	if (da == 0)
		quad_init (res, -INFINITY);
	else if (da < 0)
		quad_init (res, NAN);
	else if (!isfinite (da))
		*res = *a;
	else {
		Quad as, xi, yi, dx, dl;
		int e;

		/* Scale down to near 1. */
		frexp (da, &e);
		if (da < 1 / sqrt (2)) e--;
		quad_scalbn (&as, a, -e);

		/* Initial approximation. */
		quad_init (&xi, log (as.h));

		/* Newton step. */
		quad_exp (&yi, NULL, &xi);
		quad_sub (&dx, &as, &yi);
		quad_div (&dx, &dx, &yi);
		quad_add (&xi, &xi, &dx);

		/* Adjust for scaling. */
		quad_init (&dl, e);
		quad_mul (&dl, &QUAD_LN2, &dl);
		quad_add (&xi, &xi, &dl);

		*res = xi;
	}
}

void
quad_hypot (Quad *res, const Quad *a, const Quad *b)
{
	int e;
	Quad qa, qb, qn;
	double maxh;

	qa = (a->h < 0) ? (Quad){ -a->h, -a->l } : *a;
	qb = (b->h < 0) ? (Quad){ -b->h, -b->l } : *b;

	if (qa.h == 0) { *res = qb; return; }
	if (qb.h == 0) { *res = qa; return; }
	if (qa.h == INFINITY || qb.h == INFINITY) { quad_init (res, INFINITY); return; }
	if (isnan (qa.h) || isnan (qb.h)) { quad_init (res, NAN); return; }

	maxh = (qa.h > qb.h) ? qa.h : qb.h;
	frexp (maxh, &e);

	quad_scalbn (&qa, &qa, -e);
	quad_mul (&qa, &qa, &qa);
	quad_scalbn (&qb, &qb, -e);
	quad_mul (&qb, &qb, &qb);
	quad_add (&qn, &qa, &qb);
	quad_sqrt (&qn, &qn);
	quad_scalbn (res, &qn, e);
}

static void
quad_ihypot (Quad *res, const Quad *a)
{
	Quad qp, one;
	quad_mul (&qp, a, a);
	quad_init (&one, 1.0);
	quad_sub (&qp, &one, &qp);
	quad_sqrt (res, &qp);
}

/* Carlson's AGM algorithm for arcsin/arccos/arctan. */
enum { AGM_ARCSIN, AGM_ARCCOS, AGM_ARCTAN };

static void
quad_agm_internal (Quad *res, int method, const Quad *x)
{
	Quad g, gp, dk[20], dpk[20], qr, qrp, qnum;
	int n, k;
	int converged = 0;
	static const Quad one = { 1.0, 0.0 };
	static const Quad half = { 0.5, 0.0 };

	qrp = one;
	qrp.h = 0; qrp.l = 0;

	switch (method) {
	case AGM_ARCSIN:
		quad_ihypot (&dpk[0], x);
		gp = one;
		qnum = *x;
		break;
	case AGM_ARCCOS:
		dpk[0] = *x;
		gp = one;
		quad_ihypot (&qnum, x);
		break;
	case AGM_ARCTAN:
		dpk[0] = one;
		quad_hypot (&gp, x, &one);
		qnum = *x;
		break;
	default:
		quad_init (res, NAN);
		return;
	}

	for (n = 1; n < 20; n++) {
		Quad f;

		quad_add (&dk[0], &dpk[0], &gp);
		quad_mul (&dk[0], &dk[0], &half);

		quad_mul (&g, &dk[0], &gp);
		quad_sqrt (&g, &g);

		for (k = 1; k <= n; k++) {
			quad_init (&f, ldexp (1.0, -2 * k));
			quad_mul (&dk[k], &f, &dpk[k - 1]);
			quad_sub (&dk[k], &dk[k - 1], &dk[k]);
			quad_init (&f, 1.0 - ldexp (1.0, -2 * k));
			quad_div (&dk[k], &dk[k], &f);
		}

		quad_div (&qr, &qnum, &dk[n]);
		quad_sub (&qrp, &qrp, &qr);
		if (fabs (qrp.h) <= ldexp (fabs (qr.h), -2 * (53 - 1))) {
			converged = 1;
			break;
		}

		qrp = qr;
		gp = g;
		for (k = 0; k <= n; k++) dpk[k] = dk[k];
	}

	(void)converged;
	*res = qr;
}

void
quad_asin (Quad *res, const Quad *a)
{
	Quad aa, aam1;
	static const Quad sone = { 1.0, 0.0 };
	aa = (a->h < 0) ? (Quad){ -a->h, -a->l } : *a;
	quad_sub (&aam1, &aa, &sone);
	if (aam1.h > 0) {
		quad_init (res, NAN);
		return;
	}
	quad_agm_internal (res, AGM_ARCSIN, a);
}

void
quad_acos (Quad *res, const Quad *a)
{
	Quad aa, aam1;
	static const Quad sone = { 1.0, 0.0 };
	aa = (a->h < 0) ? (Quad){ -a->h, -a->l } : *a;
	quad_sub (&aam1, &aa, &sone);
	if (aam1.h > 0) {
		quad_init (res, NAN);
		return;
	}
	quad_agm_internal (res, AGM_ARCCOS, &aa);
	if (a->h < 0)
		quad_sub (res, &QUAD_PI, res);
}

static int
quad_atan2_special (const Quad *y, const Quad *x, double *f)
{
	double dy = quad_val (y);
	double dx = quad_val (x);

	if (dy == 0) {
		*f = (dx >= 0 ? 0 : +1);
		return 1;
	}
	if (dx == 0) {
		*f = (dy >= 0 ? 0.5 : -0.5);
		return 1;
	}
	if (fabs (fabs (dx) - fabs (dy)) < 1e-10) {
		Quad d;
		quad_sub (&d, x, y);
		if (d.h == 0) {
			*f = (dy >= 0 ? 0.25 : -0.75);
			return 1;
		}
		quad_add (&d, x, y);
		if (d.h == 0) {
			*f = (dy >= 0 ? +0.75 : -0.25);
			return 1;
		}
	}
	return 0;
}

void
quad_atan2 (Quad *res, const Quad *y, const Quad *x)
{
	double f;
	double dy = quad_val (y);
	double dx = quad_val (x);
	Quad qr;

	if (quad_atan2_special (y, x, &f)) {
		Quad qf;
		quad_init (&qf, f);
		quad_mul (res, &qf, &QUAD_PI);
		return;
	}

	if (fabs (dx) >= fabs (dy)) {
		quad_div (&qr, y, x);
		quad_agm_internal (res, AGM_ARCTAN, &qr);
	} else {
		Quad qa;
		quad_div (&qr, x, y);
		quad_agm_internal (res, AGM_ARCTAN, &qr);
		qa = QUAD_PIHALF;
		if (qr.h < 0) { qa.h = -qa.h; qa.l = -qa.l; }
		quad_sub (res, &qa, res);
	}

	if (dx < 0) {
		if (dy > 0)
			quad_add (res, res, &QUAD_PI);
		else
			quad_sub (res, res, &QUAD_PI);
	}
}

void
quad_atan2pi (Quad *res, const Quad *y, const Quad *x)
{
	double f;

	if (quad_atan2_special (y, x, &f)) {
		quad_init (res, f);
		return;
	}
	quad_atan2 (res, y, x);
	quad_div (res, res, &QUAD_PI);
}

/* Reduce a (mod 2) into [-0.5, 0.5] with quadrant bits. */
static void
quad_reduce_half (Quad *res, const Quad *a, int *pk)
{
	int k = 0;
	Quad qxr = *a;

	if (a->h < 0) {
		Quad aa;
		aa.h = -a->h; aa.l = -a->l;
		quad_reduce_half (&qxr, &aa, &k);
		qxr.h = -qxr.h; qxr.l = -qxr.l;
		k = 4 - k;
		if (qxr.h <= -0.25 && qxr.l == 0) {
			Quad qh = { 0.5, 0.0 };
			quad_add (&qxr, &qxr, &qh);
			k += 3;
		}
	} else {
		Quad qdx;
		quad_init (&qdx, qxr.h - fmod (qxr.h, 2));
		quad_sub (&qxr, &qxr, &qdx);
		quad_init (&qdx, qxr.h - fmod (qxr.h, 2));
		quad_sub (&qxr, &qxr, &qdx);
		{
			Quad qone = { 1.0, 0.0 };
			Quad qh = { 0.5, 0.0 };
			if (qxr.h >= 1) { quad_sub (&qxr, &qxr, &qone); k += 2; }
			if (qxr.h >= 0.5) { quad_sub (&qxr, &qxr, &qh); k++; }
			if (qxr.h > 0.25) { quad_sub (&qxr, &qxr, &qh); k++; }
		}
	}

	*pk = (k & 3);
	*res = qxr;
}

static void
quad_do_sinpi (Quad *res, const Quad *a, int k)
{
	Quad qr, one;

	if (a->h == 0) {
		quad_init (&qr, k & 1);
	} else if (a->h == 0.25 && a->l == 0) {
		quad_init (&one, 1.0);
		quad_div (&qr, &one, &QUAD_SQRT2);
	} else {
		Quad api;
		quad_mul (&api, a, &QUAD_PI);

		/* Sine with Newton refinement via arcsin/arccos. */
		if (k & 1) {
			Quad qn, qd, qq, qabs;
			qabs = (api.h < 0) ? (Quad){ -api.h, -api.l } : api;
			quad_init (&qr, cos (qabs.h));
			quad_acos (&qn, &qr);
			quad_sub (&qn, &qn, &qabs);
			quad_ihypot (&qd, &qr);
			quad_mul (&qq, &qn, &qd);
			quad_add (&qr, &qr, &qq);
		} else {
			Quad qn, qd, qq;
			quad_init (&qr, sin (api.h));
			quad_asin (&qn, &qr);
			quad_sub (&qn, &qn, &api);
			quad_ihypot (&qd, &qr);
			quad_mul (&qq, &qn, &qd);
			quad_sub (&qr, &qr, &qq);
		}
	}

	if (k & 2) {
		qr.h = 0 - qr.h;
		qr.l = 0 - qr.l;
	}

	*res = qr;
}

void
quad_sinpi (Quad *res, const Quad *a)
{
	int k;
	Quad a0;
	quad_reduce_half (&a0, a, &k);
	quad_do_sinpi (res, &a0, k);
}

void
quad_cospi (Quad *res, const Quad *a)
{
	int k;
	Quad a0;
	quad_reduce_half (&a0, a, &k);
	quad_do_sinpi (res, &a0, k + 1);
}

void
quad_mulmod1 (Quad *dst, const Quad *qa_, double b)
{
	Quad qa = *qa_, qfb, qfa, qp, res;
	double wb, wa;
	int ea, eb, de;

	frexp (quad_val (&qa), &ea);
	frexp (b, &eb);
	if (ea + eb <= 0) {
		quad_init (&qfb, b);
		quad_mul (dst, &qfb, &qa);
		return;
	}

	de = (ea - eb) / 2;
	if (de) {
		double f = ldexp (1.0, de);
		b *= f;
		qa.h /= f;
		qa.l /= f;
	}

	wb = round (b);
	b -= wb;
	quad_init (&qfb, b);

	wa = round (quad_val (&qa));
	quad_init (&qfa, wa);
	quad_sub (&qfa, &qa, &qfa);

	/* (wb+qfb)*(wa+qfa) mod 1, dropping the integer wa*wb term. */
	quad_mul (&res, &qfa, &qfb);

	quad_mul12 (&qp, wa, b);
	{ Quad d = qp; Quad r0 = { round (d.h), 0.0 }; quad_sub (&qp, &d, &r0); }
	quad_add (&res, &res, &qp);

	quad_init (&qp, wb);
	quad_mul (&qp, &qp, &qfa);
	{ Quad d = qp; Quad r0 = { round (d.h), 0.0 }; quad_sub (&qp, &d, &r0); }
	quad_add (&res, &res, &qp);

	{ Quad d = res; Quad r0 = { round (d.h), 0.0 }; quad_sub (dst, &d, &r0); }
}
