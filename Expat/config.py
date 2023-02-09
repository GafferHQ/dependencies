{

	"downloads" : [

		"https://github.com/libexpat/libexpat/archive/refs/tags/R_2_5_0.tar.gz",

	],

	"url" : "https://libexpat.github.io/",

	"license" : "COPYING",

	"commands" : [

		"mkdir build",
		"cd build &&"
			" cmake"
			" -D CMAKE_INSTALL_PREFIX={buildDir}"
			" -D CMAKE_INSTALL_LIBDIR={buildDir}/lib"
			" -D CMAKE_BUILD_TYPE=Release"
			" -D EXPAT_BUILD_TOOLS=OFF"
			" -D EXPAT_BUILD_EXAMPLES=OFF"
			" -D DEXPAT_BUILD_TESTS=OFF"
			" -D DEXPAT_BUILD_DOCS=OFF"
			" ../expat",
		"cd build && make -j {jobs} && make install",

	],

	"manifest" : [

		"lib/libexpat*{sharedLibraryExtension}*",

	],

}
