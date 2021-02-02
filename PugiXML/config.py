{

	"downloads" : [

		"http://github.com/zeux/pugixml/releases/download/v1.11/pugixml-1.11.tar.gz"

	],

	"url" : "https://pugixml.org",

	"license" : "LICENSE.md",

	"commands" : [

		"mkdir gafferBuild",
		"cd gafferBuild &&"
			" cmake"
			" -D CMAKE_INSTALL_PREFIX={buildDir}"
			" ..",
		"cd gafferBuild && make install -j {jobs} VERBOSE=1",

	],

}
