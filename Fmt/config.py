{

	"downloads" : [

		"https://github.com/fmtlib/fmt/archive/refs/tags/9.1.0.tar.gz",

	],

	"url" : "https://fmt.dev",

	"license" : "LICENSE.rst",

	"commands" : [

		"mkdir build",
		"cd build && "
			" cmake"
			" -D CMAKE_INSTALL_PREFIX={buildDir}"
			" -D CMAKE_INSTALL_LIBDIR={buildDir}/lib"
			" -D BUILD_SHARED_LIBS=ON"
			" -D FMT_TEST=OFF "
			" ..",
		"cd build && make -j {jobs} && make install",

	],

	"manifest" : [

		"include/fmt",
		"{sharedLibraryDir}/{libraryPrefix}fmt*{sharedLibraryExtension}*",
		"lib/{libraryPrefix}fmt*.lib"

	],

	"platform:windows" : {

		"commands" : [
			"mkdir build",
			"cd build && "
				" cmake"
				" -G {cmakeGenerator}"
				" -D CMAKE_BUILD_TYPE={cmakeBuildType}"
				" -D CMAKE_INSTALL_PREFIX={buildDir}"
				" -D CMAKE_INSTALL_LIBDIR={buildDir}/lib"
				" -D BUILD_SHARED_LIBS=ON"
				" -D FMT_TEST=OFF "
				" ..",
			"cd build && cmake --build . --config {cmakeBuildType} --target install -- -j {jobs}",
		],

	}

}
