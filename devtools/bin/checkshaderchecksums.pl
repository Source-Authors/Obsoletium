use String::CRC32;
BEGIN {use File::Basename; push @INC, dirname($0); }
require "valve_perl_helpers.pl";

sub GetShaderType
{
	my $shadername = shift;
	my $shadertype;
	if( $shadername =~ m/\.vsh/i )
	{
		$shadertype = "vsh";
	}
	elsif( $shadername =~ m/\.psh/i )
	{
		$shadertype = "psh";
	}
	elsif( $shadername =~ m/\.fxc/i )
	{
		$shadertype = "fxc";
	}
	else
	{
		die;
	}
	return $shadertype;
}

sub GetShaderSrc
{
	my $shadername = shift;
	if ( $shadername =~ m/^(.*)-----/i )
	{
		return $1;
	}
	else
	{
		return $shadername;
	}
}

sub GetShaderType
{
	my $shadername = shift;
	my $shadertype;
	if( $shadername =~ m/\.vsh/i )
	{
		$shadertype = "vsh";
	}
	elsif( $shadername =~ m/\.psh/i )
	{
		$shadertype = "psh";
	}
	elsif( $shadername =~ m/\.fxc/i )
	{
		$shadertype = "fxc";
	}
	else
	{
		die;
	}
	return $shadertype;
}

sub GetShaderBase
{
	my $shadername = shift;
	if ( $shadername =~ m/-----(.*)$/i )
	{
		return $1;
	}
	else
	{
		my $shadertype = &GetShaderType( $shadername );
		$shadername =~ s/\.$shadertype//i;
		return $shadername;
	}
}

$g_vcsext		= ".vcs";

while( 1 )
{
	$inputbase = shift;

	if( $inputbase =~ m/-x360/ )
	{
	}
	else
	{
		last;
	}
}

# rip the txt off the end if it's there.
$inputbase =~ s/\.txt//i;

my @srcfiles = &LoadShaderListFile( $inputbase );
 
foreach $srcfile ( @srcfiles )
{
	my $shadertype = &GetShaderType( $srcfile );
	my $shaderbase = &GetShaderBase( $srcfile );
	my $shadersrc = &GetShaderSrc( $srcfile );
	my $vcsFileName = "..\\..\\..\\game\\hl2\\shaders\\$shadertype\\$shaderbase" . $g_vcsext;
#	print "shadersrc: $shadersrc vcsFileName: $vcsFileName\n";

	&CheckCRCAgainstTarget( $shadersrc, $vcsFileName, 1 );
}
