{

	"downloads" : [

		"https://github.com/imageworks/OpenShadingLanguage/archive/Release-1.9.9.tar.gz"

	],

	"license" : "LICENSE",

	"environment" : {

		# Needed because the build process runs oslc, which
		# needs to link to the OIIO libraries.
		"DYLD_FALLBACK_LIBRARY_PATH" : "$BUILD_DIR/lib",
		"LD_LIBRARY_PATH" : "$BUILD_DIR/lib",

	},

	"commands" : [

		"mkdir gafferBuild",
		"cd gafferBuild &&"
		 	" cmake"
		 	" -D CMAKE_INSTALL_PREFIX=$BUILD_DIR"
		 	" -D CMAKE_INSTALL_LIBDIR=$BUILD_DIR/lib"
		 	" -D CMAKE_PREFIX_PATH=$BUILD_DIR"
		 	" -D STOP_ON_WARNING=0"
		 	" -D ENABLERTTI=1"
		 	" -D LLVM_STATIC=1"
		 	" ..",
		"cd gafferBuild && make install -j `getconf _NPROCESSORS_ONLN` VERBOSE=1"

	],

}
