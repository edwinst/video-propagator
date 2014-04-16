#!/usr/bin/perl
use strict;
use warnings;

my @images;

my @slide_images = sort glob('slides-png/*.png');

my $opt_links_dir = 'links';
-d $opt_links_dir || mkdir($opt_links_dir) or die;

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
