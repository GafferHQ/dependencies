{

	"downloads" : [

		"https://github.com/madler/zlib/archive/v1.2.11.tar.gz"

	],

	"commands" : [

		"mkdir gafferBuild",

		"cd gafferBuild &&"
			" cmake"
			" -G {cmakeGenerator}"
			" -D CMAKE_BUILD_TYPE={cmakeBuildType}"
			" -D CMAKE_INSTALL_PREFIX={buildDir}"
			" ..",

		"cd gafferBuild && cmake --build . --config {cmakeBuildType} --target install -- -j {jobs}"

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
			# for some reason Zlib building doesn't put zconf.h in the main source directory alongside zlib.h
			# manually copy it there so Boost can find it
			"cd gafferBuild && copy zconf.h ..",

		]

	}

}
