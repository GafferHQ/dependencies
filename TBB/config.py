{

	"downloads" : [

		"https://github.com/uxlfoundation/oneTBB/archive/refs/tags/v2021.13.0.tar.gz"

	],

	"url" : "http://threadingbuildingblocks.org/",

	"license" : "LICENSE.txt",

	"commands" : [

		"mkdir build",
		"cd build &&"
			" cmake"
			" -G {cmakeGenerator}"
			" -D CMAKE_BUILD_TYPE={cmakeBuildType}"
			" -D CMAKE_CXX_STANDARD={c++Standard}"
			" -D CMAKE_INSTALL_PREFIX={buildDir}"
			" -D CMAKE_INSTALL_LIBDIR={buildDir}/lib"
			" -D CMAKE_PREFIX_PATH={buildDir}"
			" ..",
		"cd build && cmake --build . --config {cmakeBuildType} --target install -- -j {jobs}",
	],

	"manifest" : [

		"include/tbb",
		"include/oneapi",
		"{sharedLibraryDir}/{libraryPrefix}tbb*{sharedLibraryExtension}*",
		"lib/{libraryPrefix}tbb*.lib",

	],

	"platform:windows" : {

		"variables" : {

			"installLibsCommand" : "",

		},

	},

}
