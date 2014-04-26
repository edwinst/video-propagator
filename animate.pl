#!/usr/bin/perl
use strict;
use warnings;
use Digest::MD5 qw(md5_hex);
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

my %make_rules_seen;

my $sequence_name;

my $opts; # XXX HACK

my $im_env = 'label-envelope.png -geometry +100+60 -composite';
my $im_igd = 'label-integrand.png -geometry +100+60 -composite';
my $im_int = 'label-integral.png -geometry +120+60 -composite';

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

    return if $make_rules_seen{$dst}++;

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


sub t_ab
{
    my ($n, $beg, $mid, $end, $t) = @_;
    return 0 if $t <= $beg;

    $t -= $beg;
    my $delta = (1 - $beg - $end - ($n - 1) * $mid) / $n;
    my $direction = 1;
    for my $i (1..$n)
    {
        if ($t < $delta)
        {
            return 0.5 * (1 - $direction) + $direction * $t / $delta;
        }
        $t -= $delta;
        $direction = -$direction;
        return 0.5 * (1 - $direction) if $t < $mid;
        $t -= $mid;
    }

    return 0.5 * (1 - $direction);
}


sub t_xaby
{
    my ($n, $beg, $mid, $end, $x, $a, $b, $y, $t) = @_;
    return $x if $t <= $beg;

    my $func = sub { sin(pi/2*$_[0])**2 };

    $t -= $beg;
    my $delta = (1 - $beg - $end - ($n - 1) * $mid) / $n;
    my $from = $x;
    my $dest = 0;
    for my $i (1..$n)
    {
        my $to = ($i < $n) ? ($dest ? $b : $a) : $y;
        if ($t < $delta)
        {
            return interpol_real($from, $to, $func->($t/$delta));
        }
        $t -= $delta;

        return $to if $t < $mid;
        $t -= $mid;

        $dest = !$dest;
        $from = $to;
    }

    return $y;
}


sub interpol_real
{
    my ($from, $to, $t) = @_;
    $from + $t * ($to - $from);
}


sub interpol_complex
{
    my ($z0_re, $z0_im, $z1_re, $z1_im, $t) = @_;
    (interpol_real($z0_re, $z1_re, $t), interpol_real($z0_im, $z1_im, $t));
}


sub seq_name
{
    my ($name) = @_;
    $sequence_name = $name;
}


sub animate
{
    my ($secs, $code) = @_;

    my @frames;
    my %frames_seen;
    my $nframes = $secs * $opt_frame_rate / $opt_frame_div;
    for my $i (0 .. ($nframes-1))
    {
        my $t = $i / ($nframes - 1);
        my $cmd = $code->($t);

        my $plot_opts = '';
        my $im_opts = '';
        my $plot_id = '';
        if (ref($cmd))
        {
            ($cmd, $plot_opts, $im_opts) = @$cmd;
            $im_opts = '' if !defined($im_opts);
            $plot_id = md5_hex($plot_opts.$im_opts);
            $plot_opts =~ s/\n/\\\n/g;
            $im_opts =~ s/\n/\\\n/g;
        }

        my $id = generate_id($cmd);
        my $prefix = "$opt_prefix$id-";
        my $fn_data = "${prefix}FUNCTION.dat";
        my $fn_contour = "${prefix}CONTOUR.dat";
        my $fn_script = "${prefix}${plot_id}PLOT.gnuplot";
        my $fn_plot = "${prefix}${plot_id}plot.png";
        my $fn_frame = "${prefix}${plot_id}frame.png";

        $fn_plot = $fn_frame if $im_opts eq '';

        add_make_rule([$fn_data, $fn_contour], ['calculate'],
                      "./calculate --prefix '$prefix' $cmd");
        add_make_rule([$fn_script], ['gen_plot_script.pl'],
                      "./gen_plot_script.pl --data-file '$fn_data' --contour-file '$fn_contour' --output-file '$fn_plot' --terminal 'pngcairo dashed size 1000,600' $plot_opts $cmd > '$fn_script'");
        add_make_rule([$fn_plot], [$fn_data, $fn_contour, $fn_script],
                      "gnuplot '$fn_script'");
        if ($im_opts ne '')
        {
            add_make_rule([$fn_frame], [$fn_plot], "convert '$fn_plot' $im_opts '$fn_frame'");
        }
        add_frame($fn_frame) for 1..$opt_frame_div;
        push @frames, $fn_frame unless $frames_seen{$fn_frame}++;
    }

    if (defined($sequence_name))
    {
        add_make_rule(["seq-$sequence_name"], \@frames);
        add_make_rule(["display-seq-$sequence_name"], ["seq-$sequence_name"],
                      "display ".join(' ', @frames));
    }

    undef $sequence_name;
}

my @animations;
my $code = '';
while (<>)
{
    if (/^\s*%:(.*)/)
    {
        my ($line) = ($1);
        $code .= "$line\n";
    }
    else
    {
        if ($code ne '')
        {
            eval($code);
            die $@ if $@;
            $code = '';
        }

        if (/^\s*\\begin{frame}/ || /^\s*\\pause\b/)
        {
            $page_counter++;
            my $fn_frame = sprintf("slides-png/slide-%04d.png", $page_counter);
            my $nframes = 3 * $opt_frame_rate;
            add_frame($fn_frame) for 1..$nframes;
        }
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

