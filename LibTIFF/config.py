{

	"downloads" : [

		"https://download.osgeo.org/libtiff/tiff-4.0.10.tar.gz",

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
