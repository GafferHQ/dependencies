{

	"downloads" : [

		"https://github.com/AcademySoftwareFoundation/openexr/archive/v2.4.1.tar.gz"

	],

	"license" : "LICENSE.md",

	"dependencies" : [ "Python", "Boost" ],

	"environment" : {

		"LD_LIBRARY_PATH" : "{buildDir}/lib",

	},

	"commands" : [

		"cmake"
			" -D CMAKE_INSTALL_PREFIX={buildDir}"
			# OpenEXR's CMake setup uses GNUInstallDirs, which unhelpfully
			# puts the libraries in `lib64`. Coax them back.
			" -D CMAKE_INSTALL_LIBDIR={buildDir}/lib"
			" -D CMAKE_PREFIX_PATH={buildDir}"
			" -D Boost_NO_SYSTEM_PATHS=TRUE"
			" -D Boost_NO_BOOST_CMAKE=TRUE"
			" -D BOOST_ROOT={buildDir}"
			" -D Python_ROOT_DIR={buildDir}"
			" -D Python2_ROOT_DIR={buildDir}"
			" -D Python3_ROOT_DIR={buildDir}"
			"."
		,

		"make VERBOSE=1 -j {jobs}",
		"make install",

		"mkdir -p {buildDir}/python",
		"mv {pythonLibDir}/python{pythonVersion}/site-packages/iex.so {buildDir}/python",
		"mv {pythonLibDir}/python{pythonVersion}/site-packages/imath.so {buildDir}/python",

	],

	"manifest" : [

		"bin/exrheader",
		"include/OpenEXR",
		"lib/libIlmImf*{sharedLibraryExtension}*",
		"lib/libIex*{sharedLibraryExtension}*",
		"lib/libHalf*{sharedLibraryExtension}*",
		"lib/libIlmThread*{sharedLibraryExtension}*",
		"lib/libImath*{sharedLibraryExtension}*",
		"lib/libPyIex*{sharedLibraryExtension}*",
		"lib/libPyImath*{sharedLibraryExtension}*",

		"python/iex.so",
		"python/imath.so",

	],

}
