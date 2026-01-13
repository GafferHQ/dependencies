{

	"downloads" : [

		"https://github.com/AcademySoftwareFoundation/OpenShadingLanguage/archive/refs/tags/v1.14.8.0.tar.gz"

	],

	"url" : "https://github.com/imageworks/OpenShadingLanguage",

	"license" : "LICENSE.md",

	"dependencies" : [ "OpenImageIO", "LLVM", "PugiXML", "Python" ],

	"environment" : {

		# Needed because the build process runs oslc, which
		# needs to link to the OIIO libraries.
		"DYLD_FALLBACK_LIBRARY_PATH" : "{buildDir}/lib",
		"LD_LIBRARY_PATH" : "{buildDir}/lib",
		"PATH" : "{buildDir}/bin:$PATH",
		# We define `OPTIX_ROOT_DIR` in the build container for
		# Cycles to find it, but OSL wants `OPTIX_INSTALL_DIR`.
		"OPTIX_INSTALL_DIR" : "$OPTIX_ROOT_DIR",

	},

	"commands" : [

		"mkdir gafferBuild",
		"cd gafferBuild &&"
			" cmake"
			" -D CMAKE_CXX_STANDARD={c++Standard}"
			" -D CMAKE_INSTALL_PREFIX={buildDir}"
			" -D CMAKE_INSTALL_LIBDIR={buildDir}/lib"
			" -D CMAKE_PREFIX_PATH={buildDir}"
			" -D STOP_ON_WARNING=0"
			" -D LLVM_STATIC=1"
			" -D USE_BATCHED={useBatched}"
			" -D OSL_SHADER_INSTALL_DIR={buildDir}/shaders"
			" -D OSL_BUILD_PLUGINS=0"
			" -D PYTHON_VERSION={pythonVersion}"
			" -D Python_ROOT_DIR={buildDir}"
			" -D Python_FIND_STRATEGY=LOCATION"
			" {extraArguments}"
			" ..",
		"cd gafferBuild && make install -j {jobs} VERBOSE=1",
		"{extraCommands}",

	],

	"variables" : {

		"extraArguments" : "",
		"extraCommands" : "",
		"useBatched" : "b8_AVX,b8_AVX2,b8_AVX2_noFMA,b8_AVX512,b8_AVX512_noFMA,b16_AVX512,b16_AVX512_noFMA",

	},

	"manifest" : [

		"bin/oslc",
		"bin/oslinfo",
		"bin/testshade",
		"include/OSL",
		"lib/libosl*",
		"lib/lib*oslexec*",
		"doc/osl*",
		"shaders",

	],

	"platform:linux" : {

		"variables" : {

			"extraArguments" : "-D OSL_USE_OPTIX=1",

		},
	},

	"platform:macos" : {

		"variables" : {

			"extraCommands" : "mv {buildDir}/lib/python{pythonVersion}/site-packages/oslquery {pythonLibDir}/python{pythonVersion}/site-packages/oslquery",
			"useBatched" : "0",

		},

	}

}
