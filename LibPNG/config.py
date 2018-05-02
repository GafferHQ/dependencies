{

	"downloads" : [

		"https://download.sourceforge.net/libpng/libpng-1.6.37.tar.gz"

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
		"lib/libpng*{sharedLibraryExtension}*",	# lib prefix is accurate for all platforms
		"lib/libpng*.lib",

	],
	"platform:windows" : {

		"commands" : [

			"mkdir gafferBuild",
			"cd gafferBuild && "
				" cmake"
				" -G {cmakeGenerator}"
				" -D CMAKE_BUILD_TYPE={cmakeBuildType}"
				" -D CMAKE_INSTALL_PREFIX={buildDir}"
				" -D ZLIB_INCLUDE_DIR={buildDir}\\include"
				" -D ZLIB_LIBRARY={buildDir}\\lib\\zlib.lib"
				" ..",

			"cd gafferBuild && cmake --build . --config {cmakeBuildType} --target install -- -j {jobs}",
			"copy {buildDir}\\bin\\libpng*{sharedLibraryExtension}* {buildDir}\\lib\\",

		],

	},

}
