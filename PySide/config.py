{

	"downloads" : [

		"https://download.qt.io/official_releases/QtForPython/pyside6/PySide6-6.5.8-src/pyside-setup-opensource-src-6.5.8.tar.xz"

	],

	"url" : "http://www.pyside.org",

	"license" : "LICENSES/LGPL-3.0-only.txt",

	# PySide uses clang for parsing headers, hence the LLVM dependency.
	"dependencies" : [ "Python", "Qt", "LLVM" ],

	"environment" : {

		"PATH" : "{buildDir}/bin:$PATH",

	},

	"commands" : [

		"python setup.py --parallel {jobs} --qtpaths={buildDir}/bin/qtpaths6 install",

	],

	"platform:linux" : {

		"environment" : {

			"LD_LIBRARY_PATH" : "{buildDir}/lib",

		},

	},

	"platform:macos" : {

		"environment" : {

			"DYLD_FRAMEWORK_PATH" : "{buildDir}/lib",

		},

		"symbolicLinks" : [

			( "{buildDir}/bin/pyside6-uic", "../lib/Python.framework/Versions/Current/bin/pyside6-uic" ),

		]

	},

}
