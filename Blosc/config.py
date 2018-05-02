{

	"downloads" : [

		"https://github.com/Blosc/c-blosc/archive/refs/tags/v1.21.1.tar.gz"

	],

	"url" : "https://blosc.org",

	"license" : "LICENSES",

	"commands" : [

		"cmake -DCMAKE_INSTALL_PREFIX={buildDir} .",
		# Note : Blosc does not declare its build dependencies
		# correctly, so we cannot do a parallel build with `-j`.
		"make install VERBOSE=1",

	],

	"manifest" : [

		"include/blosc*.h",
		"{sharedLibraryDir}/{libraryPrefix}blosc*{sharedLibraryExtension}*",
		"lib/{libraryPrefix}blosc*.lib",

	],
	"platform:windows" : {

		"commands" : [

			"mkdir gafferBuild",
			"cd gafferBuild && "
				" cmake"
				" -Wno-dev"
				" -G {cmakeGenerator}"
				" -D CMAKE_BUILD_TYPE={cmakeBuildType}"
				" -D CMAKE_INSTALL_PREFIX={buildDir}"
				" -D BUILD_TESTS=OFF"
				" -D BUILD_BENCHMARKS=OFF"
				" -D BUILD_STATIC=OFF"
				" ..",

			"cd gafferBuild && cmake --build . --config {cmakeBuildType} --target install -- -j {jobs}",

		],

	},

}
