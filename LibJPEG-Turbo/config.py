{

	"downloads" : [

		"https://github.com/libjpeg-turbo/libjpeg-turbo/releases/download/3.0.3/libjpeg-turbo-3.0.3.tar.gz",

	],

	"url" : "https://libjpeg-turbo.org",
	"license" : "LICENSE.md",

	"commands" : [

		"mkdir build",
		"cd build &&"
			" cmake"
			" -D CMAKE_INSTALL_PREFIX={buildDir}"
			" -D CMAKE_INSTALL_LIBDIR={buildDir}/lib"
			" ..",
		"cd build && make -j {jobs} && make install",

	],

	"manifest" : [

		"include/jconfig.h",
		"include/jerror.h",
		"include/jmorecfg.h",
		"include/jpeglib.h",

		"lib/libjpeg*{sharedLibraryExtension}*",

	],

}
