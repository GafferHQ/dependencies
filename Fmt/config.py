{

	"downloads" : [

		"https://github.com/fmtlib/fmt/archive/refs/tags/12.1.0.tar.gz",

	],

	"url" : "https://fmt.dev",

	"license" : "LICENSE",

	"commands" : [

		"mkdir build",
		"cd build && "
			" cmake"
			" -D CMAKE_CXX_STANDARD={c++Standard}"
			" -D CMAKE_INSTALL_PREFIX={buildDir}"
			" -D CMAKE_INSTALL_LIBDIR={buildDir}/lib"
			" -D BUILD_SHARED_LIBS=ON"
			" -D FMT_TEST=OFF "
			" ..",
		"cd build && make -j {jobs} && make install",

	],

	"manifest" : [

		"include/fmt",
		"lib/libfmt*{sharedLibraryExtension}*",

	],

}
