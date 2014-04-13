#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <gsl/gsl_complex.h>
#include <gsl/gsl_complex_math.h>
#include <gsl/gsl_integration.h>

#define MAX_NPOINTS  100


typedef enum { REAL, IMAG } ComplexPart;


typedef struct Params_s
{
    double m;
    double r;
    double Pr;
    double Pi;
    double peps;
} Params;


typedef gsl_complex (* ComplexFunction) (gsl_complex x, void *params);


typedef struct Contour_s
{
    unsigned npoints;
    gsl_complex points[MAX_NPOINTS];
    int skip[MAX_NPOINTS];
} Contour;


typedef struct AffineWrapperParams_s
{
    ComplexFunction fun;
    gsl_complex d;
    gsl_complex k;
    void *params;
} AffineWrapperParams;


typedef struct PlotContext_s
{
    char *filename_data;
    char *filename_baseline_data;
    char *filename_contour;
    char *filename_output;
} PlotContext;


char *alloc_sprintf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int len = vsnprintf(NULL, 0, fmt, ap);
    char *str = malloc((len + 1) * sizeof(char));
    if (!str)
        exit(1);
    va_start(ap, fmt);
    vsnprintf(str, len + 1, fmt, ap);
    va_end(ap);
    return str;
}


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

    //fprintf(stderr, "p0 = %g %g, p1 = %g %g, k = %g %g, r = %g, omega = %g, damping = %g\n",
    //        GSL_REAL(p0), GSL_IMAG(p0), GSL_REAL(p1), GSL_IMAG(p1),
    //        GSL_REAL(k), GSL_IMAG(k), r, omega, lsp.damping);

    gsl_function F;
    F.function = &line_segment_integrand_wrapper;
    F.params = &lsp;

    double result_real_cos, abserr_real_cos;
    double result_real_sin, abserr_real_sin;
    double result_imag_cos, abserr_imag_cos;
    double result_imag_sin, abserr_imag_sin;

    double epsrel = 1e-3;

    lsp.part = REAL;
    gsl_integration_qawo(&F, a, 0, epsrel, table_size, ws, table_cos, &result_real_cos, &abserr_real_cos);
    gsl_integration_qawo(&F, a, 0, epsrel, table_size, ws, table_sin, &result_real_sin, &abserr_real_sin);

    lsp.part = IMAG;
    gsl_integration_qawo(&F, a, 0, epsrel, table_size, ws, table_cos, &result_imag_cos, &abserr_imag_cos);
    gsl_integration_qawo(&F, a, 0, epsrel, table_size, ws, table_sin, &result_imag_sin, &abserr_imag_sin);

    //fprintf(stderr, "    cos: %g (+- %g) %g (+- %g)  sin: %g (+- %g) %g (+- %g)\n",
    //        result_real_cos, abserr_real_cos, result_imag_cos, abserr_imag_cos,
    //        result_real_sin, abserr_real_sin, result_imag_sin, abserr_imag_sin);

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


gsl_complex integrate_contour(Params *params,
                              const Contour *contour)
{
    gsl_complex result = gsl_complex_rect(0.0, 0.0);

    if (contour->npoints < 2)
        return result;

    for (unsigned i = 0; i < contour->npoints - 1; ++i)
    {
        if (contour->skip[i])
            continue;

        result = gsl_complex_add(result,
                integrate_line_segment(params, contour->points[i], contour->points[i+1]));
    }

    return result;
}


void tabulate_integral(Params *params,
                       const Contour *contour,
                       double r0,
                       double r1,
                       int n,
                       FILE *os)
{
    for (int i = 0; i < n; ++i)
    {
        double r = r0 + (r1 - r0) * ((double)i / (n - 1));

        params->r = r;
        gsl_complex res = integrate_contour(params, contour);

        double abs = gsl_complex_abs(res);

        fprintf(os, "%g %g %g %g %g\n",
                r, GSL_REAL(res), GSL_IMAG(res), abs, -abs);
    }
}


void emit_contour_points(const Params *params,
                         const Contour *contour,
                         FILE *os)
{
    int skipping = 1;

    for (unsigned i = 0; i < contour->npoints; ++i)
    {
        if (!skipping || !contour->skip[i])
        {
            fprintf(os, "%g %g\n", GSL_REAL(contour->points[i]), GSL_IMAG(contour->points[i]));

            if (contour->skip[i])
            {
                fprintf(os, "\n");
            }
        }
        skipping = contour->skip[i];
    }
}


void emit_plot_contour_inset(const Params *params,
                             const PlotContext *ctx,
                             double ox, double oy, double sx, double sy,
                             double xmin, double xmax, double ymin, double ymax,
                             FILE *os)
{
    fprintf(os,
    "set origin %g, %g\n"
    "set size %g, %g\n"
    "set xrange [%g:%g]\n"
    "set yrange [%g:%g]\n"
    "set grid x2tics\n"
    "set grid y2tics\n"
    "set x2tics (0) format \"\" scale 0\n"
    "set y2tics (0) format \"\" scale 0\n"
    "\n"
    "plot \"<echo '0 %g'\" with points notitle, \\\n"
    "     \"%s\" using 1:2 with lines lc rgb 'blue' notitle\n"
    "\n",
    ox, oy, sx, sy,
    xmin, xmax, ymin, ymax,
    params->m,
    ctx->filename_contour);
}


void emit_plot_commands(const Params *params,
                        const PlotContext *ctx,
                        FILE *os)
{
    fprintf(os,
    "set size 1,1\n"
    "set multiplot\n"
    "\n"
    "set origin 0,0\n"
    "set size 1,1\n"
    "\n"
    "set xrange [0:10]\n"
    "set yrange [-15:15]\n"
    "set style fill solid 0.2\n"
    "\n"
    "plot ");

    if (ctx->filename_baseline_data != 0)
    {
        fprintf(os,"\"%s\" using 1:5:4 with filledcurve lc rgb 'gray40' title \"baseline\", \\\n     ",
                ctx->filename_baseline_data);
    }

    fprintf(os,"\"%s\" using 1:5:4 with filledcurve lc 4 title \"abs\", \\\n     "
               "\"%s\" using 1:2 with lines lc 1 title \"real\", \\\n     "
               "\"%s\" using 1:3 with lines lc 3 title \"imag\"",
            ctx->filename_data, ctx->filename_data, ctx->filename_data);

    fprintf(os, "\n\n");

    emit_plot_contour_inset(params, ctx, 0.6, 0.1, 0.4, 0.4, -5,+5, -1, params->m+4, os);
    emit_plot_contour_inset(params, ctx, 0.4, 0.55, 0.3, 0.3, -100,+100, -1, params->m+4, os);
    emit_plot_contour_inset(params, ctx, 0.7, 0.55, 0.3, 0.3, -100,+100, -10,+100, os);

    fprintf(os, "\nunset multiplot\n");
}


void define_contour_M(const Params *params,
                      double t,
                      double d,
                      Contour *contour)
{
    memset(contour, 0, sizeof(*contour));

    double Pr = params->Pr;
    double peps = params->peps;

    if (t <= params->m - peps)
    {
        contour->npoints = 4;
        contour->points[0] = gsl_complex_rect(-Pr, d);
        contour->points[1] = gsl_complex_rect(-Pr, t);
        contour->points[2] = gsl_complex_rect(+Pr, t);
        contour->points[3] = gsl_complex_rect(+Pr, d);
    }
    else
    {
        contour->npoints = 8;
        contour->points[0] = gsl_complex_rect(-Pr, d);
        contour->points[1] = gsl_complex_rect(-Pr, t);
        contour->points[2] = gsl_complex_rect(-peps, t);
        contour->points[3] = gsl_complex_rect(-peps, params->m - peps);
        contour->points[4] = gsl_complex_rect(+peps, params->m - peps);
        contour->points[5] = gsl_complex_rect(+peps, t);
        contour->points[6] = gsl_complex_rect(+Pr, t);
        contour->points[7] = gsl_complex_rect(+Pr, d);
    }
}


void define_contour_V(const Params *params,
                      double t,
                      Contour *contour)
{
    memset(contour, 0, sizeof(*contour));

    double Pr = params->Pr;
    double Pi = params->Pi;
    double peps = params->peps;

    contour->npoints = 4;
    if (t < 0.5)
    {
        contour->points[0] = gsl_complex_rect(-Pr, Pi*2*t);
        contour->points[1] = gsl_complex_rect(-peps, params->m - peps);
        contour->points[2] = gsl_complex_rect(+peps, params->m - peps);
        contour->points[3] = gsl_complex_rect(+Pr, Pi*2*t);
    }
    else
    {
        contour->points[0] = gsl_complex_rect(-Pr+(2*t - 1.0)*(Pr-peps), Pi);
        contour->points[1] = gsl_complex_rect(-peps, params->m - peps);
        contour->points[2] = gsl_complex_rect(+peps, params->m - peps);
        contour->points[3] = gsl_complex_rect(+Pr-(2*t - 1.0)*(Pr-peps), Pi);
    }
}


int main(void)
{
    Params params;
    params.m = 1;
    params.r = 2;
    params.Pr = 60;
    params.Pi = 60;
    params.peps = 0.2;

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

    PlotContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.filename_contour = "CONTOUR";

    double shorten = 1.0;

    for (int i = 0; i < 11; ++i)
    {
        ctx.filename_data = alloc_sprintf("DATA-%04d.dat", i);
        ctx.filename_output = alloc_sprintf("PLOT-%04d.png", i);

        double d = 0.1 * (pow(1.5, i) - 1.0);
        if (d > params.Pi)
            d = params.Pi;
        printf("d = %g\n", d);

        //params.Pr = 60 + (i-5) * 0.5;
        Contour contour;
        define_contour_M(&params, params.Pi, d, &contour);

        FILE *os = fopen(ctx.filename_data, "w");
        tabulate_integral(
            &params,
            &contour,
            0.1, 10 / shorten,
            500 / floor(shorten), os);
        fclose(os);

        os = fopen(ctx.filename_contour, "w");
        emit_contour_points(&params, &contour, os);
        fclose(os);

        os = fopen("PLOT", "w");
        fprintf(os, "set terminal pngcairo size 1000,600\n");
        fprintf(os, "set output '%s'\n", ctx.filename_output);
        emit_plot_commands(&params, &ctx, os);
        fclose(os);

        char *cmd = alloc_sprintf("gnuplot 'PLOT'");
        system(cmd);
        free(cmd);

        if (!ctx.filename_baseline_data)
            ctx.filename_baseline_data = strdup(ctx.filename_data);

        free(ctx.filename_output); ctx.filename_output = NULL;
        free(ctx.filename_data); ctx.filename_data = NULL;
    }

    return 0;
}
