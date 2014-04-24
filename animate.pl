#!/usr/bin/perl
use strict;
use warnings;
use Math::Trig;

my @images;

my $opt_prefix = 'tmp/';
my $opt_frame_rate = 24;
my $opt_frame_div = 4;

my $opt_links_dir = 'links';
-d $opt_links_dir || mkdir($opt_links_dir) or die;

my $frame_counter = 0;
my @frame_links;
my $page_counter = 0;

open(my $makefile, '>', 'Makefile.generated') or die;

sub generate_id
{
    my ($cmd) = @_;
    $cmd =~ tr/\.\/ \t\n/_/;
    $cmd;
}

sub add_make_rule
{
    my ($dsts, $srcs, @commands) = @_;
    my $dst = join(' ', @$dsts);
    my $src = join(' ', @$srcs);

    print $makefile "$dst : $src\n";
    print $makefile "\t$_\n" for @commands;
    print $makefile "\n";
}

sub add_frame
{
    my ($path) = @_;
    my $fn_link = sprintf("$opt_links_dir/frame-%06d.png", $frame_counter++);
    push @frame_links, [$fn_link, $path];
}

sub animate
{
    my ($secs, $code) = @_;

    my $nframes = $secs * $opt_frame_rate / $opt_frame_div;
    for my $i (0 .. ($nframes-1))
    {
        print "FRAME: $i / $nframes\n";
        my $t = $i / ($nframes - 1);
        my $cmd = $code->($t);
        print "CMD: $cmd\n";

        my $id = generate_id($cmd);
        my $prefix = "$opt_prefix$id-";
        my $fn_data = "${prefix}FUNCTION.dat";
        my $fn_contour = "${prefix}CONTOUR.dat";
        my $fn_plot = "${prefix}PLOT.gnuplot";
        my $fn_frame = "${prefix}plot.png";

        add_make_rule([$fn_data, $fn_contour], ['calculate'],
                      "./calculate --prefix '$prefix' $cmd");
        add_make_rule([$fn_plot], ['gen_plot_script.pl'],
                      "./gen_plot_script.pl --data-file '$fn_data' --contour-file '$fn_contour' --output-file '$fn_frame' --terminal 'pngcairo size 1000,600' $cmd > '$fn_plot'");
        add_make_rule([$fn_frame], [$fn_data, $fn_contour, $fn_plot],
                      "gnuplot '$fn_plot'");
        add_frame($fn_frame) for 1..$opt_frame_div;
    }
}

my @animations;
while (<>)
{
    if (/^\s*%:(.*)/)
    {
        my ($line) = ($1);
        print "EVAL: $line\n";
        eval($line);
        die $@ if $@;
    }
    elsif (/^\s*\\begin{frame}/)
    {
        $page_counter++;
        my $fn_frame = sprintf("slides-png/slide-%04d.png", $page_counter);
        my $nframes = 3 * $opt_frame_rate;
        add_frame($fn_frame) for 1..$nframes;
    }
}

print $makefile "\n.PHONY: link-frames render-frames gen-frames\n";
print $makefile "\nlink-frames:\n";
print $makefile "\tln -sf ../$_->[1] $_->[0]\n" for @frame_links;
print $makefile "\nrender-frames: ";
my %seen;
for my $frame (@frame_links)
{
    my $fn_frame = $frame->[1];
    print $makefile "\\\n    $fn_frame" unless $seen{$fn_frame}++;
}
print $makefile "\n";
print $makefile "\ngen-frames: render-frames link-frames\n";

