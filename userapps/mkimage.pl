#!/usr/bin/perl

# get list of bins
opendir my $dir, "appsbin";
my @files = grep { /elf$/ } readdir $dir;

open(my $imgFile,">","apps.bin");

my $imageOffset = 0;
my %apps;
#get size and entry point of each bin
for (@files)
{
    my $size = -s "appsbin/". $_;
    my $paddedSize = ($size+511)& ~0x1FF;
    my $padSize = $paddedSize-$size;

    open my $f, "appsbin/". $_;
    binmode($f);
    read($f, my $elfContent, $size);
    print $imgFile $elfContent;
    print $imgFile pack("x".$padSize,0);
    close($f);

    my $appname = $_;
    $appname =~ s/\.elf//g;
    $apps{$appname} = {size=>$size, position=> $imageOffset};

    $imageOffset += $paddedSize;
}

open(my $bootscriptFile,"<","bootscript");
open(my $indexFile,">:raw","index.bin");
binmode($indexFile);
my $pad = 0;
while (my $appname = <$bootscriptFile>)
{
    $appname =~ s/\R//g;
    print $indexFile pack("Q",$apps{$appname}{position});
    print $indexFile pack("Q",$apps{$appname}{size});
    $pad += 16;
}

$pad = 512-$pad;
print $pad;
print $indexFile pack("x".$pad,0);

