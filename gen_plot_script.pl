#!/usr/bin/perl

# Copyright 2014 Edwin Steiner <edwin.steiner@gmx.net>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

use strict;
use warnings;
use Getopt::Long;
use Data::Dumper;

my $opt_m = 1;
my $opt_r = 2;
my $opt_t;
my $opt_n;
my $opt_d;
my $opt_Pr;
my $opt_Pi;
my $opt_terminal;
my $opt_filename_output;
my $opt_filename_contour = "";
my $opt_filename_data = "";
my $opt_envelope = 0;
my $opt_integrand = 0;
my $opt_z0 = '0,0';
my $opt_z1 = '0,0';
my $opt_yrange;
my $opt_contour_II;
my $opt_contour_IUI;
my $opt_smear = 0.0;
my @opt_pre_plot;
my @opt_append_plot;

sub parse_complex
{
    my ($var) = @_;
    if ($$var =~ /(.*),(.*)/)
    {
        $$var = [$1, $2];
    }
    else
    {
        $$var = [$$var, 0.0];
    }
    1;
}

my $result = GetOptions(
        "m=s" => \$opt_m,
        "r=s" => \$opt_r,
        "n=s" => \$opt_n,
        "t=s" => \$opt_t,
        "d=s" => \$opt_d,
        "Pr=s" => \$opt_Pr,
        "Pi=s" => \$opt_Pi,
        "terminal=s" => \$opt_terminal,
        "envelope" => \$opt_envelope,
        "integrand" => \$opt_integrand,
        "z0=s" => \$opt_z0,
        "z1=s" => \$opt_z1,
        "output-file=s" => \$opt_filename_output,
        "data-file=s" => \$opt_filename_data,
        "contour-file=s" => \$opt_filename_contour,
        "pre-plot=s" => \@opt_pre_plot,
        "append-plot=s" => \@opt_append_plot,
        "yrange=s" => \$opt_yrange,
        "contour-II" => \$opt_contour_II,
        "contour-IUI" => \$opt_contour_IUI,
        "smear=s" => \$opt_smear,
);
parse_complex(\$opt_z0);
parse_complex(\$opt_z1);

if (!$result)
{
    print STDERR "Usage: ./gen_plot_script.pl [options]\n";
    exit(1);
}

sub emit_plot_contour_inset
{
    my ($file, $ox, $oy, $sx, $sy, $xmin, $xmax, $ymin, $ymax, $plot_m, $linestyle) = @_;

    printf $file
     "set origin %g, %g\n"
    ."set size %g, %g\n"
    ."set xrange [%g:%g]\n"
    ."set yrange [%g:%g]\n"
    ."set grid x2tics\n"
    ."set grid y2tics\n"
    ."set x2tics (0) format \"\" scale 0\n"
    ."set y2tics (0) format \"\" scale 0\n",
    $ox, $oy, $sx, $sy,
    $xmin, $xmax, $ymin, $ymax;

    if (($xmax - $xmin) >= 100)
    {
        print $file "set xtics 40\n";
    }
    elsif (($xmax - $xmin) >= 10)
    {
        print $file "set xtics 5\n";
    }
    else
    {
        print $file "set xtics 1\n";
    }

    printf $file "\nplot ";

    if ($plot_m)
    {
        printf $file "\"<echo '0 %g'\" with points notitle, \\\n     ", $opt_m;
    }

    $linestyle = q{lw 2 lt 1 lc rgb 'blue'} if !defined($linestyle);

    printf $file
    "\"%s\" using 1:2 with lines $linestyle notitle\n\n", $opt_filename_contour;
}


sub emit_plot_commands_function
{
    my ($file) = @_;

    my $yrange = $opt_yrange;
    $yrange = '-3:3' if !defined($yrange);

    print $file 
     "set size 1,1\n"
    ."set multiplot\n"
    ."\n"
    ."set origin 0,0\n"
    ."set size 1,1\n"
    ."set lmargin 6\n"
    ."set rmargin 6\n"
    ."\n"
    ."set xrange [0:1]\n"
    ."set yrange [$yrange]\n"
    ."set xlabel \"p\"\n"
    ."set style fill solid 0.4\n"
    ."\n";

    print $file "set xtics (";
    for my $i (0..4)
    {
        my $t = $i / 4.0;
        my $z_re = $opt_z0->[0] + ($opt_z1->[0] - $opt_z0->[0]) * $t;
        my $z_im = $opt_z0->[1] + ($opt_z1->[1] - $opt_z0->[1]) * $t;
        print $file ", " if $i > 0;
        printf $file "\"(%4.2g + %4.2g i)\" %g", $z_re, $z_im, $t;
    }
    print $file ")\n";

    print $file "set object 1 rectangle from graph 0.69,0.025 to graph 0.98,0.4 front fc rgb \"white\"\n";

    print $file "$_\n" for (@opt_pre_plot);

    print $file "\nplot ";

    printf $file "\"%s\" using 1:7:6 with filledcurve lt 1 lc rgb \"orange\" title \"± abs\", \\\n     "
               ."\"%s\" using 1:4 with lines lt 1 lc 1 title \"real\", \\\n     "
               ."\"%s\" using 1:5 with lines lt 1 lc 3 title \"imag\"",
            $opt_filename_data, $opt_filename_data, $opt_filename_data;

    print $file "\n\n";
    print $file "unset arrow\n";
    print $file "unset label\n";
    print $file "unset xlabel\n";
    print $file "unset object 1\n";

    print $file "set title \"complex plane\"\n";
    emit_plot_contour_inset($file, 0.66, 0.1, 0.3, 0.35, -10.5,+10.5, -1, 4, 1, q{lc rgb "black" lt 2 lw 3});

    print $file "\nunset multiplot\n";
}


sub emit_plot_commands
{
    my ($file) = @_;

    my $yrange = $opt_yrange;
    $yrange = '-15:15' if !defined($yrange);

    print $file 
     "set size 1,1\n"
    ."set multiplot\n"
    ."\n"
    ."set origin 0,0\n"
    ."set lmargin 6\n"
    ."set size 1,1\n"
    ."\n"
    ."set xrange [0:10]\n"
    ."set yrange [$yrange]\n"
    ."set style fill solid 0.4\n"
    ."set xlabel \"r\"\n"
    ."\n";

    print $file "$_\n" for (@opt_pre_plot);

    printf $file "plot ".join(", \\\n     ",
            "\"$opt_filename_data\" using 1:(0):4 with filledcurve lc rgb \"orange\" lt 1 title \"abs\"",
            "\"$opt_filename_data\" using 1:2 with lines lt 1 lc 1 title \"real\"",
            "\"$opt_filename_data\" using 1:3 with lines lt 1 lc 3 title \"imag\"",
            @opt_append_plot);

    print $file "\n\n";
    print $file "unset arrow\n";
    print $file "unset label\n";
    print $file "unset xlabel\n";
    print $file "\n\n";

    print $file "set title \"contour\"\n";
    emit_plot_contour_inset($file, 0.2, 0.1, 0.3, 0.35, -90,+90, -10,+90, 0);
    print $file "set title \"contour (zoom)\"\n";
    emit_plot_contour_inset($file, 0.45, 0.1, 0.3, 0.35, -90,+90, -1, $opt_m+4, 0);
    print $file "set title \"contour (zoom)\"\n";
    emit_plot_contour_inset($file, 0.7, 0.1, 0.3, 0.35, -4.5,+4.5, -1, $opt_m+3, 1);

    print $file "\nunset multiplot\n";
}


if (defined($opt_terminal))
{
    print "set terminal $opt_terminal\n";
}

if (defined($opt_filename_output))
{
    printf "set output '%s'\n", $opt_filename_output;
}

if ($opt_envelope || $opt_integrand)
{
    emit_plot_commands_function(\*STDOUT);
}
else
{
    emit_plot_commands(\*STDOUT);
}
