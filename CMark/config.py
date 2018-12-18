{

	"downloads" : [

		"https://github.com/github/cmark-gfm/archive/refs/tags/0.29.0.gfm.2.tar.gz",

	],

	"url" : "https://github.com/github/cmark",

	"license" : "COPYING",

	"commands" : [

		"mkdir build",
		"cd build && cmake -D CMAKE_INSTALL_PREFIX={buildDir} ..",
		"cd build && make -j {jobs} && make install",

	],

	"manifest" : [

		"lib/{libraryPrefix}cmark*{sharedLibraryExtension}*",
		"lib/{libraryPrefix}cmark*.lib",

	],

	"platform:windows" : {

		"commands" : [

			"mkdir gafferBuild",
			"cd gafferBuild && "
				" cmake"
				" -G {cmakeGenerator}"
				" -D CMAKE_BUILD_TYPE={cmakeBuildType}"
				" -D CMAKE_INSTALL_PREFIX={buildDir}"
				" ..",

			"cd gafferBuild && cmake --build . --config {cmakeBuildType} --target install -- -j {jobs}",
			"copy {buildDir}\\bin\\{libraryPrefix}cmark*{sharedLibraryExtension}* {buildDir}\\lib\\",

		],

	}
	
}
