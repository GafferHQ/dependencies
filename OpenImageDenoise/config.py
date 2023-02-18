{

	"downloads" : [

		"https://github.com/OpenImageDenoise/oidn/releases/download/v1.4.3/oidn-1.4.3.src.tar.gz"

	],

	"url" : "https://www.openimagedenoise.org/",

	"license" : "LICENSE.txt",

	"dependencies" : ["ISPC"],

	"environment" : {

		"PATH" : "{buildDir}/bin:$PATH",
		"LD_LIBRARY_PATH" : "{buildDir}/lib:$LD_LIBRARY_PATH",

	},

	"commands" : [

		"mkdir gafferBuild",
		"cd gafferBuild &&"
			" cmake"
            " -G {cmakeGenerator}"
			" -D CMAKE_INSTALL_PREFIX={buildDir}"
			" -D CMAKE_PREFIX_PATH={buildDir}"
			" -D CMAKE_BUILD_TYPE=Release"
			" -D CMAKE_INSTALL_LIBDIR={buildDir}/lib"
			" ..",
		"cd gafferBuild && cmake --build . --config Release --target install -- -j {jobs}",

	],

	"manifest" : [

		"cmake/OpenImageDenoise*",
		"include/OpenImageDenoise*",
		"lib/*OpenImageDenoise*",

	],

	"platform:linux" : {

		"environment" : {

			"LD_LIBRARY_PATH" : "{buildDir}/lib:$LD_LIBRARY_PATH",

		},

	},

	"platform:osx" : {

		"environment" : {

			"LD_LIBRARY_PATH" : "{buildDir}/lib:$LD_LIBRARY_PATH",

		},

	},

	"platform:windows" : {

		"environment" : {

			"PATH" : "{buildDir}/lib;%PATH%",

		},

	},

}
