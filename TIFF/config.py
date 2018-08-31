{

	"downloads" : [

		"https://download.osgeo.org/libtiff/tiff-4.0.9.tar.gz",

	],

	"license" : "COPYRIGHT",

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
