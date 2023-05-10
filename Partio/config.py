{

	"downloads" : [

		"https://github.com/wdas/partio/archive/refs/tags/v1.14.6.tar.gz",

	],

	"url" : "http://partio.us",

	"license" : "LICENSE",

	"dependencies" : [ "ZLib" ],

	"commands" : [

		"mkdir build",
		"cd build && cmake"
			" -D CMAKE_CXX_STANDARD={c++Standard}"
			" -D CMAKE_INSTALL_PREFIX={buildDir}"
			" -D CMAKE_INSTALL_LIBDIR={buildDir}/lib"
			" ..",

		"cd build && make clean && make VERBOSE=1 -j {jobs} && make install",

	],

	"manifest" : [

		"lib/libpartio{sharedLibraryExtension}*",

	],

}
