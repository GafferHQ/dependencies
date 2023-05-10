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

		"{sharedLibraryDir}/{libraryPrefix}partio{sharedLibraryExtension}*",

	],

	"platform:windows" : {

		"commands" : [

			"mkdir build",
			"cd build && cmake"
				" -G {cmakeGenerator}"
                " -D CMAKE_BUILD_TYPE={cmakeBuildType}"
                " -D PARTIO_BUILD_SHARED_LIBS=ON"
				" -D CMAKE_CXX_STANDARD={c++Standard}"
				" -D CMAKE_INSTALL_PREFIX={buildDir}"
				" -D CMAKE_INSTALL_LIBDIR={buildDir}/lib"
				" ..",

			"cd build && cmake --build . --config {cmakeBuildType} --target install -- -j {jobs}",

		],

		"postMovePaths" : {

			"{buildDir}/lib/partio.dll" : "{buildDir}/bin",

		}
	},

}
