{

	"downloads" : [

		"https://github.com/01org/tbb/archive/2017_U6.tar.gz"

	],

	"license" : "LICENSE",

	"commands" : [

		"mkdir gafferBuild",
		"cd gafferBuild &&"
			" cmake"
			" -G \"Unix Makefiles\""
			" -D CMAKE_BUILD_TYPE=Release"
			" -D CMAKE_INSTALL_PREFIX={buildDir}"
			" -D CMAKE_PREFIX_PATH={buildDir}"
			" -D TBB_BUILD_TESTS=OFF"
			" ..",
		"cd gafferBuild && cmake --build . --config %BUILD_TYPE% --target install -- -j {jobs} VERBOSE=1"

	],

}
