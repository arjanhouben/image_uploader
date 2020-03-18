<?php
try
{
	include "config.php";
	foreach( [ "PASSWORD", "STORAGE", "CONVERT", "HEIF_CONVERT" ] as $i )
	{
		if ( !defined( $i ) )
		{
			throw new Exception( "please define " . $i . " inside config.php" );
		}
	}
	foreach( [ "password", "file", "filename" ] as $i )
	{
		if ( !array_key_exists( $i, $_POST ) )
		{
			throw new Exception( "please supply " . $i . " in POST data" );
		}
	}
	function store( $where, $what )
	{
		file_put_contents( $where, $what );
	}
	function resize( $from, $width )
	{
		$info = pathinfo( $from );
		$extension = '.' .  strtolower( $info[ "extension" ] );
		if ( $extension == ".heic" ) $extension = ".jpg";
		$result = $width . $extension;
		exec(
			CONVERT . ' ' . $from . " -thumbnail " . $width . "x " . $info[ "dirname" ] . '/' . $result
		);
		return $result;
	}
	$psw = $_POST[ "password" ];
	if ( $psw != PASSWORD ) throw new Exception( "invalid password specified" );
	$data = $_POST[ "file" ];
	if ( !$data ) throw new Exception( "no file data uploaded" );
	$filename = $_POST[ "filename" ];
	if ( preg_match( "/[\/\\:\*;\"'\|`]|^$/", $filename ) )
	{
		throw new Exception( "the following characters are not allowed in filename: /\:;'`\"*|, empty name also not allowed" );
	}
	$info = pathinfo( $filename );
	$extension = '.' . strtolower( $info[ "extension" ] );
	$data = base64_decode( $data );
	$signature = md5( $data );
	$prefix = STORAGE . $signature . '/';
	if ( !is_dir( $prefix ) )
	{
		$metadata = array();
		$metadata[ "metadata" ] = "metadata.json";
		mkdir( $prefix );
		$metadata[ "prefix" ] = $prefix;
		$metadata[ "original" ] = "original" . $extension;
		$metadata[ "original_name" ] = $filename;
		$source = $metadata[ "original" ];
		store( $prefix . $source, $data );
		if ( $extension == ".heic" )
		{
			$intermediate = "intermediate.png";
			exec(
				HEIF_CONVERT . ' ' . $prefix . '/' . $source . ' ' . $prefix . '/' . $intermediate
			);
			$source = $intermediate;
		}
		$metadata[ "thumbnail" ] = resize( $prefix . $source, 120 );
		$metadata[ "forum" ] = resize( $prefix . $source, 600 );
		store( $prefix . $metadata[ "metadata" ], json_encode( $metadata ) );
	}
	echo file_get_contents( $prefix . "metadata.json" );
}
catch( Exception $e )
{
	echo json_encode( [ "error" => $e->getMessage() ] );
}
?>
