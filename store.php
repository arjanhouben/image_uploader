<?php
try
{
	include "config.php";
	foreach( [ "PASSWORD", "STORAGE", "CONVERT" ] as $i )
	{
		if ( !defined( $i ) )
		{
			throw new Exception( "please define " . $i . " inside config.php" );
		}
	}
	function store( $where, $what )
	{
		file_put_contents( $where, $what );
	}
	function resize( $from, $width )
	{
		$info = pathinfo( $from );
		$result = $width . '.' .  $info[ "extension" ];
		exec(
			CONVERT . ' ' . $from . " -thumbnail " . $width . "x " . $info[ "dirname" ] . '/' . $result
		);
		return $result;
	}
	$psw = $_POST["password"];
	if ( $psw != PASSWORD ) throw new Exception( "invalid password specified" );
	$data = $_POST["file"];
	if ( !$data ) throw new Exception( "no file data uploaded" );
	$name = $_POST["filename"];
	if ( preg_match( "/[\/\\:\*;\"'\|`]|^$/", $name ) )
	{
		throw new Exception( "the following characters are not allowed in filename: /\:;'`\"*|, empty name also not allowed" );
	}
	$extension = '.' . pathinfo( $name, PATHINFO_EXTENSION );
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
		$metadata[ "original_name" ] = $name;
		store( $prefix . $metadata[ "original" ], $data );
		$metadata[ "thumbnail" ] = resize( $prefix . $metadata[ "original" ], 120 );
		$metadata[ "forum" ] = resize( $prefix . $metadata[ "original" ], 600 );
		store( $prefix . $metadata[ "metadata" ], json_encode( $metadata ) );
	}
	echo file_get_contents( $prefix . "metadata.json" );
}
catch( Exception $e )
{
	echo json_encode( [ "error" => $e->getMessage() ] );
}
?>
