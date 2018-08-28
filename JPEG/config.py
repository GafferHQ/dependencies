{

	"downloads" : [

		"https://download.sourceforge.net/libjpeg-turbo/libjpeg-turbo-2.0.0.tar.gz"

	],

	"license" : "LICENSE.md",

	"commands" : [

		"mkdir gafferBuild",
		"cd gafferBuild &&"
			" cmake"
			" -D CMAKE_INSTALL_PREFIX={buildDir}"
			" -D CMAKE_PREFIX_PATH={buildDir}"
			" ..",
		"cd gafferBuild && make install -j {jobs} VERBOSE=1"

	],

}
