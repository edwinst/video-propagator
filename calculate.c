#define _GNU_SOURCE

#include <getopt.h>
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


void tabulate(FILE *os,
              ComplexFunction fun,
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

        fprintf(os, "%g %g %g %g %g %g %g\n",
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
                             int plot_m,
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
    "set y2tics (0) format \"\" scale 0\n",
    ox, oy, sx, sy,
    xmin, xmax, ymin, ymax);

    if ((xmax - xmin) >= 100)
        fprintf(os, "set xtics 40\n");
    else
        fprintf(os, "set xtics 1\n");

    fprintf(os, "\nplot ");

    if (plot_m)
        fprintf(os, "\"<echo '0 %g'\" with points notitle, \\\n     ",
                params->m);

    fprintf(os,
    "\"%s\" using 1:2 with lines lc rgb 'blue' notitle\n"
    "\n",
    ctx->filename_contour);
}


void emit_plot_commands_function(const Params *params,
                                 const PlotContext *ctx,
                                 const Contour *contour,
                                 FILE *os)
{
    fprintf(os,
    "set size 1,1\n"
    "set multiplot\n"
    "\n"
    "set origin 0,0\n"
    "set size 1,1\n"
    "\n"
    "set xrange [0:1]\n"
    "set yrange [-1:1]\n"
    "set style fill solid 0.4\n"
    "\n");

    fprintf(os, "set xtics (");
    for (int i = 0; i < 5 ; ++i)
    {
        double t = i / 4.0;
        gsl_complex z = gsl_complex_add(contour->points[0],
                gsl_complex_mul_real(gsl_complex_sub(contour->points[1], contour->points[0]), t));
        if (i)
           fprintf(os, ", ");
        fprintf(os, "\"(%g + %g i)\" %g", GSL_REAL(z), GSL_IMAG(z), t);
    }
    fprintf(os, ")\n");

    fprintf(os, "set object 1 rectangle from graph 0.62,0.05 to graph 0.92,0.4 front fc rgb \"white\"\n");
    fprintf(os, "\nplot ");

    fprintf(os,"\"%s\" using 1:7:6 with filledcurve lc 4 title \"abs\", \\\n     "
               "\"%s\" using 1:4 with lines lc 1 title \"real\", \\\n     "
               "\"%s\" using 1:5 with lines lc 3 title \"imag\"",
            ctx->filename_data, ctx->filename_data, ctx->filename_data);

    fprintf(os, "\n\n");
    fprintf(os, "unset object 1\n");

    emit_plot_contour_inset(params, ctx, 0.6, 0.1, 0.3, 0.3, -4.5,+4.5, -1, params->m+3, 1, os);

    fprintf(os, "\nunset multiplot\n");
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
    "set style fill solid 0.4\n"
    "\n"
    "plot ");

    if (ctx->filename_baseline_data != 0)
    {
        fprintf(os,"\"%s\" using 1:4 with lines linestyle 0 lc rgb 'gray40' notitle, \\\n     ",
                ctx->filename_baseline_data);
    }

    fprintf(os,"\"%s\" using 1:5:4 with filledcurve lc 4 title \"abs\", \\\n     "
               "\"%s\" using 1:2 with lines lc 1 title \"real\", \\\n     "
               "\"%s\" using 1:3 with lines lc 3 title \"imag\"",
            ctx->filename_data, ctx->filename_data, ctx->filename_data);

    fprintf(os, "\n\n");

    emit_plot_contour_inset(params, ctx, 0.2, 0.1, 0.3, 0.3, -90,+90, -10,+90, 0, os);
    emit_plot_contour_inset(params, ctx, 0.45, 0.1, 0.3, 0.3, -90,+90, -1, params->m+4, 0, os);
    emit_plot_contour_inset(params, ctx, 0.7, 0.1, 0.3, 0.3, -4.5,+4.5, -1, params->m+3, 1, os);

    fprintf(os, "\nunset multiplot\n");
}


void define_contour_line_segment(gsl_complex z0,
                                 gsl_complex z1,
                                 Contour *contour)
{
    memset(contour, 0, sizeof(*contour));

    contour->npoints = 2;
    contour->points[0] = z0;
    contour->points[1] = z1;
}


double pos(double x)
{
    return (x > 0) ? x : 0;
}


void define_contour_M(const Params *params,
                      double t,
                      double d,
                      Contour *contour)
{
    memset(contour, 0, sizeof(*contour));

    double Pr = params->Pr;
    double Pi = params->Pi;
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
        int i = 0;

        if (d < 1.0)
            contour->points[i++] = gsl_complex_rect(-Pr, Pi*d);

        if (d < 2.0)
            contour->points[i++] = gsl_complex_rect(-Pr+pos(d - 1.0)*(Pr - peps), Pi);

        if (d < 3.0)
            contour->points[i++] = gsl_complex_rect(-peps, Pi-pos(d - 2.0)*(Pi - params->m + peps));

        contour->points[i++] = gsl_complex_rect(-peps, params->m - peps);
        contour->points[i++] = gsl_complex_rect(+peps, params->m - peps);

        if (d < 3.0)
            contour->points[i++] = gsl_complex_rect(+peps, Pi-pos(d - 2.0)*(Pi - params->m + peps));

        if (d < 2.0)
            contour->points[i++] = gsl_complex_rect(+Pr-pos(d - 1.0)*(Pr - peps), Pi);

        if (d < 1.0)
            contour->points[i++] = gsl_complex_rect(+Pr, Pi*d);

        contour->npoints = i;
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


void print_usage(FILE *os, int exitcode)
{
    fprintf(os, "Usage: ./calculate [--prefix=PREFIX]\n\n");

    exit(exitcode);
}


enum {
    OPT_SELECT_ENVELOPE,
    OPT_SELECT_INTEGRAND,
    OPT_SELECT_INTEGRAL
};


void parse_double(const char *str, double *dest)
{
    char *end;
    *dest = strtod(str, &end);
}


void parse_complex(const char *str, gsl_complex *dest)
{
    const char *comma = strchr(str, ',');
    if (comma != NULL)
    {
        char *end;
        GSL_SET_REAL(dest, strtod(str, &end));
        GSL_SET_IMAG(dest, strtod(comma + 1, &end));
    }
    else
    {
        char *end;
        GSL_SET_REAL(dest, strtod(str, &end));
        GSL_SET_IMAG(dest, 0.0);
    }
}


int main(int argc, char **argv)
{
    Params params;
    params.m = 1;
    params.r = 2;
    params.Pr = 60;
    params.Pi = 60;
    params.peps = 0.2;

    const char* opt_prefix = "";
    int opt_select = OPT_SELECT_INTEGRAL;
    gsl_complex opt_z0 = { { 0.0 } };
    gsl_complex opt_z1 = { { 0.0 } };
    int opt_n = 10000;

    const char* const short_options = "";

    const struct option long_options[] = {
      { "help",      0, NULL, 'h' },
      { "envelope",  0, &opt_select, OPT_SELECT_ENVELOPE },
      { "integrand", 0, &opt_select, OPT_SELECT_INTEGRAND },
      { "m",         1, NULL, 'm' },
      { "prefix",    1, NULL, 'p' },
      { "r",         1, NULL, 'r' },
      { "z0",        1, NULL, '0' },
      { "z1",        1, NULL, '1' },
      { NULL,        0, NULL, 0   } /* end */
    };

    int next_option;
    do {
        next_option = getopt_long(argc, argv, short_options,
                long_options, NULL);
        switch (next_option)
        {
            case 'h':
                print_usage(stdout, 0);

            case 'm':
                parse_double(optarg, &params.m);
                break;

            case 'p':
                opt_prefix = optarg;
                break;

            case 'r':
                parse_double(optarg, &params.r);
                break;

            case '?':
                // invalid option
                print_usage(stderr, 1);

            case '0':
                parse_complex(optarg, &opt_z0);
                break;

            case '1':
                parse_complex(optarg, &opt_z1);
                break;

            case 0:  break; // flag handled
            case -1: break; // end of options

            default:
                abort ();
        }
    }
    while (next_option != -1);

    PlotContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.filename_contour = alloc_sprintf("%sCONTOUR.dat", opt_prefix);

    if (opt_select != OPT_SELECT_INTEGRAL)
    {
        ctx.filename_data = alloc_sprintf("%sFUNCTION.dat", opt_prefix);
        ctx.filename_output = alloc_sprintf("%splot.png", opt_prefix);
        const char *script = alloc_sprintf("%sPLOT.gnuplot", opt_prefix);

        FILE *os = fopen(ctx.filename_data, "w");
        tabulate(os, (ComplexFunction)
                ((opt_select == OPT_SELECT_INTEGRAND) ? &f_integrand : &f_envelope),
                &params, opt_z0, opt_z1, opt_n);
        fclose(os);

        Contour contour;
        define_contour_line_segment(opt_z0, opt_z1, &contour);

        os = fopen(ctx.filename_contour, "w");
        emit_contour_points(&params, &contour, os);
        fclose(os);

        os = fopen(script, "w");
        fprintf(os, "set terminal pngcairo size 1000,600\n");
        fprintf(os, "set output '%s'\n", ctx.filename_output);
        emit_plot_commands_function(&params, &ctx, &contour, os);
        fclose(os);
    }
    else
    {
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
    }

    return 0;
}
