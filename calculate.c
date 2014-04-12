#include <stdio.h>
#include <gsl/gsl_complex.h>
#include <gsl/gsl_complex_math.h>
#include <gsl/gsl_integration.h>

typedef enum { REAL, IMAG } ComplexPart;

typedef struct Params_s
{
    double m;
    double r;
} Params;

typedef gsl_complex (* ComplexFunction) (gsl_complex x, void *params);

typedef struct AffineWrapperParams_s
{
    ComplexFunction fun;
    gsl_complex d;
    gsl_complex k;
    void *params;
} AffineWrapperParams;


gsl_complex f_envelope(gsl_complex p, const Params *params)
{
    double m = params->m;

    return gsl_complex_div(
            p,
            gsl_complex_sqrt(gsl_complex_add_real(gsl_complex_mul(p,p), m*m)));
}


gsl_complex f_integrand(gsl_complex p, const Params *params)
{
    double r = params->r;

    return gsl_complex_mul(
            f_envelope(p, params),
            gsl_complex_exp(gsl_complex_mul_imag(p, r)));
}

gsl_complex affine_wrapper(double x, void *data)
{
    const AffineWrapperParams *params = (const AffineWrapperParams *)data;

    gsl_complex z = gsl_complex_add(params->d,
            gsl_complex_mul_real(params->k, x));

    return params->fun(z, params->params);
}


void tabulate(ComplexFunction fun,
              void *params,
              gsl_complex z0,
              gsl_complex z1,
              int n)
{
    gsl_complex k = gsl_complex_sub(z1, z0);
    for (int i = 0; i < n; ++i)
    {
        double t = (double)i / (n - 1);
        gsl_complex z = gsl_complex_add(z0, gsl_complex_mul_real(k, t));
        gsl_complex f = fun(z, params);
        double abs = gsl_complex_abs(f);

        printf("%g %g %g %g %g %g %g\n",
               t,
               GSL_REAL(z), GSL_IMAG(z),
               GSL_REAL(f), GSL_IMAG(f), abs, -abs);
    }
}


typedef struct LineSegmentParams_s {
    gsl_complex p0;
    gsl_complex k;
    double damping;
    Params *params;
    ComplexPart part;
} LineSegmentParams;


double line_segment_integrand_wrapper(double t, void *data)
{
    const LineSegmentParams *params = (const LineSegmentParams *)data;

    gsl_complex p = gsl_complex_add(params->p0,
            gsl_complex_mul_real(params->k, t));

    gsl_complex v = f_envelope(p, params->params);

    double v_part = (params->part == REAL) ? GSL_REAL(v) : GSL_IMAG(v);
    double damped = exp( - params->damping * t) * v_part;

    return damped;
}


gsl_complex integrate_line_segment(Params *params,
                                   gsl_complex p0,
                                   gsl_complex p1)
{
    gsl_complex k = gsl_complex_sub(p1, p0);

    const double r = params->r;
    const double a = 0.0; // parameter interval start
    const double b = 1.0; // parameter interval end
    const double L = b - a; // length of parameter interval
    const size_t table_size = 1000;

    // calculate frequency of oscillatory part
    double omega = GSL_REAL(k) * r;

    gsl_integration_workspace *ws = gsl_integration_workspace_alloc(table_size);

    // prepare sine/cosine tables for integration
    gsl_integration_qawo_table *table_cos = gsl_integration_qawo_table_alloc(omega, L, GSL_INTEG_COSINE, table_size);
    gsl_integration_qawo_table *table_sin = gsl_integration_qawo_table_alloc(omega, L, GSL_INTEG_SINE, table_size);

    LineSegmentParams lsp;
    lsp.p0 = p0;
    lsp.k = k;
    lsp.damping = params->r * GSL_IMAG(k);
    lsp.params = params;

    fprintf(stderr, "p0 = %g %g, p1 = %g %g, k = %g %g, r = %g, omega = %g, damping = %g\n",
            GSL_REAL(p0), GSL_IMAG(p0), GSL_REAL(p1), GSL_IMAG(p1),
            GSL_REAL(k), GSL_IMAG(k), r, omega, lsp.damping);

    gsl_function F;
    F.function = &line_segment_integrand_wrapper;
    F.params = &lsp;

    double result_real_cos, abserr_real_cos;
    double result_real_sin, abserr_real_sin;
    double result_imag_cos, abserr_imag_cos;
    double result_imag_sin, abserr_imag_sin;

    double epsrel = 1e-6;

    lsp.part = REAL;
    gsl_integration_qawo(&F, a, 0, epsrel, table_size, ws, table_cos, &result_real_cos, &abserr_real_cos);
    gsl_integration_qawo(&F, a, 0, epsrel, table_size, ws, table_sin, &result_real_sin, &abserr_real_sin);

    lsp.part = IMAG;
    gsl_integration_qawo(&F, a, 0, epsrel, table_size, ws, table_cos, &result_imag_cos, &abserr_imag_cos);
    gsl_integration_qawo(&F, a, 0, epsrel, table_size, ws, table_sin, &result_imag_sin, &abserr_imag_sin);

    fprintf(stderr, "    cos: %g (+- %g) %g (+- %g)  sin: %g (+- %g) %g (+- %g)\n",
            result_real_cos, abserr_real_cos, result_imag_cos, abserr_imag_cos,
            result_real_sin, abserr_real_sin, result_imag_sin, abserr_imag_sin);

    gsl_complex cos_part = gsl_complex_rect(result_real_cos, result_imag_cos);
    gsl_complex sin_part = gsl_complex_rect(-result_imag_sin, result_real_sin);
    gsl_complex sum = gsl_complex_add(cos_part, sin_part);
    gsl_complex result = gsl_complex_mul(
            k,
            gsl_complex_mul(sum, gsl_complex_exp(gsl_complex_mul_imag(p0, params->r))));

    gsl_integration_qawo_table_free(table_sin);
    gsl_integration_qawo_table_free(table_cos);

    gsl_integration_workspace_free(ws);

    return result;
}


void tabulate_integral(Params *params,
                       gsl_complex p0,
                       gsl_complex p1,
                       double r0,
                       double r1,
                       int n)
{
    for (int i = 0; i < n; ++i)
    {
        double r = r0 + (r1 - r0) * ((double)i / (n - 1));

        params->r = r;
        gsl_complex res = integrate_line_segment(params, p0, p1);

        double abs = gsl_complex_abs(res);

        printf("%g %g %g %g %g\n",
               r, GSL_REAL(res), GSL_IMAG(res), abs, -abs);
    }
}


int main(void)
{
    Params params;
    params.m = 1;
    params.r = 2;

    // AffineWrapperParams wp;
    // wp.fun = (ComplexFunction) &f_envelope;
    // wp.d = gsl_complex_rect(0,-1);
    // wp.k = gsl_complex_rect(2,1);
    // wp.params = &params;

    //tabulate((ComplexFunction) &f_integrand, &params,
             //gsl_complex_rect(0,-10), gsl_complex_rect(0,10),
             //gsl_complex_rect(-10,0.9), gsl_complex_rect(10,0.9),
             //gsl_complex_rect(-10,-10), gsl_complex_rect(10,10),
             //gsl_complex_rect(-0.5,-10), gsl_complex_rect(0.5,10),
             //gsl_complex_rect(-10,1), gsl_complex_rect(10,1),
             //gsl_complex_rect(-10,0), gsl_complex_rect(10,0),
             //10000);

    tabulate_integral(
        &params,
        gsl_complex_rect(-100,0.0), gsl_complex_rect(100,0.0),
        0.1, 10,
        1000);

    return 0;
}
