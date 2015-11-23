#!/usr/bin/perl


our $imgFile;
our %apps;
our $imageOffset = 512*10;

sub addDirectory
{
    my $dirname = shift;
    opendir my $dir, $dirname;
    #my @files = grep { /elf$/ } readdir $dir;
    my @files = readdir $dir;

    for (@files)
    {
        next if ($_ =~ m/^\./);
        my $size = -s $dirname . "/" . $_;
        my $paddedSize = ($size+511)& ~0x1FF;
        my $padSize = $paddedSize-$size;
    
        open my $f, $dirname . "/" . $_;
        binmode($f);
        read($f, my $elfContent, $size);
        print $imgFile $elfContent;
        print $imgFile pack("x".$padSize,0);
        close($f);
            
        my $appname = $_;
        $appname =~ s/\.elf//;
        $apps{$appname} = {size=>$size, position=> $imageOffset};
    
        $imageOffset += $paddedSize;
    }   
}


# get list of bins
open($imgFile,">","apps.bin");
addDirectory("appsbin");
addDirectory("bulkfiles");

# create index sector: This is a lame file system. Just an index in the first sector
open(my $indexFile,">:raw","index.bin");
binmode($indexFile);
my $pad = 512*10;
foreach my $key (keys %apps)
{
    printf $indexFile sprintf("% -32s",$key);
    print $indexFile pack("q",$apps{$key}{position});
    print $indexFile pack("Q",$apps{$key}{size});

    $pad -= (32+8+8);
}
print $indexFile pack("x".$pad,0);



