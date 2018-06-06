{

	"downloads" : [

		"https://github.com/appleseedhq/appleseed/archive/1.9.0-beta.tar.gz"

	],

	"license" : "LICENSE.txt",

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

		# Make sure we pick up the python headers from {buildDir},
		# rather than any system level headers.
		"PYTHON_INCLUDE_DIR" : "{buildDir}/lib/Python.framework/Headers" if platform == "osx" else "{buildDir}/include/python2.7"

	},

	"commands" : [

		"mkdir build",

		"cd build &&"
			" cmake"
			" -D WITH_CLI=ON"
			" -D WITH_STUDIO=OFF"
			" -D WITH_TOOLS=OFF"
			" -D WITH_TESTS=OFF"
			" -D WITH_PYTHON=ON"
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
			" -D PYTHON_INCLUDE_DIR=$PYTHON_INCLUDE_DIR"
			" ..",

		"cd build && make install -j {jobs} VERBOSE=1"

	],

}
