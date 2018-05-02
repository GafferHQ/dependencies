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
			" -D CMAKE_CXX_STANDARD={c++Standard}"
			" -D CMAKE_INSTALL_PREFIX={buildDir}"
			" -D CMAKE_INSTALL_LIBDIR={buildDir}/lib"
			" -D CMAKE_PREFIX_PATH={buildDir}"
			" ..",
		"cd build && make install -j {jobs} VERBOSE=1",
	],

	"manifest" : [

		"include/tbb",
		"include/oneapi",
		"lib/libtbb*{sharedLibraryExtension}*",

	],

	"platform:windows" : {

		"commands" : [

			"mkdir gafferBuild",
			"cd gafferBuild && "
				" cmake"
				" -G {cmakeGenerator}"
				" -D CMAKE_CXX_STANDARD={c++Standard}"
				" -D CMAKE_BUILD_TYPE={cmakeBuildType}"
				" -D CMAKE_PREFIX_PATH={buildDir}"
				" -D CMAKE_INSTALL_PREFIX={buildDir}"
				" -D TBB_BUILD_TESTS=OFF"
				" ..",

			"cd gafferBuild && cmake --build . --config {cmakeBuildType} --target install -- -j {jobs}",

		],

	},

}
