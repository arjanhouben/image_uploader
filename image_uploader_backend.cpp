#include <span>
#include <cstdlib>
#include <string>
#include <charconv>
#include <iostream>
#include <string_view>
#include <unordered_map>
#include <fstream>
#include <functional>
#include <filesystem>
#include "fmt/format.h"
#include "nlohmann/json.hpp"

std::string_view env( const char *str )
{
	if ( auto result = getenv( str ) )
	{
		return result;
	}
	throw std::runtime_error( "missing environment variable: " + std::string( str ) );
}

template < typename T >
T to_number( std::string_view str )
{
	T result;
	auto [ ptr, ec ] = std::from_chars( str.begin(), str.end(), result );
	if ( ec == std::errc() )
	{
		return result;
	}
	throw std::system_error( std::make_error_code( ec ) );
}

std::string_view remove_quotes( std::string_view str )
{
	if ( str.size() >= 2 )
	{
		switch ( str.front() )
		{
			case '"':
			case '\'':
				if ( str.back() == str.front() ) 
				{
					return str.substr( 1, str.size() - 2 );
				}
		}
	}
	return str;
}

std::vector< std::string_view > split( std::string_view str, std::string_view separator )
{
	std::vector< std::string_view > result;
	auto pos = str.find( separator );
	while ( pos != str.npos )
	{
		result.push_back( str.substr( 0, pos ) );
		str = str.substr( pos + separator.size() );
		pos = str.find( separator );
	}
	result.push_back( str );
	return result;
}

template < typename T, typename F >
constexpr auto for_each( T t, F f )
{
	for ( auto &i : t ) i = f( i );
	return t;
}

constexpr std::string_view trim( std::string_view str )
{
	while ( !str.empty() && std::isspace( str.front() ) )
	{
		str = str.substr( 1 );
	}
	while ( !str.empty() && std::isspace( str.back() ) )
	{
		str = str.substr( 0, str.size() - 1 );
	}
	return str;
}

auto get_post_from( std::istream &stream, std::string_view ignore )
{
	std::unordered_map< std::string, std::string > result;
	std::string line;
	std::string attribute, value;
	constexpr std::string_view content = "Content-Disposition: form-data;";
	while ( std::getline( stream, line ) )
	{
		const auto trimmed_line = trim( line );
		
		if ( const auto start_of_multipart_indicator = trimmed_line.find( ignore );
			start_of_multipart_indicator != std::string_view::npos )
		{
			if ( ( start_of_multipart_indicator + ignore.size() ) < trimmed_line.size() )
			{
				break;
			}
			value.clear();
			continue;
		}
		if ( trimmed_line.starts_with( content ) )
		{
			const auto attr_value = for_each( split( trimmed_line.substr( content.size() ), "=" ), trim );
			value = remove_quotes( attr_value.at( 1 ) );
		}
		else if ( !value.empty() )
		{
			result[ value ] += trimmed_line;
		}
	}
	return result;
}

std::vector< std::byte > base64_decode( std::string_view in )
{
	std::vector< std::byte > out;
	out.reserve( in.size() * 64 / 255 );

    constexpr std::string_view lookup = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    size_t val = 0;
	int valb = -8;
    for ( auto c : in )
	{
		const auto value = lookup.find( c );
		if ( value != std::string_view::npos )
		{
			val = ( val << 6 ) + value;
			valb += 6;
			if ( valb >= 0 )
			{
				out.push_back( std::byte( ( val >> valb ) & 0xFF ) );
				valb -= 8;
			}
		}
    }
    return out;
}

template < typename T >
void store( std::filesystem::path path, std::span< T > data )
{
	std::ofstream( path.c_str(), std::ios::binary ).write( reinterpret_cast< const char* >( data.data() ), data.size() );
}

std::string_view extension( std::string_view str )
{
	const auto filename_start = std::max( str.find_last_of( '/' ), 0ul );
	const auto extension_start = str.find( '.', filename_start );
	return str.substr( extension_start );
}

std::string tolower( std::string_view str )
{
	std::string result;
	std::transform( 
		str.begin(), 
		str.end(),
		std::back_insert_iterator( result ),
		[]( auto c ) { return std::tolower( c ); }
	);
	return result;
}

const std::string CONVERT = "/opt/local/bin/convert";

std::filesystem::path resize( std::filesystem::path source, int width )
{
	auto ext = tolower( extension( source.string() ) );
	if ( ext == ".heic" ) ext = ".jpg";
	std::filesystem::path result = std::to_string( width ) + ext;
	system(
		( CONVERT + ' ' + source.string() + " -thumbnail " + std::to_string( width ) + "x " + source.parent_path().string() + '/' + result.string() ).c_str()
	);
	return result;
}

std::string handle_file( const nlohmann::json &js )
{
	const auto data = base64_decode( js.at( "file" ).get< std::string >() );
	const auto signature = std::to_string(
		std::hash< std::string_view >{}( std::string_view{ reinterpret_cast< const char* >( data.data() ), data.size() } )
	);

	const auto filename = std::filesystem::path( js.at( "filename" ).get< std::string >() );
	const auto extension = filename.extension();
	const auto prefix = std::filesystem::path( "storage" ) / signature;
	if ( !std::filesystem::is_directory( prefix ) )
	{
		nlohmann::json metadata;
		metadata[ "metadata" ] = "metadata.json";
		std::filesystem::create_directories( prefix );
		metadata[ "prefix" ] = prefix.string();
		metadata[ "original" ] = "original" + extension.string();
		metadata[ "original_name" ] = filename.string();
		const auto source = metadata[ "original" ];
		store( prefix / ( "original" + extension.string() ), std::span{ data } );
		metadata[ "thumbnail" ] = resize( prefix / source, 120 ).string();
		metadata[ "forum" ] = resize( prefix / source, 600 ).string();
		store( prefix / metadata[ "metadata" ], std::span{ metadata.dump() } );
		return metadata.dump();
	}
	std::ifstream file( ( prefix / "metadata.json" ).c_str() );
	return std::string( std::istream_iterator< char >{ file }, std::istream_iterator< char >{} );
}

#ifndef PASSWORD
#error "Please define PASSWORD for password checking
#endif

int main( int argc, char *argv[] )
{
	nlohmann::json js;
	fmt::print( "Content-type: application/json\r\n\r\n" );
	// const auto args = std::span{ argv, static_cast< size_t >( argc ) };
	try
	{
		// const auto content_length = to_number< size_t >( env( "CONTENT_LENGTH" ) );
		const auto content_type = for_each( split( env( "CONTENT_TYPE" ), ";" ), trim );
		constexpr std::string_view boundary = "boundary=";
		std::string multi_part_boundary;
		for ( auto i : content_type )
		{
			if ( i.starts_with( boundary ) )
			{
				auto part = i.substr( boundary.size() );
				multi_part_boundary = part.substr( part.find_first_not_of( "-" ) );
			}
		}
		for ( auto &[k,v] : get_post_from( std::cin, multi_part_boundary ) )
		{
			js[ k ] = v;
		}

		if ( !js.contains( "password" ) )
		{
			throw std::runtime_error( "no password specified" );
		}
		if ( PASSWORD != js[ "password" ].get< std::string >() )
		{
			throw std::runtime_error( "invalid password" );
		}

		fmt::print( "{}", handle_file( js ) );
	}
	catch( const std::exception &err )
	{
		js[ "error" ] = err.what();
		fmt::print( "{}", js.dump() );
	}

	return 0;
}