{

	"downloads" : [

		"https://github.com/AcademySoftwareFoundation/OpenShadingLanguage/archive/refs/tags/v1.13.11.0.tar.gz"

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
			" -D Python_ROOT_DIR={buildDir}"
			" -D Python_FIND_STRATEGY=LOCATION"
			" {extraArguments}"
			" ..",
		"cd gafferBuild && make install -j {jobs} VERBOSE=1",
		"cp {buildDir}/share/doc/OSL/osl-languagespec.pdf {buildDir}/doc",
		"{extraCommands}",

	],

	"variables" : {

		"extraArguments" : "",
		"extraCommands" : "",
		"useBatched" : "b8_AVX,b8_AVX2,b8_AVX2_noFMA,b8_AVX512,b8_AVX512_noFMA,b16_AVX512,b16_AVX512_noFMA",

	},

	"manifest" : [

		"bin/oslc{executableExtension}",
		"bin/oslinfo{executableExtension}",
		"bin/testshade{executableExtension}",
		"include/OSL",
		"lib/{libraryPrefix}osl*",
		"lib/{libraryPrefix}*oslexec*",
		"lib/osl.imageio.*",
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

			"extraCommands" : "mv {buildDir}/lib/python{pythonVersion}/site-packages/oslquery.so {pythonLibDir}/python{pythonVersion}/site-packages/oslquery.so",
			"useBatched" : "0",

		},

	},

	"platform:windows" : {

		"variables" : {

			"version" : "1.13.11.0",

		},

		"manifest" : [

			"bin/oslc{executableExtension}",
			"bin/oslinfo{executableExtension}",
			"include/OSL",
			"{sharedLibraryDir}/{libraryPrefix}osl*{sharedLibraryExtension}",
			"lib/{libraryPrefix}osl*.lib",
			"{sharedLibraryDir}/{libraryPrefix}*oslexec*{sharedLibraryExtension}",
			"lib/{libraryPrefix}*oslexec*.lib",
			"python/oslquery.pyd",
			"doc/osl*",
			"shaders",

		],

		"environment" : {
			"PATH" : "{buildDir}\\lib;{buildDir}\\bin;%ROOT_DIR%\\OpenShadingLanguage\\working\\OpenShadingLanguage-Release-{version}\\gafferBuild\\src\\liboslcomp;%ROOT_DIR%\\OpenShadingLanguage\\working\\OpenShadingLanguage-Release-{version}\\gafferBuild\\src\\oslc;%PATH%",

		},

		"commands" : [
			"mkdir gafferBuild",
			"cd gafferBuild &&"
				" cmake"
				" -Wno-dev -G {cmakeGenerator}"
				" -D CMAKE_CXX_STANDARD={c++Standard}"
				" -D CMAKE_BUILD_TYPE={cmakeBuildType}"
				" -D CMAKE_INSTALL_PREFIX={buildDir}"
				" -D BUILDSTATIC=OFF"
				" -D USE_OIIO_STATIC=OFF"
				" -D OSL_BUILD_PLUGINS=OFF"
				" -D CMAKE_PREFIX_PATH={buildDir}"
				" -D STOP_ON_WARNING=0"
				" -D LLVM_DIRECTORY={buildDir}"
				" -D USE_LLVM_BITCODE=OFF"
				" -D OSL_BUILD_TESTS=OFF"
				" -D BOOST_ROOT={buildDir}"
				" -D Boost_USE_STATIC_LIBS=OFF"
				" -D LLVM_STATIC=ON"
				" -D OPENEXR_INCLUDE_PATH={buildDir}\\include"
				" -D OPENEXR_IMATH_LIBRARY={buildDir}\\lib\\Imath.lib"
				" -D OPENEXR_ILMIMF_LIBRARY={buildDir}\\lib\\IlmImf.lib"
				" -D OPENEXR_IEX_LIBRARY={buildDir}\\lib\\Iex.lib"
				" -D OPENEXR_ILMTHREAD_LIBRARY={buildDir}\\lib\\IlmThread.lib"
				" -D OPENIMAGEIOHOME={buildDir}"
				" -D ZLIB_INCLUDE_DIR={buildDir}\\include"
				" -D ZLIB_LIBRARY={buildDir}\\lib\\zlib.lib"
				" -D EXTRA_CPP_ARGS=\" /D TINYFORMAT_ALLOW_WCHAR_STRINGS\""
				" -D OSL_BUILD_MATERIALX=1"
				" -D OSL_SHADER_INSTALL_DIR={buildDir}\\shaders"
				" -D OSL_USE_OPTIX=1"
				" -D CMAKE_CXX_FLAGS=\"-DBOOST_ALL_NO_LIB\""
				" -D Python_ROOT_DIR={buildDir}"
				" -D Python_FIND_STRATEGY=LOCATION"
				" ..",
			"cd gafferBuild && cmake --build . --config {cmakeBuildType} --target install -- -j {jobs}",

		],

		"postMovePaths" : {

			"{buildDir}/lib/python{pythonVersion}/site-packages/oslquery.pyd" : "{buildDir}/python",

		}

	}

}
