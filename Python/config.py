{

	"downloads" : [

		"https://www.python.org/ftp/python/2.7.15/Python-2.7.15.tgz",

	],

	"license" : "LICENSE",

	"dependencies" : [ "OpenSSL" ],

	"commands" : [

		"./configure --prefix={buildDir} {libraryType} --enable-unicode=ucs4",
		"make -j {jobs}",
		"make install",

	],

	"variables" : {

		"libraryType" : "--enable-shared",

	},

	"platform:osx" : {

		"variables" : {

			"libraryType" : "--enable-framework={buildDir}/lib",

		},

		"symbolicLinks" : [

			( "{buildDir}/bin/python", "../lib/Python.framework/Versions/Current/bin/python" ),
			( "{buildDir}/bin/python2", "../lib/Python.framework/Versions/Current/bin/python2" ),
			( "{buildDir}/bin/python2.7", "../lib/Python.framework/Versions/Current/bin/python2.7" ),

		],

	},

}
