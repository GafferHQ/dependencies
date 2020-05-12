{

	"downloads" : [

		"https://download.sourceforge.net/libpng/libpng-1.6.3.tar.gz"

	],

	"url" : "http://www.libpng.org",
	"license" : "LICENSE",

	"commands" : [

		"./configure --prefix={buildDir}",
		"make -j {jobs}",
		"make install",

	],

	"manifest" : [

		"include/png*",
		"include/libpng*",
		"lib/libpng*{sharedLibraryExtension}*",

	],

}
