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

	},

	"variables" : {

		# Make sure we pick up the python headers from {buildDir},
		# rather than any system level headers. We refer to this
		# variable in "commands" below.
		"pythonIncludeDir" : "{buildDir}/include/python2.7",

	},

	"commands" : [

		"mkdir build",

		"cd build &&"
			" cmake"
			" -G {cmakeGenerator}"
			" -D CMAKE_BUILD_TYPE={cmakeBuildType}"
			" -D WITH_CLI=ON"
			" -D WITH_STUDIO=OFF"
			" -D WITH_TOOLS=OFF"
			" -D WITH_TESTS=OFF"
			" -D WITH_PYTHON=ON"
			" -D USE_STATIC_BOOST=OFF"
			" -D USE_STATIC_EXR=OFF"
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
			" -D PYTHON_INCLUDE_DIR={pythonIncludeDir}"
			" ..",
		"cd build && cmake --build . --config {cmakeBuildType} --target install -- -j {jobs}"

	],

	"platform:osx" : {

		"variables" : {

			# Python headers have a different location on OSX.
			"pythonIncludeDir" : "{buildDir}/lib/Python.framework/Headers",

		},

	},

	"platform:windows" : {

		"variables" : {
			"cmakeGenerator" : "\"Visual Studio 15 2017 Win64\"",
		},

		"environment" : {

			"PATH" : "%PATH%;{buildDir}\\lib;{buildDir}\\bin",

		},

		"commands" : [
			"if not exist \"build\" mkdir build",
			"cd build &&"
				" cmake"
				" -G {cmakeGenerator}"
				" -D CMAKE_VERBOSE_MAKEFILE:BOOL=ON"
				" -D CMAKE_BUILD_TYPE={cmakeBuildType}"
				" -D PYTHON_LIBRARY={buildDir}\\lib\\python27.lib"
				" -D PYTHON_INCLUDE_DIR={buildDir}\\include"
				" -D WITH_OSL=ON"
				" -D WITH_CLI=ON"
				" -D WITH_STUDIO=OFF"
				" -D WITH_TOOLS=OFF"
				" -D WITH_PYTHON=ON"
				" -D WITH_OSL=ON"
				" -D WITH_TESTS=OFF"
				" -D USE_STATIC_BOOST=OFF"
				" -D USE_STATIC_OIIO=OFF"
				" -D USE_STATIC_EXR=OFF"
				" -D USE_STATIC_OSL=OFF"
				" -D USE_EXTERNAL_ZLIB=ON"
				" -D USE_EXTERNAL_EXR=ON"
				" -D USE_EXTERNAL_PNG=ON"
				" -D USE_EXTERNAL_XERCES=ON"
				" -D USE_EXTERNAL_OSL=ON"
				" -D USE_EXTERNAL_OIIO=ON"
				" -D USE_EXTERNAL_ALEMBIC=ON"
				" -D WARNINGS_AS_ERRORS=OFF"
				" -D USE_SSE=ON"
				" -D CMAKE_PREFIX_PATH={buildDir}"
				" -D CMAKE_INSTALL_PREFIX={buildDir}\\appleseed"
				" -D BOOST_ROOT={buildDir}"
				" -D IMATH_INCLUDE_DIR={buildDir}\\include"
				" -D IMATH_HALF_LIBRARY={buildDir}\\lib\\Half.lib"
				" -D IMATH_IEX_LIBRARY={buildDir}\\lib\\Iex-2_2.lib"
				" -D IMATH_MATH_LIBRARY={buildDir}\\lib\\Imath-2_2.lib"
				" -D OPENEXR_INCLUDE_DIR={buildDir}\\include"
				" -D OPENEXR_IMF_LIBRARY={buildDir}\\lib\\IlmImf-2_2.lib"
				" -D OPENEXR_THREADS_LIBRARY={buildDir}\\lib\\IlmThread-2_2.lib"
				" -D XERCES_LIBRARY={buildDir}\\lib\\xerces-c_3.lib"
				" -D OSL_INCLUDE_DIR={buildDir}\\include"
				" -D LLVM_LIBS_DIR={buildDir}\\lib"
				" OSL_EXEC_LIBRARY={buildDir}\\lib\\oslexec.lib"
				" OSL_COMP_LIBRARY={buildDir}\\lib\\oslcomp.lib"
				" OSL_QUERY_LIBRARY={buildDir}\\lib\\oslquery.lib"
				" ..",
			"cd build && cmake --build . --config {cmakeBuildType} --target install",
		]

	}

}
