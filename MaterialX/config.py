{

	"downloads" : [

		"https://github.com/AcademySoftwareFoundation/MaterialX/archive/refs/tags/v1.38.10.tar.gz",

	],

	"url" : "http://www.materialx.org/",

	"license" : "LICENSE",

	"dependencies" : [ "Python", "OpenImageIO", "OpenShadingLanguage" ],

	"environment" : {

		"PATH" : "{buildDir}/bin:$PATH",
		"LD_LIBRARY_PATH" : "{buildDir}/lib:$LD_LIBRARY_PATH",

	},

	"commands" : [

		"mkdir build",
		"cd build && cmake"
			" -D CMAKE_CXX_STANDARD={c++Standard}"
			" -D CMAKE_INSTALL_PREFIX={buildDir}"
			" -D MATERIALX_BUILD_SHARED_LIBS=ON"
			" -D MATERIALX_BUILD_PYTHON=ON"
			" -D MATERIALX_BUILD_OIIO=ON"
			" -D MATERIALX_BUILD_TESTS=OFF"
			" -D MATERIALX_INSTALL_LIB_PATH={buildDir}/lib"
			" -D MATERIALX_INSTALL_STDLIB_PATH={buildDir}/materialX/libraries"
			" -D MATERIALX_PYTHON_EXECUTABLE={buildDir}/bin/python"
			" ..",

		"cd build && make clean && make VERBOSE=1 -j {jobs} && make install",

	],

	"manifest" : [

		"include/MaterialX*",
		"{sharedLibraryDir}/{libraryPrefix}MaterialX*{sharedLibraryExtension}*",
		"lib/{libraryPrefix}MaterialX*{sharedLibraryExtension}*",
		"materialX",
		"python/MaterialX",

	],

	"platform:windows" : {

		"manifest" : [

			"include/MaterialX*",
			"{sharedLibraryDir}/{libraryPrefix}MaterialX*{sharedLibraryExtension}*",
			"lib/{libraryPrefix}MaterialX*{staticLibraryExtension}*",
			"materialX",
			"python/MaterialX",

		],

		"commands" : [

			"mkdir build",
			"cd build && cmake"
				" -G {cmakeGenerator}"
                " -D CMAKE_BUILD_TYPE={cmakeBuildType}"
				" -D CMAKE_CXX_STANDARD={c++Standard}"
				" -D CMAKE_INSTALL_PREFIX={buildDir}"
                " -D CMAKE_PREFIX_PATH={buildDir}"
				" -D MATERIALX_BUILD_SHARED_LIBS=ON"
				" -D MATERIALX_BUILD_PYTHON=ON"
				" -D MATERIALX_BUILD_OIIO=ON"
				" -D MATERIALX_BUILD_TESTS=OFF"
				" -D MATERIALX_INSTALL_LIB_PATH={buildDir}/lib"
				" -D MATERIALX_INSTALL_STDLIB_PATH={buildDir}/materialX/libraries"
				" -D MATERIALX_PYTHON_EXECUTABLE={buildDir}/bin/python"
				" -D CMAKE_VERBOSE_MAKEFILE:BOOL=ON"
				" ..",

			"cd build && cmake --build . --config {cmakeBuildType} --target install -- -j {jobs}",

		]

	}

}
