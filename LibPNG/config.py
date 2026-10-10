{

	"downloads" : [

		"https://github.com/pnggroup/libpng/archive/refs/tags/v1.6.59.tar.gz"

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
