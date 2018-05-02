{

	"downloads" : [

		"http://apache.mirror.anlx.net//xerces/c/3/sources/xerces-c-3.2.1.tar.gz"

	],

	"license" : "LICENSE",

	"commands" : [

		"mkdir gafferBuild",
		"cd gafferBuild &&"
		 	" cmake"
		 	" -G $CMAKE_GENERATOR"
		 	" -D CMAKE_BUILD_TYPE=$CMAKE_BUILD_TYPE"
		 	" -D CMAKE_INSTALL_PREFIX=$BUILD_DIR"
		 	" ..",
		"cd gafferBuild && cmake --build . --config $CMAKE_BUILD_TYPE --target install -- -j $NUM_PROCESSORS"
	],

}
