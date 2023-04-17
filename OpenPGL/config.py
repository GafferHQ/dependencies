{

	"downloads" : [

		"https://github.com/OpenPathGuidingLibrary/openpgl/archive/refs/tags/v0.6.0.tar.gz"

	],

	"url" : "https://github.com/OpenPathGuidingLibrary/openpgl",

	"license" : "LICENSE.txt",

	"dependencies" : [ "TBB" ],

	"commands" : [

		"mkdir gafferBuild",
		"cd gafferBuild &&"
			" cmake"
			" -D CMAKE_CXX_STANDARD={c++Standard}"
			" -D CMAKE_INSTALL_PREFIX={buildDir}"
			" -D CMAKE_INSTALL_LIBDIR={buildDir}/lib"
			" -D CMAKE_PREFIX_PATH={buildDir}"
			" -D OPENPGL_BUILD_STATIC=ON"
			" ..",
		"cd gafferBuild && make install -j {jobs}",

	],

	"manifest" : [

		"include/openpgl",
		"lib/{libraryPrefix}openpgl*",

	],
    
	"platform:windows" : {

		"commands" : [

			"mkdir gafferBuild",
			"cd gafferBuild &&"
				" cmake"
				" -G {cmakeGenerator}"
                " -D CMAKE_BUILD_TYPE={cmakeBuildType}"
				" -D CMAKE_CXX_STANDARD={c++Standard}"
				" -D CMAKE_INSTALL_PREFIX={buildDir}"
				" -D CMAKE_INSTALL_LIBDIR={buildDir}/lib"
				" -D CMAKE_PREFIX_PATH={buildDir}"
				" -D OPENPGL_BUILD_STATIC=ON"
				" ..",
			"cd gafferBuild && cmake --build . --config {cmakeBuildType} --target install -- -j {jobs}",

		],

	},

}
