/*
==================================
TEXPOW2 by Iikka Keranen 2001

Loads TGA files and scales them
up to the closest power of two.
Overwrites the originals, so be
careful and make backups.
==================================
*/

#include <algorithm>
#include <array>
#include <cerrno>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace
{

struct Pixel
{
	std::uint8_t r = 0;
	std::uint8_t g = 0;
	std::uint8_t b = 0;
	std::uint8_t a = 255;
};

struct Image
{
	std::uint32_t        width = 0;
	std::uint32_t        height = 0;
	std::vector< Pixel > pixels;

	Pixel &at( std::uint32_t x, std::uint32_t y )
	{
		return pixels[static_cast< std::size_t >( y ) * width + x];
	}

	const Pixel &at( std::uint32_t x, std::uint32_t y ) const
	{
		return pixels[static_cast< std::size_t >( y ) * width + x];
	}
};

[[nodiscard]] std::uint16_t readU16( const std::array< std::uint8_t, 18 > &h,
                                     std::size_t                           offset )
{
	return static_cast< std::uint16_t >( h[offset] ) | ( static_cast< std::uint16_t >( h[offset + 1] ) << 8 );
}

[[nodiscard]] std::uint32_t nextPowerOfTwo( std::uint32_t value )
{
	if ( value == 0 )
	{
		throw std::runtime_error( "Image dimensions must be nonzero." );
	}

	std::uint32_t result = 1;
	while ( result < value )
	{
		if ( result > std::numeric_limits< std::uint32_t >::max() / 2 )
		{
			throw std::runtime_error( "Image dimensions are too large." );
		}
		result *= 2;
	}
	return result;
}

[[nodiscard]] Image loadTga( const fs::path &path )
{
	std::ifstream file( path, std::ios::binary );
	if ( !file )
	{
		throw std::runtime_error( "Cannot open input file." );
	}

	std::array< std::uint8_t, 18 > header{};
	file.read( reinterpret_cast< char * >( header.data() ), header.size() );
	if ( !file )
	{
		throw std::runtime_error( "File is too small to contain a TGA header." );
	}

	const std::uint8_t  idLength = header[0];
	const std::uint8_t  colorMapType = header[1];
	const std::uint8_t  imageType = header[2];
	const std::uint16_t width = readU16( header, 12 );
	const std::uint16_t height = readU16( header, 14 );
	const std::uint8_t  bitsPerPixel = header[16];
	const std::uint8_t  descriptor = header[17];

	if ( colorMapType != 0 || ( imageType != 2 && imageType != 3 ) )
	{
		throw std::runtime_error(
		"Only uncompressed, non-paletted TGA files are supported." );
	}
	if ( ( descriptor & 0xC0 ) != 0 )
	{
		throw std::runtime_error( "Interleaved TGA images are not supported." );
	}
	if ( ( imageType == 2 && bitsPerPixel != 24 && bitsPerPixel != 32 ) ||
	     ( imageType == 3 && bitsPerPixel != 8 ) )
	{
		throw std::runtime_error(
		"Supported formats are 8-bit grayscale and 24-/32-bit color." );
	}
	if ( width == 0 || height == 0 )
	{
		throw std::runtime_error( "TGA dimensions must be nonzero." );
	}

	// Skip the optional image ID field.
	file.seekg( idLength, std::ios::cur );
	if ( !file )
	{
		throw std::runtime_error( "Could not read the TGA image ID." );
	}

	Image image;
	image.width = width;
	image.height = height;
	image.pixels.resize( static_cast< std::size_t >( width ) * height );

	const std::size_t           bytesPerPixel = bitsPerPixel / 8;
	std::vector< std::uint8_t > raw( bytesPerPixel );

	const bool topOrigin = ( descriptor & 0x20 ) != 0;
	const bool rightOrigin = ( descriptor & 0x10 ) != 0;

	for ( std::uint32_t fileY = 0; fileY < height; ++fileY )
	{
		const std::uint32_t y = topOrigin ? fileY : height - 1 - fileY;

		for ( std::uint32_t fileX = 0; fileX < width; ++fileX )
		{
			file.read( reinterpret_cast< char * >( raw.data() ),
			           static_cast< std::streamsize >( raw.size() ) );
			if ( !file )
			{
				throw std::runtime_error( "Unexpected end of TGA pixel data." );
			}

			const std::uint32_t x = rightOrigin ? width - 1 - fileX : fileX;
			Pixel               pixel;

			if ( imageType == 3 )
			{
				pixel.r = pixel.g = pixel.b = raw[0];
			}
			else
			{
				// TGA stores color channels in B, G, R, A order.
				pixel.b = raw[0];
				pixel.g = raw[1];
				pixel.r = raw[2];
				pixel.a = bitsPerPixel == 32 ? raw[3] : 255;
			}

			image.at( x, y ) = pixel;
		}
	}

	return image;
}

[[nodiscard]] std::uint8_t interpolateChannel( std::uint8_t a, std::uint8_t b,
                                               std::uint8_t c, std::uint8_t d,
                                               double fx, double fy )
{
	const double top = a * ( 1.0 - fx ) + b * fx;
	const double bottom = c * ( 1.0 - fx ) + d * fx;
	const double value = top * ( 1.0 - fy ) + bottom * fy;
	return static_cast< std::uint8_t >(
	std::clamp( std::lround( value ), 0L, 255L ) );
}

[[nodiscard]] Pixel interpolate( const Pixel &p00, const Pixel &p10,
                                 const Pixel &p01, const Pixel &p11,
                                 double fx, double fy )
{
	return {
		interpolateChannel( p00.r, p10.r, p01.r, p11.r, fx, fy ),
		interpolateChannel( p00.g, p10.g, p01.g, p11.g, fx, fy ),
		interpolateChannel( p00.b, p10.b, p01.b, p11.b, fx, fy ),
		interpolateChannel( p00.a, p10.a, p01.a, p11.a, fx, fy )
	};
}

[[nodiscard]] Image scaleToPowerOfTwo( const Image &source )
{
	Image result;
	result.width = nextPowerOfTwo( source.width );
	result.height = nextPowerOfTwo( source.height );

	if ( result.width > 65535 || result.height > 65535 )
	{
		throw std::runtime_error( "Scaled dimensions exceed the TGA limit." );
	}

	result.pixels.resize(
	static_cast< std::size_t >( result.width ) * result.height );

	for ( std::uint32_t y = 0; y < result.height; ++y )
	{
		// Map destination pixel centers to source pixel centers.
		const double sourceY =
		( static_cast< double >( y ) + 0.5 ) * source.height / result.height - 0.5;
		const double clampedY = std::clamp( sourceY, 0.0,
		                                    static_cast< double >( source.height - 1 ) );
		const auto   y0 = static_cast< std::uint32_t >( std::floor( clampedY ) );
		const auto   y1 = std::min( y0 + 1, source.height - 1 );
		const double fy = clampedY - y0;

		for ( std::uint32_t x = 0; x < result.width; ++x )
		{
			const double sourceX =
			( static_cast< double >( x ) + 0.5 ) * source.width / result.width - 0.5;
			const double clampedX = std::clamp( sourceX, 0.0,
			                                    static_cast< double >( source.width - 1 ) );
			const auto   x0 = static_cast< std::uint32_t >( std::floor( clampedX ) );
			const auto   x1 = std::min( x0 + 1, source.width - 1 );
			const double fx = clampedX - x0;

			result.at( x, y ) = interpolate(
			source.at( x0, y0 ), source.at( x1, y0 ),
			source.at( x0, y1 ), source.at( x1, y1 ), fx, fy );
		}
	}

	return result;
}

void writeU16( std::ofstream &file, std::uint16_t value )
{
	file.put( static_cast< char >( value & 0xFF ) );
	file.put( static_cast< char >( ( value >> 8 ) & 0xFF ) );
}

void saveTga( const Image &image, const fs::path &path )
{
	std::ofstream file( path, std::ios::binary );
	if ( !file )
	{
		throw std::runtime_error( "Cannot open output file." );
	}

	// 18-byte TGA header: uncompressed true-color, top-left origin, 8 alpha bits.
	file.put( 0 );       // ID length
	file.put( 0 );       // No color map
	file.put( 2 );       // Uncompressed true-color image
	writeU16( file, 0 ); // Color map origin
	writeU16( file, 0 ); // Color map length
	file.put( 0 );       // Color map depth
	writeU16( file, 0 ); // X origin
	writeU16( file, 0 ); // Y origin
	writeU16( file, static_cast< std::uint16_t >( image.width ) );
	writeU16( file, static_cast< std::uint16_t >( image.height ) );
	file.put( 32 );   // Bits per pixel
	file.put( 0x28 ); // Top-left origin, 8 alpha bits

	for ( std::uint32_t y = 0; y < image.height; ++y )
	{
		for ( std::uint32_t x = 0; x < image.width; ++x )
		{
			const Pixel &p = image.at( x, y );
			file.put( static_cast< char >( p.b ) );
			file.put( static_cast< char >( p.g ) );
			file.put( static_cast< char >( p.r ) );
			file.put( static_cast< char >( p.a ) );
		}
	}

	if ( !file )
	{
		throw std::runtime_error( "Failed while writing output file." );
	}
}

[[nodiscard]] bool hasTgaExtension( const fs::path &path )
{
	std::string extension = path.extension().string();
	std::transform( extension.begin(), extension.end(), extension.begin(),
	                []( unsigned char c )
	                {
						return static_cast< char >( std::tolower( c ) );
					} );
	return extension == ".tga";
}

} // namespace

int main( int argc, char *argv[] )
{
	std::vector< fs::path > inputs;
	fs::path                outputDirectory;
	bool                    useOutputDirectory = false;

	for ( int i = 1; i < argc; ++i )
	{
		const std::string arg = argv[i];

		if ( arg == "-o" )
		{
			if ( i + 1 >= argc )
			{
				std::cerr << "Error: -o requires an output directory.\n";
				return EINVAL;
			}
			outputDirectory = argv[++i];
			useOutputDirectory = true;
		}
		else if ( !arg.empty() && arg[0] == '-' )
		{
			std::cerr << "Unknown option: " << arg << '\n';
			return EINVAL;
		}
		else if ( hasTgaExtension( arg ) )
		{
			inputs.emplace_back( arg );
		}
		else
		{
			std::cerr << "Skipping non-TGA file: " << arg << '\n';
		}
	}

	if ( inputs.empty() )
	{
		std::cerr << "Usage: texpow2 <source.tga> [source2.tga ...] [-o DIR]\n";
		return EINVAL;
	}

	if ( useOutputDirectory )
	{
		std::error_code ec;
		fs::create_directories( outputDirectory, ec );
		if ( ec )
		{
			std::cerr << "Cannot create output directory: "
					  << outputDirectory << ": " << ec.message() << '\n';
			return ec.value();
		}
	}

	int failures = 0;
	for ( const auto &inputPath : inputs )
	{
		const fs::path outputPath = useOutputDirectory ? outputDirectory / inputPath.filename() : inputPath;

		try
		{
			const Image source = loadTga( inputPath );
			const Image scaled = scaleToPowerOfTwo( source );
			saveTga( scaled, outputPath );

			std::cout << inputPath.string() << " -> "
					  << outputPath.string() << " ("
					  << source.width << 'x' << source.height << " -> "
					  << scaled.width << 'x' << scaled.height << ")\n";
		}
		catch ( const std::exception &e )
		{
			++failures;
			std::cerr << inputPath.string() << ": " << e.what() << '\n';
		}
	}

	return failures == 0 ? 0 : 1;
}
