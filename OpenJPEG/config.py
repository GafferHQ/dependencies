{

	"downloads" : [

		"https://github.com/uclouvain/openjpeg/archive/refs/tags/v2.5.4.tar.gz"

	],

	"url" : "https://www.openjpeg.org/",

	"license" : "LICENSE",

	"commands" : [

		"mkdir build",
		"cd build &&"
			" cmake"
			" -D CMAKE_INSTALL_PREFIX={buildDir}"
			" -D CMAKE_PREFIX_PATH={buildDir}"
			# OpenJPEG's CMake setup uses GNUInstallDirs, which unhelpfully
			# puts the libraries in `lib64`. Coax them back.
			" -D CMAKE_INSTALL_LIBDIR={buildDir}/lib"
			" ..",
		"cd build && make install -j {jobs} VERBOSE=1",

	],

	"manifest" : [

		"include/openjpeg-2.5",
		"lib/libopenjp2*{sharedLibraryExtension}*",

	],

}
