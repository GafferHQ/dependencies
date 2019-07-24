{

	"downloads" : [

		"https://github.com/AcademySoftwareFoundation/openexr/archive/refs/tags/v3.1.13.tar.gz"

	],

	"url" : "http://www.openexr.com",

	"license" : "LICENSE.md",

	"dependencies" : [ "Python", "Boost", "Imath" ],

	"environment" : {

		"PATH" : "{buildDir}/bin:$PATH",
		"LD_LIBRARY_PATH" : "{buildDir}/lib",

	},

	"commands" : [

		"cmake"
			" -D CMAKE_INSTALL_PREFIX={buildDir}"
			# OpenEXR's CMake setup uses GNUInstallDirs, which unhelpfully
			# puts the libraries in `lib64`. Coax them back.
			" -D CMAKE_INSTALL_LIBDIR={buildDir}/lib"
			" -D CMAKE_PREFIX_PATH={buildDir}"
			" ."
		,

		"make VERBOSE=1 -j {jobs}",
		"make install",

	],

	"manifest" : [

		"bin/exrheader.*",
		"include/OpenEXR",
		"{sharedLibraryDir}/{libraryPrefix}Iex*{sharedLibraryExtension}*",
        "lib/{libraryPrefix}Iex*.lib",
		"{sharedLibraryDir}/{libraryPrefix}IlmThread*{sharedLibraryExtension}*",
		"lib/{libraryPrefix}IlmThread*.lib",
		"{sharedLibraryDir}/{libraryPrefix}OpenEXR*{sharedLibraryExtension}*",
		"lib/{libraryPrefix}OpenEXR*.lib",

	],

	"platform:windows" : {
        
		"environment" : {

			"PATH" : "{buildDir}\\bin;%PATH%",

		},

		"variables" : {

			"cmakeGenerator" : "\"Visual Studio 17 2022\"",

		},

		"dependencies" : [ "Python", "Boost", "Imath", "ZLib" ],

		"commands" : [
			"mkdir gafferBuild",
			"cd gafferBuild && cmake"
				" -D CMAKE_INSTALL_PREFIX={buildDir}"
				" -D CMAKE_BUILD_TYPE={cmakeBuildType}"
				" -G {cmakeGenerator}"
				" -D CMAKE_PREFIX_PATH={buildDir}"
				" -D OPENEXR_LIB_SUFFIX="
				" -D ILMBASE_LIB_SUFFIX="
				" -D PYILMBASE_LIB_SUFFIX="
				" -D Boost_NO_SYSTEM_PATHS=TRUE"
				" -D Boost_NO_BOOST_CMAKE=TRUE"
				" -D BOOST_ROOT={buildDir}"
				" -D Python3_ROOT_DIR={buildDir}"
				" -D Python_NO_SYSTEM_PATHS=TRUE"
				" -D Python3_FIND_STRATEGY=LOCATION"
				" -D CMAKE_CXX_FLAGS=\"-DBOOST_ALL_NO_LIB\""
				" -D ZLIB_ROOT={buildDir}"
				" ..",
			"cd gafferBuild && cmake --build . --config {cmakeBuildType} --target install",
		]
	},

}
