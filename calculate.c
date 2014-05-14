// Copyright 2014 Edwin Steiner <edwin.steiner@gmx.net>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#define _GNU_SOURCE

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include <gsl/gsl_complex.h>
#include <gsl/gsl_complex_math.h>
#include <gsl/gsl_integration.h>
#include <gsl/gsl_sf_bessel.h>

#define MAX_NPOINTS  100


typedef enum { REAL, IMAG } ComplexPart;


typedef struct Params_s
{
    double m;
    double r;
    double Pr;
    double Pi;
    double peps;
    double t;
    double d;
    double sigma;
    int    smear;
} Params;


typedef gsl_complex (* ComplexFunction) (gsl_complex x, void *params);


typedef struct Contour_s
{
    unsigned npoints;
    gsl_complex points[MAX_NPOINTS];
    int skip[MAX_NPOINTS];
} Contour;


typedef struct PlotContext_s
{
    char *filename_data;
    char *filename_contour;
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

    gsl_complex env = gsl_complex_div(
            p,
            gsl_complex_sqrt(gsl_complex_add_real(gsl_complex_mul(p,p), m*m)));

    if (params->smear)
    {
        gsl_complex sp = gsl_complex_mul_real(p, params->sigma);
        env = gsl_complex_mul(env, gsl_complex_exp(gsl_complex_negative(gsl_complex_mul(sp, sp))));
    }

    return env;
}


gsl_complex f_integrand(gsl_complex p, const Params *params)
{
    double r = params->r;

    return gsl_complex_mul(
            f_envelope(p, params),
            gsl_complex_exp(gsl_complex_mul_imag(p, r)));
}


gsl_complex f_bessel(gsl_complex p, const Params *params)
{
    double m = params->m;

    return gsl_complex_rect(0, 2*m * gsl_sf_bessel_K1(m * GSL_REAL(p)));
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

    double epsabs = 1e-9;
    double epsrel = 1e-9;

    lsp.part = REAL;
    gsl_integration_qawo(&F, a, epsabs, epsrel, table_size, ws, table_cos, &result_real_cos, &abserr_real_cos);
    gsl_integration_qawo(&F, a, epsabs, epsrel, table_size, ws, table_sin, &result_real_sin, &abserr_real_sin);

    lsp.part = IMAG;
    gsl_integration_qawo(&F, a, epsabs, epsrel, table_size, ws, table_cos, &result_imag_cos, &abserr_imag_cos);
    gsl_integration_qawo(&F, a, epsabs, epsrel, table_size, ws, table_sin, &result_imag_sin, &abserr_imag_sin);

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
                      double d,
                      int skip_top,
                      Contour *contour)
{
    memset(contour, 0, sizeof(*contour));

    double Pr = params->Pr;
    double Pi = params->Pi;
    double peps = params->peps;

    if (Pi <= params->m - peps)
    {
        contour->npoints = 4;
        contour->points[0] = gsl_complex_rect(-Pr, d);
        contour->points[1] = gsl_complex_rect(-Pr, Pi);
        contour->points[2] = gsl_complex_rect(+Pr, Pi);
        contour->points[3] = gsl_complex_rect(+Pr, d);

        contour->skip[1] = skip_top;
    }
    else
    {
        int i = 0;

        if (d < 1.0)
            contour->points[i++] = gsl_complex_rect(-Pr, Pi*d);

        if (d < 2.0)
        {
            contour->skip[i] = skip_top;
            contour->points[i++] = gsl_complex_rect(-Pr+pos(d - 1.0)*(Pr - peps), Pi);
        }

        if (d < 3.0)
            contour->points[i++] = gsl_complex_rect(-peps, Pi-pos(d - 2.0)*(Pi - params->m + peps));

        contour->points[i++] = gsl_complex_rect(-peps, params->m - peps);
        contour->points[i++] = gsl_complex_rect(+peps, params->m - peps);

        if (d < 3.0)
            contour->points[i++] = gsl_complex_rect(+peps, Pi-pos(d - 2.0)*(Pi - params->m + peps));

        if (d < 2.0)
        {
            contour->skip[i-1] = skip_top;
            contour->points[i++] = gsl_complex_rect(+Pr-pos(d - 1.0)*(Pr - peps), Pi);
        }

        if (d < 1.0)
            contour->points[i++] = gsl_complex_rect(+Pr, Pi*d);

        contour->npoints = i;
    }
}


void define_contour_II(const Params *params,
                       double d,
                       Contour *contour)
{
    memset(contour, 0, sizeof(*contour));

    double Pr = params->Pr;
    double Pi = params->Pi;

    contour->npoints = 4;
    contour->points[0] = gsl_complex_rect(-Pr, d);
    contour->points[1] = gsl_complex_rect(-Pr, Pi);
    contour->points[2] = gsl_complex_rect(+Pr, Pi);
    contour->points[3] = gsl_complex_rect(+Pr, d);

    contour->skip[1] = 1;
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
    OPT_SELECT_INTEGRAL,
    OPT_SELECT_BESSEL
};

enum {
    OPT_CONTOUR_M,
    OPT_CONTOUR_II,
    OPT_CONTOUR_IUI
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
    params.t = 0.0;
    params.smear = 0;
    params.sigma = 0.0;

    const char* opt_prefix = "";
    int opt_select = OPT_SELECT_INTEGRAL;
    int opt_contour = OPT_CONTOUR_M;
    gsl_complex opt_z0 = { { 0.0 } };
    gsl_complex opt_z1 = { { 0.0 } };
    int opt_n = 10000;

    const char* const short_options = "";

    const struct option long_options[] = {
      { "help",         0, NULL, 'h' },
      { "envelope",     0, &opt_select, OPT_SELECT_ENVELOPE },
      { "integrand",    0, &opt_select, OPT_SELECT_INTEGRAND },
      { "bessel",       0, &opt_select, OPT_SELECT_BESSEL },
      { "contour-II",   0, &opt_contour, OPT_CONTOUR_II },
      { "contour-IUI",  0, &opt_contour, OPT_CONTOUR_IUI },
      { "d",            1, NULL, 'd' },
      { "n",            1, NULL, 'n' },
      { "m",            1, NULL, 'm' },
      { "prefix",       1, NULL, 'p' },
      { "r",            1, NULL, 'r' },
      { "t",            1, NULL, 't' },
      { "z0",           1, NULL, '0' },
      { "z1",           1, NULL, '1' },
      { "Pr",           1, NULL, 'P' },
      { "Pi",           1, NULL, 'I' },
      { "smear",        1, NULL, 's' },
      { NULL,           0, NULL, 0   } /* end */
    };

    int next_option;
    do {
        next_option = getopt_long(argc, argv, short_options,
                long_options, NULL);
        switch (next_option)
        {
            case 'h':
                print_usage(stdout, 0);

            case 'd':
                parse_double(optarg, &params.d);
                break;

            case 'm':
                parse_double(optarg, &params.m);
                break;

            case 'n':
                opt_n = atoi(optarg);
                if (opt_n < 1)
                    opt_n = 1;
                break;

            case 'p':
                opt_prefix = optarg;
                break;

            case 'r':
                parse_double(optarg, &params.r);
                break;

            case 's':
                parse_double(optarg, &params.sigma);
                params.smear = 1;
                break;

            case 't':
                parse_double(optarg, &params.t);
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

            case 'P':
                parse_double(optarg, &params.Pr);
                break;

            case 'I':
                parse_double(optarg, &params.Pi);
                break;

            case 0:  break; // flag handled
            case -1: break; // end of options

            default:
                abort();
        }
    }
    while (next_option != -1);

    PlotContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.filename_contour = alloc_sprintf("%sCONTOUR.dat", opt_prefix);
    ctx.filename_data = alloc_sprintf("%sFUNCTION.dat", opt_prefix);

    if (opt_select != OPT_SELECT_INTEGRAL)
    {
        FILE *os = fopen(ctx.filename_data, "w");
        ComplexFunction func;
        switch (opt_select)
        {
            case OPT_SELECT_ENVELOPE : func = (ComplexFunction) &f_envelope; break;
            case OPT_SELECT_INTEGRAND: func = (ComplexFunction) &f_integrand; break;
            case OPT_SELECT_BESSEL   : func = (ComplexFunction) &f_bessel; break;
            default: abort();
        }
        tabulate(os, func, &params, opt_z0, opt_z1, opt_n);
        fclose(os);

        Contour contour;
        define_contour_line_segment(opt_z0, opt_z1, &contour);

        os = fopen(ctx.filename_contour, "w");
        emit_contour_points(&params, &contour, os);
        fclose(os);
    }
    else
    {
        Contour contour;
        switch (opt_contour)
        {
            case OPT_CONTOUR_II:
                define_contour_II(&params, params.d, &contour);
                break;
            case OPT_CONTOUR_IUI:
                define_contour_M(&params, params.d, 1, &contour);
                break;
            case OPT_CONTOUR_M:
                define_contour_M(&params, params.d, 0, &contour);
                break;
            default: abort();
        }

        FILE *os = fopen(ctx.filename_data, "w");
        tabulate_integral(
            &params,
            &contour,
            GSL_REAL(opt_z0), GSL_REAL(opt_z1),
            opt_n, os);
        fclose(os);

        os = fopen(ctx.filename_contour, "w");
        emit_contour_points(&params, &contour, os);
        fclose(os);
    }

    return 0;
}
