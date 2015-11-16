#!/usr/bin/perl

# get list of bins
opendir my $dir, "appsbin";
my @files = grep { /elf$/ } readdir $dir;

open(my $imgFile,">","apps.bin");

my $imageOffset = 0x400; # to skip bootscript which is at sector 1 and the index sector at 0.
my %apps;
my $index;

# put the bootscript name in the index sector
$apps{"bootscript"} = {size=>0x200, position=> 0x200};

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


# create index sector: This is a lame file system. Just an index in the first sector
open(my $indexFile,">:raw","index.bin");
binmode($indexFile);
my $pad = 512;
foreach my $key (keys %apps)
{
    printf $indexFile sprintf("% -32s",$key);
#    print $indexFile ;
    print $indexFile pack("q",$apps{$key}{position});
    print $indexFile pack("Q",$apps{$key}{size});

    $pad -= (32+8+8);
}
print $indexFile pack("x".$pad,0);

# now copy bootscript on 2nd sector
$size = -s "bootscript";
$pad = 512-$size;
open(my $bootscriptFile,"<","bootscript");
read($bootscriptFile,my $txt, $size);
print $indexFile $txt;
print $indexFile pack("x".$pad,0);
close($bootscriptFile);


