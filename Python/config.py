{

	"publicVariables" : {

		"pythonVersion" : "2.7",
		"pythonMajorVersion" : "2",
		"pythonIncludeDir" : "{buildDir}/include/python{pythonVersion}",
		"pythonLibDir" : "{buildDir}/lib",

	},

	"downloads" : [

		"https://www.python.org/ftp/python/2.7.15/Python-2.7.15.tgz",

	],

	"url" : "https://www.python.org",

	"license" : "LICENSE",

	"dependencies" : [ "OpenSSL" ],

	"commands" : [

		"./configure --prefix={buildDir} {libraryType} --enable-unicode=ucs4",
		"make -j {jobs}",
		"make install",

	],

	"manifest" : [

		"bin/python",
		"bin/python*[0-9]",

		"include/python*",

		"lib/libpython*{sharedLibraryExtension}*",
		"lib/Python.framework*",
		"lib/python{pythonVersion}",

	],

	"variables" : {

		"libraryType" : "--enable-shared",

	},

	"environment" : {

		"LDFLAGS" : "-L{buildDir}/lib",
		"CPPFLAGS" : "-I{buildDir}/include"
	},

	"platform:osx" : {

		"variables" : {

			"libraryType" : "--enable-framework={buildDir}/lib",

		},

		"publicVariables" : {

			"pythonIncludeDir" : "{buildDir}/lib/Python.framework/Headers",
			"pythonLibDir" : "{buildDir}/lib/Python.framework/Versions/{pythonVersion}/lib",

		},

		"symbolicLinks" : [

			( "{buildDir}/bin/python", "../lib/Python.framework/Versions/Current/bin/python" ),
			( "{buildDir}/bin/python{pythonMajorVersion}", "../lib/Python.framework/Versions/Current/bin/python{pythonMajorVersion}" ),
			( "{buildDir}/bin/python{pythonVersion}", "../lib/Python.framework/Versions/Current/bin/python{pythonVersion}" ),

		],

	},

}
