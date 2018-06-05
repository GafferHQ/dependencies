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
			" -D CMAKE_INSTALL_PREFIX={buildDir}"
			" -D CMAKE_INSTALL_LIBDIR={buildDir}/lib"
			" -D CMAKE_PREFIX_PATH={buildDir}"
			" -D STOP_ON_WARNING=0"
			" -D ENABLERTTI=1"
			" -D LLVM_STATIC=1"
			" ..",
		"cd gafferBuild && make install -j {jobs} VERBOSE=1"

	],

}
