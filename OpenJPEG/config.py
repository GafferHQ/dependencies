{

	"downloads" : [

		"https://github.com/uclouvain/openjpeg/archive/refs/tags/v2.4.0.tar.gz"

	],

	"url" : "https://www.openjpeg.org/",

	"license" : "LICENSE",

	"commands" : [

		"mkdir build",
		"cd build &&"
			" cmake"
			" -D CMAKE_INSTALL_PREFIX={buildDir}"
			" -D CMAKE_PREFIX_PATH={buildDir}"
			" ..",
		"cd build && make install -j {jobs} VERBOSE=1",

	],

	"manifest" : [

		"include/openjpeg-2.4",
		"lib/libopenjp2*{sharedLibraryExtension}*",

	],

}
