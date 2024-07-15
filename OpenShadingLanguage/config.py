{

	"downloads" : [

		"https://github.com/AcademySoftwareFoundation/OpenShadingLanguage/archive/refs/tags/v1.12.14.0.tar.gz"

	],

	"url" : "https://github.com/imageworks/OpenShadingLanguage",

	"license" : "LICENSE.md",

	"dependencies" : [ "OpenImageIO", "LLVM", "PugiXML", "Python", "Partio" ],

	"environment" : {

		# Needed because the build process runs oslc, which
		# needs to link to the OIIO libraries.
		"DYLD_FALLBACK_LIBRARY_PATH" : "{buildDir}/lib",
		"LD_LIBRARY_PATH" : "{buildDir}/lib",
		"PATH" : "{buildDir}/bin:$PATH",

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
			" -D ENABLERTTI=1"
			" -D LLVM_STATIC=1"
			" -D USE_BATCHED={useBatched}"
			" -D OSL_SHADER_INSTALL_DIR={buildDir}/shaders"
			" -D Python_ROOT_DIR={buildDir}"
			" -D Python_FIND_STRATEGY=LOCATION"
			" ..",
		"cd gafferBuild && make install -j {jobs} VERBOSE=1",
		"cp {buildDir}/share/doc/OSL/osl-languagespec.pdf {buildDir}/doc",
		"{extraCommands}",

	],

	"variables" : {

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
		"lib/osl.imageio.*",
		"doc/osl*",
		"shaders",

	],

	"platform:macos" : {

		"variables" : {

			"extraCommands" : "mv {buildDir}/lib/python{pythonVersion}/site-packages/oslquery.so {pythonLibDir}/python{pythonVersion}/site-packages/oslquery.so",
			"useBatched" : "0",

		},

	},

	"arch:aarch64" : {

		"variables" : {
			"useBatched" : "0",
		}

	},

}
