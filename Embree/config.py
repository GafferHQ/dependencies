{

	"downloads" : [

		"https://github.com/embree/embree/archive/v4.3.1.tar.gz"

	],

	"url" : "https://www.embree.org/",

	"license" : "LICENSE.txt",

	"dependencies" : [ "TBB" ],

	"commands" : [

		"mkdir gafferBuild",
		"cd gafferBuild &&"
			" cmake"
			" -D CMAKE_INSTALL_PREFIX={buildDir}"
			" -D CMAKE_PREFIX_PATH={buildDir}"
			" -D CMAKE_INSTALL_LIBDIR={buildDir}/lib"
			" -D EMBREE_TBB_ROOT={buildDir}"
			" -D EMBREE_STATIC_LIB=OFF"
			" -D EMBREE_ISPC_SUPPORT=OFF"
			" -D EMBREE_TUTORIALS=OFF"
			" -D EMBREE_RAY_MASK=ON"
			" -D EMBREE_FILTER_FUNCTION=ON"
			" -D EMBREE_BACKFACE_CULLING=OFF"
			" -D EMBREE_BACKFACE_CULLING_CURVES=ON"
			" -D EMBREE_BACKFACE_CULLING_SPHERES=ON"
			" -D EMBREE_NO_SPLASH=ON"
			" -D EMBREE_TASKING_SYSTEM=TBB"
			" ..",
		"cd gafferBuild && make install -j {jobs} VERBOSE=1",

	],

	"manifest" : [

		"include/embree4",

		"lib/libembree4*{sharedLibraryExtension}*",

	],

}
