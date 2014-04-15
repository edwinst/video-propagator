#!/usr/bin/perl

for my $i (0..140)
{
    my $pre = sprintf("test-%04d-", $i);
    my $m = 0.1 * ($i + 1);

    #system("./calculate --prefix $pre --integrand --z0 -4 --z1 4 --r 10 --m $m && gnuplot ${pre}PLOT.gnuplot");

    my $rho = $i * 0.05 / 4;
    #system("./calculate --n 500 --prefix $pre --envelope --z0 -4,$rho --z1 4,$rho --r 20 --m 2 && gnuplot ${pre}PLOT.gnuplot");
    system("./calculate --n 500 --prefix $pre --integrand --z0 -4,$rho --z1 4,$rho --r 2 --m 1 && gnuplot ${pre}PLOT.gnuplot");
    #my $rho = -1 + 0.05 / 4 * $i;
    #system("./calculate --n 500 --prefix $pre --integrand --z0 $rho,0 --z1 $rho,2 --r 10 --m 1 && gnuplot ${pre}PLOT.gnuplot");

}
