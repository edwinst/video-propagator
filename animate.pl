#!/usr/bin/perl
use strict;
use warnings;

my @images;

my @slide_images = sort glob('slides-png/*.png');

my $opt_prefix = 'tmp/';
my $opt_frame_rate = 24;
my $opt_frame_div = 4;

my $opt_links_dir = 'links';
-d $opt_links_dir || mkdir($opt_links_dir) or die;

my $frame_counter = 0;
my @frame_links;
my $page_counter = 0;

open(my $makefile, '>', 'Makefile.generated') or die;

my $i = 0;
for my $img (@slide_images)
{
    for (0..49)
    {
        my $linkname = $opt_links_dir.'/link-'.sprintf('%06d', $i).'.png';
        link($img, $linkname);
        $i++;
    }
}

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
    my $fn_link = sprintf("links/frame-%06d.png", $frame_counter++);
    push @frame_links, [$fn_link, $path];
}

sub animate
{
    my ($secs, $opts, @params) = @_;

    my $nframes = $secs * $opt_frame_rate / $opt_frame_div;
    for my $i (0 .. ($nframes-1))
    {
        print "FRAME: $i / $nframes\n";
        my $t = $i / ($nframes - 1);
        my @args;
        for my $param (@params)
        {
            push @args, ref($param) ? $param->($t) : $param;
        }
        my $cmd = sprintf($opts, @args);
        print "CMD: $cmd\n";

        my $id = generate_id($cmd);
        my $prefix = "$opt_prefix$id-";
        my $fn_data = "${prefix}FUNCTION.dat";
        my $fn_contour = "${prefix}CONTOUR.dat";
        my $fn_plot = "${prefix}PLOT.gnuplot";
        my $fn_frame = "${prefix}plot.png";

        add_make_rule([$fn_data, $fn_contour, $fn_plot], [],
                      "./calculate --prefix '$prefix' $cmd");
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
        eval($line);
    }
    elsif (/^\s*\\begin{frame}/)
    {
        $page_counter++;
        my $fn_frame = sprintf("slides-png/slide-%04d.png", $page_counter);
        my $nframes = 3 * $opt_frame_rate;
        add_frame($fn_frame) for 1..$nframes;
    }
}

print $makefile "\n.PHONY: link-frames render-frames\n";
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

