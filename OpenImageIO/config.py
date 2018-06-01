{

	"downloads" : [

		"https://github.com/OpenImageIO/oiio/archive/Release-1.8.12.tar.gz"

	],

	"license" : "LICENSE",

	"commands" : [

		"mkdir gafferBuild",
		"cd gafferBuild &&"
		 	" cmake"
		 	" -D CMAKE_INSTALL_PREFIX=$BUILD_DIR"
		 	" -D CMAKE_INSTALL_LIBDIR=$BUILD_DIR/lib"
		 	" -D CMAKE_PREFIX_PATH=$BUILD_DIR"
		 	" -D USE_FFMPEG=NO"
		 	" ..",
		"cd gafferBuild && make install -j `getconf _NPROCESSORS_ONLN` VERBOSE=1"

	],

}
