{

	"downloads" : [

		"https://github.com/Blosc/c-blosc/archive/v1.14.4.tar.gz"

	],

	"license" : "LICENSES/BLOSC.txt",

	"commands" : [

		"cmake -E make_directory {buildDir}/doc/licenses/blosc && cmake -E copy LICENSES/* {buildDir}/doc/licenses/blosc/",
		"mkdir gafferBuild",
		"cd gafferBuild &&"
			" cmake"
			" -G \"Unix Makefiles\""
			" -D CMAKE_INSTALL_PREFIX={buildDir}"
			" ..",
		"cd gafferBuild && make install -j {jobs} VERBOSE=1"

	],

}
