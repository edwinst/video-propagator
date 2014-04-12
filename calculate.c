#include <stdio.h>
#include <gsl/gsl_complex.h>
#include <gsl/gsl_complex_math.h>

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


int main(void)
{
    Params params;
    params.m = 1;
    params.r = 2;

    AffineWrapperParams wp;
    wp.fun = (ComplexFunction) &f_envelope;
    wp.d = gsl_complex_rect(0,-1);
    wp.k = gsl_complex_rect(2,1);
    wp.params = &params;

    tabulate((ComplexFunction) &f_integrand, &params,
             //gsl_complex_rect(0,-10), gsl_complex_rect(0,10),
             //gsl_complex_rect(-10,0.9), gsl_complex_rect(10,0.9),
             //gsl_complex_rect(-10,-10), gsl_complex_rect(10,10),
             //gsl_complex_rect(-0.5,-10), gsl_complex_rect(0.5,10),
             gsl_complex_rect(-10,1), gsl_complex_rect(10,1),
             //gsl_complex_rect(-10,0), gsl_complex_rect(10,0),
             10000);

    return 0;
}
