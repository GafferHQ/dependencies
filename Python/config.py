{

	"downloads" : [ "https://www.python.org/ftp/python/3.7.6/Python-3.7.6.tgz" ],

	"publicVariables" : {

		"pythonVersion" : "3.7",
		# Python 3 unconditionally puts these infuriating "m" ABI suffixes on
		# everything. This is intended to allow different types of Python builds
		# to exist in the same place, but that's not a problem we have. The problem
		# we _do_ have is that a bunch of our projects get tripped up by these
		# suffixes. See : https://www.python.org/dev/peps/pep-3149.
		"pythonABIVersion" : "3.7m",
		"pythonMajorVersion" : "3",
		"pythonMinorVersion" : "7",
		"pythonIncludeDir" : "{buildDir}/include/python{pythonABIVersion}",
		"pythonLibDir" : "{buildDir}/lib",

	},

	"url" : "https://www.python.org",

	"license" : "LICENSE",

	"dependencies" : [ "OpenSSL", "LibFFI" ],

	"environment" : {

		"LDFLAGS" : "-L{buildDir}/lib",
		"CPPFLAGS" : "-I{buildDir}/include",
		"LD_LIBRARY_PATH" : "{buildDir}/lib",

	},

	"commands" : [

		"./configure --prefix={buildDir} {libraryType} --enable-unicode=ucs4 --with-ensurepip=install --with-system-ffi --with-openssl={buildDir}",
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

	"symbolicLinks" : [

		( "{buildDir}/bin/python", "python3" ),

	],

	"platform:macos" : {

		# Python 3.7 doesn't compile for M1, so we use 3.8 on Mac until
		# we upgrade all platforms to a mutually compatible version. We
		# don't support the Python 2 variant at all on Mac.
		"downloads" : [

			"https://www.python.org/ftp/python/3.8.13/Python-3.8.13.tar.xz",

		],

		"variables" : {

			"libraryType" : "--enable-framework={buildDir}/lib",

		},

		"environment" : {

			"MACOSX_DEPLOYMENT_TARGET" : "11.0",

		},

		"publicVariables" : {

			# See `downloads`.
			"pythonVersion" : "3.8",
			"pythonABIVersion" : "3.8m",
			"pythonMajorVersion" : "3",
			"pythonMinorVersion" : "8",

			"pythonIncludeDir" : "{buildDir}/lib/Python.framework/Headers",
			"pythonLibDir" : "{buildDir}/lib/Python.framework/Versions/{pythonVersion}/lib",

		},

		"symbolicLinks" : [

			( "{buildDir}/bin/python", "../lib/Python.framework/Versions/Current/bin/python{pythonMajorVersion}" ),
			( "{buildDir}/bin/python{pythonMajorVersion}", "../lib/Python.framework/Versions/Current/bin/python{pythonMajorVersion}" ),
			( "{buildDir}/bin/python{pythonVersion}", "../lib/Python.framework/Versions/Current/bin/python{pythonVersion}" ),
			( "{buildDir}/lib/Python.framework/Versions/Current/lib/libpython{pythonMajorVersion}.dylib", "libpython{pythonMajorVersion}.{pythonMinorVersion}.dylib" ),
		],

	},

}
