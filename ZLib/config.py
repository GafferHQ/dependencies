{

	"downloads" : [

		"https://github.com/madler/zlib/releases/download/v1.2.13/zlib-1.2.13.tar.gz",

	],

	"url" : "https://zlib.net/",

	"license" : "LICENSE",

	"commands" : [

		"mkdir build",
		"cd build &&"
			" cmake"
			" -G {cmakeGenerator}"
			" -D CMAKE_INSTALL_PREFIX={buildDir}"
			" -D CMAKE_INSTALL_LIBDIR={buildDir}/lib"
			" -D CMAKE_BUILD_TYPE=Release"
			" ..",
		"cd build && cmake --build . --config {cmakeBuildType} --target install -- -j {jobs}",

	],

	"manifest" : [

		"include/zlib.h",
		"include/zconf.h",
		"{sharedLibraryDir}/{libraryPrefix}z*{sharedLibraryExtension}*",
		"lib/{libraryPrefix}z*.lib"

	],

}
