{

	"downloads" : [

		"https://github.com/openexr/openexr/releases/download/v2.3.0/ilmbase-2.3.0.tar.gz"

	],

	"url" : "http://www.openexr.com",
	"license" : "LICENSE",

	"commands" : [

		"./configure --prefix={buildDir}",
		"make -j {jobs}",
		"make install",

	],

	"manifest" : [

		"include/OpenEXR/Iex*.h",
		"include/OpenEXR/IlmBaseConfig.h",
		"include/OpenEXR/IlmThread*.h",
		"include/OpenEXR/Imath*.h",
		"include/OpenEXR/half*.h",

		"lib/libIex*{sharedLibraryExtension}*",
		"lib/libHalf*{sharedLibraryExtension}*",
		"lib/libIlmThread*{sharedLibraryExtension}*",
		"lib/libImath*{sharedLibraryExtension}*",

	],

}
