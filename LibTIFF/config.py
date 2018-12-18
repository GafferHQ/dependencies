{

	"downloads" : [

		"https://download.osgeo.org/libtiff/tiff-4.1.0.zip",

	],

	"url" : "http://www.libtiff.org",

	"license" : "COPYRIGHT",

	"dependencies" : [ "LibJPEG-Turbo" ],

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
		"lib/libtiff*{sharedLibraryExtension}*",
        "lib/libtiff.lib",

	],
	"platform:windows" : {

		"commands" : [

			"nmake /f makefile.vc",
			"copy libtiff\\*.h {buildDir}\\include",
			"copy libtiff\\libtiff.lib {buildDir}\\lib",

		],

	},

}
