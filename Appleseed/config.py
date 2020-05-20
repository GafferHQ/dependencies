{

	"downloads" : [

		"https://github.com/appleseedhq/appleseed/archive/2.1.0-beta.tar.gz"

	],

	"url" : "https://appleseedhq.net",

	"license" : "LICENSE.txt",

	"dependencies" : [ "Python", "Xerces", "OpenShadingLanguage", "OpenImageIO", "Boost", "LibPNG", "OpenEXR", "LZ4" ],

	"environment" : {

		# Needed so that `oslc` can be run to compile
		# shaders during the build.
		"DYLD_FALLBACK_LIBRARY_PATH" : "{buildDir}/lib",
		"LD_LIBRARY_PATH" : "{buildDir}/lib",

		# Appleseed embeds minizip, which appears to require a later version
		# of zlib than CentOS 6 provides. These defines disable encryption,
		# which isn't needed anyway, and fixes the problem.
		# See https://github.com/appleseedhq/appleseed/issues/1597.
		"CFLAGS" : "-DNOCRYPT -DNOUNCRYPT",

	},

	"commands" : [

		"mkdir build",

		"cd build &&"
			" cmake"
			" -D WITH_CLI=ON"
			" -D WITH_STUDIO=OFF"
			" -D WITH_TOOLS=OFF"
			" -D WITH_TESTS=OFF"
			" -D WITH_SAMPLES=OFF"
			" -D WITH_DOXYGEN=OFF"
			" {pythonArguments}"
			" -D USE_STATIC_BOOST=OFF"
			" -D USE_STATIC_OIIO=OFF"
			" -D USE_STATIC_OSL=OFF"
			" -D USE_EXTERNAL_ZLIB=ON"
			" -D USE_EXTERNAL_EXR=ON"
			" -D USE_EXTERNAL_PNG=ON"
			" -D USE_EXTERNAL_XERCES=ON"
			" -D USE_EXTERNAL_OSL=ON"
			" -D USE_EXTERNAL_OIIO=ON"
			" -D USE_SSE=ON"
			" -D WARNINGS_AS_ERRORS=OFF"
			" -D CMAKE_PREFIX_PATH={buildDir}"
			" -D CMAKE_INSTALL_PREFIX={buildDir}/appleseed"
			" -D CMAKE_LIBRARY_PATH={pythonLibDir}"
			" ..",

		"cd build && make install -j {jobs} VERBOSE=1"

	],

	"variant:Python:2" : {

		"variables" : {

			"pythonArguments" :
				" -D WITH_PYTHON2_BINDINGS=ON"
				" -D WITH_PYTHON3_BINDINGS=OFF"
				" -D PYTHON_INCLUDE_DIR={pythonIncludeDir}"
				" -D Boost_PYTHON_LIBRARY={buildDir}/lib/libboost_python{pythonMajorVersion}{pythonMinorVersion}{sharedLibraryExtension}"
			,

		},

	},

	"variant:Python:3" : {

		"variables" : {

			"pythonArguments" :
				" -D WITH_PYTHON2_BINDINGS=OFF"
				" -D WITH_PYTHON3_BINDINGS=ON"
				" -D PYTHON3_INCLUDE_DIR={pythonIncludeDir}"
				" -D PYTHON_MAJOR_VERSION={pythonMajorVersion}"
				" -D PYTHON_MINOR_VERSION={pythonMinorVersion}"
				" -D Boost_PYTHON3_LIBRARY={buildDir}/lib/libboost_python{pythonMajorVersion}{pythonMinorVersion}{sharedLibraryExtension}"
			,

		},

	},

	"manifest" : [

		"appleseed/bin/appleseed.cli",
		"appleseed/include",
		"appleseed/lib",
		"appleseed/samples",
		"appleseed/schemas",
		"appleseed/settings",
		"appleseed/shaders",

	],

}
