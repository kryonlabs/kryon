#ifndef KRY_QUAD_H
#define KRY_QUAD_H

/*
 * kry_quad.h: double-double ("quad") arithmetic, compensated summation,
 * and dense QR linear algebra on top of it.
 *
 * The arithmetic follows Dekker/Knuth two-sum and two-product splitting
 * with a 2^27+1 constant, the same construction used by goffice's
 * go-quad.c.  The accumulator is Shewchuk's growing-partials algorithm.
 * The QR decomposition is a Householder reduction carried out entirely
 * in quad precision.
 *
 * Everything is plain C99 with no dependencies beyond libm.
 */

#define QUAD_MATRIX_MAX 16
#define QUAD_ACC_MAX 128

typedef struct Quad {
	double h; /* high part */
	double l; /* low part */
} Quad;

typedef struct QuadAcc {
	double partials[QUAD_ACC_MAX];
	int len;
} QuadAcc;

typedef struct QuadMatrix {
	int m;
	int n;
	Quad data[QUAD_MATRIX_MAX][QUAD_MATRIX_MAX];
} QuadMatrix;

typedef struct QuadQR {
	QuadMatrix V;
	QuadMatrix R;
	int qdet;
} QuadQR;

/* Quad basics. */
void   quad_init (Quad *r, double hv);
double quad_val (const Quad *a);
void   quad_neg (Quad *r, const Quad *a);
void   quad_zero (Quad *r);

/* Exact/near-exact arithmetic. */
void quad_add (Quad *r, const Quad *a, const Quad *b);
void quad_sub (Quad *r, const Quad *a, const Quad *b);
void quad_mul12 (Quad *r, double xv, double yv); /* exact product of two doubles */
void quad_mul (Quad *r, const Quad *a, const Quad *b);
void quad_div (Quad *r, const Quad *a, const Quad *b);
void quad_sqrt (Quad *r, const Quad *a);
void quad_sqrt1pm1 (Quad *r, const Quad *a); /* sqrt(1+a)-1 with a Newton step */
void quad_scalbn (Quad *r, const Quad *a, int e);
int  quad_compare (const Quad *a, const Quad *b);
void quad_floor (Quad *r, const Quad *a);
void quad_rescale_base (Quad *r, double *e);

/* Powers and exponentials. */
void quad_pow_int (Quad *r, double *exp2, const Quad *x, const Quad *y);
void quad_pow_frac (Quad *r, const Quad *x, const Quad *y); /* y in [0;1] */
void quad_pow (Quad *r, double *expb, const Quad *x, const Quad *y);
void quad_exp (Quad *r, double *expb, const Quad *a);

/* Shewchuk compensated accumulator. */
void   quad_acc_clear (QuadAcc *acc);
void   quad_acc_add (QuadAcc *acc, double x);
void   quad_acc_add_quad (QuadAcc *acc, const Quad *x);
double quad_acc_value (const QuadAcc *acc);

/* Householder QR of an m-times-n (m >= n) quad matrix. */
QuadQR    quad_qr_new (const QuadMatrix *A);
void      quad_qr_multiply_qt (QuadQR *qr, Quad *x);

/* Triangular solves and eigen range of the R factor. */
int  quad_matrix_back_solve (QuadMatrix *R, Quad *x, const Quad *b, int allow_degenerate);
int  quad_matrix_fwd_solve (QuadMatrix *R, Quad *x, const Quad *b, int allow_degenerate);
void quad_matrix_eigen_range (QuadMatrix *A, double *emin, double *emax);

/* Determinant/inverse/algebra on quad matrices. */
void quad_matrix_determinant (QuadMatrix *A, Quad *res);
int  quad_matrix_inverse (QuadMatrix *A, double threshold, QuadMatrix *Z);
void quad_matrix_multiply (QuadMatrix *C, const QuadMatrix *A, const QuadMatrix *B);
void quad_matrix_transpose (QuadMatrix *A, const QuadMatrix *B);

/* Logarithms, hypotenuse, and inverse trigonometry. */
void quad_log (Quad *res, const Quad *a);
void quad_hypot (Quad *res, const Quad *a, const Quad *b);
void quad_asin (Quad *res, const Quad *a);
void quad_acos (Quad *res, const Quad *a);
void quad_atan2 (Quad *res, const Quad *y, const Quad *x);
void quad_atan2pi (Quad *res, const Quad *y, const Quad *x);

/* Accurate sin/cos of a times pi. */
void quad_sinpi (Quad *res, const Quad *a);
void quad_cospi (Quad *res, const Quad *a);

/* Multiply a by b modulo 1 (both near-unit scaled); helper for
 * argument reduction in complex powers. */
void quad_mulmod1 (Quad *dst, const Quad *qa, double b);

/* The pi constants as double-double values. */
extern const Quad QUAD_2PI_EXPORT;
extern const Quad QUAD_PI_EXPORT;

#endif /* KRY_QUAD_H */
