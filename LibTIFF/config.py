{

	"downloads" : [

		"https://download.osgeo.org/libtiff/tiff-4.1.0.zip",

	],

	"url" : "http://www.libtiff.org",

	"license" : "COPYRIGHT",

	"dependencies" : [ "LibJPEG-Turbo", "ZLib" ],

	"environment" : {

		# Needed to make sure we link against the libjpeg
		# in the Gaffer distribution and not the system
		# libjpeg.
		"CPPFLAGS" : "-I{buildDir}/include",
		"LDFLAGS" : "-L{buildDir}/lib",

	},

	"commands" : [

		"./configure --without-x --prefix={buildDir}",
		"make -j {jobs}",
		"make install"

	],

	"manifest" : [

		"include/tiff*",
		"{sharedLibraryDir}/{libraryPrefix}tiff*{sharedLibraryExtension}*",
		"lib/{libraryPrefix}tiff.lib",

	],
	"platform:windows" : {

		"commands" : [

			"mkdir gafferBuild",
			"cd gafferBuild && "
				" cmake"
				" -G {cmakeGenerator}"
				" -D CMAKE_BUILD_TYPE={cmakeBuildType}"
				" -D CMAKE_INSTALL_PREFIX={buildDir}"
				" -D ZLIB_INCLUDE_DIR={buildDir}\\include"
				" -D ZLIB_LIBRARY={buildDir}\\lib\\zlib.lib"
				" ..",

			"cd gafferBuild && cmake --build . --config {cmakeBuildType} --target install -- -j {jobs}",

		],

		"postMovePaths" : {

			"libtiff/*.h" : "{buildDir}/include",
			"libtiff/libtiff.lib" : "{buildDir}/lib",

		}

	},

}
