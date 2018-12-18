{

	"downloads" : [

		"https://github.com/libjpeg-turbo/libjpeg-turbo/releases/download/3.0.3/libjpeg-turbo-3.0.3.tar.gz",

	],

	"url" : "https://libjpeg-turbo.org",
	"license" : "LICENSE.md",

	"commands" : [

		"mkdir build",
		"cd build &&"
			" cmake"
			" -D CMAKE_INSTALL_PREFIX={buildDir}"
			" -D CMAKE_INSTALL_LIBDIR={buildDir}/lib"
			" ..",
		"cd build && make -j {jobs} && make install",

	],

	"manifest" : [

		"include/jconfig.h",
		"include/jerror.h",
		"include/jmorecfg.h",
		"include/jpeglib.h",

		"{sharedLibraryDir}/libjpeg*{sharedLibraryExtension}*",	# Linux / Mac OS
		"lib/libjpeg.lib",
        "lib/jpeg.lib",
		"{sharedLibraryDir}/jpeg62{sharedLibraryExtension}",	# Windows only
		"{sharedLibraryDir}/turbojpeg{sharedLibraryExtension}",	# Windows only
	],
	"platform:windows" : {

		"commands" : [

			"mkdir gafferBuild",
			"cd gafferBuild && "
				" cmake"
				" -G {cmakeGenerator}"
				" -D CMAKE_BUILD_TYPE={cmakeBuildType}"
				" -D CMAKE_INSTALL_PREFIX={buildDir}"
				" -D WITH_SIMD=OFF"
				" ..",

			"cd gafferBuild && cmake --build . --config {cmakeBuildType} --target install -- -j {jobs}",
		],

	},

}
