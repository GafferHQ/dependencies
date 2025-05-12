{

	"downloads" : [

		"https://github.com/PixarAnimationStudios/OpenSubdiv/archive/v3_6_0.tar.gz"

	],

	"url" : "http://graphics.pixar.com/opensubdiv",

	"license" : "LICENSE.txt",

	"commands" : [

		"mkdir build",
		"cd build && cmake"
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
			" .."
		,

		"cd build && make VERBOSE=1 -j {jobs} && make install",

	],

	"manifest" : [

		"include/opensubdiv",
		"lib/libosd*{sharedLibraryExtension}*",

	],

}
