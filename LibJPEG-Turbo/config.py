{

	"downloads" : [

		"https://download.sourceforge.net/project/libjpeg-turbo/1.5.2/libjpeg-turbo-1.5.2.tar.gz",

	],

	"license" : "LICENSE.md",

	"commands" : [

		"./configure --prefix={buildDir}",
		"make -j {jobs}",
		"make install",

	],

	"platform:windows" : {

		"commands" : [

			"mkdir gafferBuild",
			"cd gafferBuild && "
				" cmake"
				" -G {cmakeGenerator}"
				" -D CMAKE_BUILD_TYPE={cmakeBuildType}"
				" -D CMAKE_INSTALL_PREFIX={buildDir}"
				" -D WITH_SIMD=OFF"
				" ..",

			"cd gafferBuild && cmake --build . --config {cmakeBuildType} --target install -- -j {jobs}",

		],

	},

}
