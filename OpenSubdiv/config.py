{

	"downloads" : [

		"https://github.com/PixarAnimationStudios/OpenSubdiv/archive/v3_4_4.tar.gz"

	],

	"url" : "http://graphics.pixar.com/opensubdiv",

	"license" : "LICENSE.txt",

	"commands" : [

		"cmake"
			" -D CMAKE_INSTALL_PREFIX={buildDir}"
			" -D CMAKE_PREFIX_PATH={buildDir}"
			" -D NO_DOC=1"
			" -D NO_OMP=1"
			" -D NO_CUDA=1"
			" -D NO_OPENCL=1"
			" -D NO_GLEW=1"
			" -D NO_GLFW=1"
			" -D NO_DX=1"
			" -D NO_TESTS=1"
			" -D NO_TBB=1"
			" -D OPENEXR_LOCATION={buildDir}/lib"
			" ."
		,

		"make VERBOSE=1 -j {jobs}",
		"make install",

	],

	"manifest" : [

		"include/opensubdiv",
		"lib/libosd*{sharedLibraryExtension}*",

	],

}
