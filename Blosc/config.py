{

	"downloads" : [

		"https://github.com/Blosc/c-blosc/archive/1.15.1.tar.gz"

	],

	"url" : "https://blosc.org",

	"license" : "LICENSES",

	"commands" : [

		"cmake -DCMAKE_INSTALL_PREFIX={buildDir} .",
		# Note : Blosc does not declare its build dependencies
		# correctly, so we cannot do a parallel build with `-j`.
		"make install VERBOSE=1",

	],

	"manifest" : [

		"include/blosc*.h",
		"lib/libblosc*{sharedLibraryExtension}*",

	],

}
