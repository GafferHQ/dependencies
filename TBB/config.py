{

	"downloads" : [

		"https://github.com/01org/tbb/archive/2017_U6.tar.gz"

	],

	"license" : "LICENSE",

	"commands" : [

		"make -j {jobs} stdver=c++11",
		"cp -r include/tbb {buildDir}/include",
		"{installLibsCommand}",

	],

	"platform:linux" : {

		"environment" : {

			"tbb_os" : "linux",

		},

		"variables" : {

			"installLibsCommand" : "cp build/*_release/*.so* {buildDir}/lib",

		},

	},

	"platform:osx" : {

		"environment" : {

			"tbb_os" : "macos",

		},

		"variables" : {

			"installLibsCommand" : "cp build/macos_*_release/*.dylib {buildDir}/lib",

		},

	},

	"platform:windows" : {

		"commands" : [

			"mkdir gafferBuild",
			"cd gafferBuild && "
				" cmake"
				" -G {cmakeGenerator}"
				" -D CMAKE_BUILD_TYPE={cmakeBuildType}"
				" -D CMAKE_PREFIX_PATH={buildDir}"
				" -D CMAKE_INSTALL_PREFIX={buildDir}"
				" -D TBB_BUILD_TESTS=OFF"
				" ..",

			"cd gafferBuild && cmake --build . --config {cmakeBuildType} --target install -- -j {jobs}",

		],

	},

}
