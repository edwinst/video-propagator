#!/usr/bin/perl

for my $i (0..10)
{
    my $pre = sprintf("test-%04d-", $i);
    my $m = 0.1 * ($i + 1);

    #system("./calculate --prefix $pre --integrand --z0 -4 --z1 4 --r 10 --m $m && gnuplot ${pre}PLOT.gnuplot");

    #my $rho = $i * 0.05 / 4;
    #system("./calculate --n 500 --prefix $pre --envelope --z0 -4,$rho --z1 4,$rho --r 20 --m 2 && gnuplot ${pre}PLOT.gnuplot");
    #system("./calculate --n 500 --prefix $pre --integrand --z0 -4,$rho --z1 4,$rho --r 2 --m 1 && gnuplot ${pre}PLOT.gnuplot");
    #my $rho = -1 + 0.05 / 4 * $i;
    #system("./calculate --n 500 --prefix $pre --integrand --z0 $rho,0 --z1 $rho,2 --r 10 --m 1 && gnuplot ${pre}PLOT.gnuplot");

    my $t = 60;
    my $d = $i * 0.1 / 60;
    my $Pr = 40 + $i;
    system("./calculate --n 500 --prefix $pre --z0 0.1 --z1 10 --m 1 --Pr 45 --t 45 --d $d && gnuplot ${pre}PLOT.gnuplot");
    #system("./calculate --n 500 --prefix $pre --z0 0.1 --z1 10 --m 1 --t 0 --Pr $Pr && gnuplot ${pre}PLOT.gnuplot");
}
