{

	"downloads" : [

		"https://download.sourceforge.net/libpng/libpng-1.6.35.tar.gz"

	],

	"license" : "LICENSE",

	"commands" : [

		"mkdir gafferBuild",
		"cd gafferBuild &&"
			" cmake"
			" -G \"Unix Makefiles\""
			" -D CMAKE_INSTALL_PREFIX={buildDir}"
			" -D CMAKE_PREFIX_PATH={buildDir}"
			" ..",
		"cd gafferBuild && make install -j {jobs} VERBOSE=1"

	],

}
