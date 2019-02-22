{

	"downloads" : [

		"https://download.osgeo.org/libtiff/old/tiff-3.8.2.tar.gz",

	],

	"license" : "COPYRIGHT",

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

	"platform:windows" : {

		"commands" : [

			"nmake /f makefile.vc",
			"copy libtiff\\*.h {buildDir}\\include",
			"copy libtiff\\libtiff.lib {buildDir}\\lib"

		],

	},

}
