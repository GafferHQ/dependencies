{

	"downloads" : [

		"https://github.com/imageworks/OpenShadingLanguage/archive/Release-1.9.9.tar.gz"

	],

	"license" : "LICENSE",

	"environment" : {

		# Needed because the build process runs oslc, which
		# needs to link to the OIIO libraries.
		"DYLD_FALLBACK_LIBRARY_PATH" : "{buildDir}/lib",
		"LD_LIBRARY_PATH" : "{buildDir}/lib",

	},

	"commands" : [

		"mkdir gafferBuild",
		"cd gafferBuild &&"
		 	" cmake"
		 	" -G {cmakeGenerator}"
		 	" -D CMAKE_BUILD_TYPE={cmakeBuildType}"
		 	" -D CMAKE_INSTALL_PREFIX={buildDir}"
		 	" -D CMAKE_INSTALL_LIBDIR={buildDir}/lib"
		 	" -D CMAKE_PREFIX_PATH={buildDir}"
		 	" -D STOP_ON_WARNING=0"
		 	" -D ENABLERTTI=1"
		 	" -D LLVM_STATIC=1"
		 	" ..",
		"cd gafferBuild && cmake --build . --config {cmakeBuildType} --target install -- -j {jobs}"

	],

	"platform:windows" : {

		"variables" : {

			"version" : "1.9.9",

		},

		"environment" : {
			"PATH" : "%PATH%;{buildDir}\\lib;{buildDir}\\bin;%ROOT_DIR%\\winbuild\\FlexBison\\bin;%ROOT_DIR%\\OpenShadingLanguage\\working\\OpenShadingLanguage-Release-{version}\\gafferBuild\\src\\liboslcomp",

		},

		"commands" : [
			"mkdir gafferBuild",
			"cd gafferBuild &&"
			 	" cmake"
			 	" -Wno-dev -G {cmakeGenerator}"
				" -D CMAKE_BUILD_TYPE={cmakeBuildType}"
				" -D CMAKE_INSTALL_PREFIX={buildDir}"
				" -D BUILDSTATIC=OFF"
				" -D USE_OIIO_STATIC=OFF"
				" -D OSL_BUILD_PLUGINS=OFF"
				" -D ENABLERTTI=1"
				" -D CMAKE_PREFIX_PATH={buildDir}"
				" -D STOP_ON_WARNING=0"
				" -D LLVM_DIRECTORY={buildDir}"
				" -D USE_LLVM_BITCODE=OFF"
				" -D OSL_BUILD_TESTS=OFF"
				" -D BOOST_ROOT={buildDir}"
				" -D Boost_USE_STATIC_LIBS=OFF"
				" -D LLVM_STATIC=ON"
				" -D OPENEXR_INCLUDE_PATH={buildDir}\\include"
				" -D OPENEXR_IMATH_LIBRARY={buildDir}\\lib\\Imath-2_2.lib"
				" -D OPENEXR_ILMIMF_LIBRARY={buildDir}\\lib\\IlmImf-2_2.lib"
				" -D OPENEXR_IEX_LIBRARY={buildDir}\\lib\\Iex-2_2.lib"
				" -D OPENEXR_ILMTHREAD_LIBRARY={buildDir}\\lib\\IlmThread-2_2.lib"
				" -D OPENIMAGEIOHOME={buildDir}"
				" -D ZLIB_INCLUDE_DIR={buildDir}\\include"
				" -D ZLIB_LIBRARY={buildDir}\\lib\\zlib.lib"
				" -D EXTRA_CPP_ARGS=\"/DTINYFORMAT_ALLOW_WCHAR_STRINGS\""
				" -D FLEX_EXECUTABLE=flex"
				" -D BISON_EXECUTABLE=bison"
			 	" ..",
			"cd gafferBuild && cmake --build . --config {cmakeBuildType} --target install -- -j {jobs}"
		
		],

	}

}
