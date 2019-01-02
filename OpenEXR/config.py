{

	"downloads" : [

		"https://github.com/openexr/openexr/releases/download/v2.3.0/openexr-2.3.0.tar.gz"

	],

	"license" : "LICENSE",

	"dependencies" : [ "IlmBase" ],

	"commands" : [

		"./configure --prefix={buildDir}",
		"make -j {jobs}",
		"make install",

	],

	"manifest" : [

		"bin/exrheader",
		"include/OpenEXR/Imf*.h",
		"include/OpenEXR/OpenEXRConfig.h",
		"lib/libIlmImf*{sharedLibraryExtension}*",

	]

}
