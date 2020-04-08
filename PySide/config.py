{

	"downloads" : [

		"https://download.qt.io/official_releases/QtForPython/pyside2/PySide2-5.12.6-src/pyside-setup-everywhere-src-5.12.6.tar.xz"

	],

	"license" : "LICENSE.LGPLv3",

	# PySide uses clang for parsing headers, hence the LLVM dependency.
	"dependencies" : [ "Python", "Qt", "LLVM" ],

	"environment" : {

		"PATH" : "{buildDir}/bin:$PATH",

	},

	"commands" : [

		"python setup.py --verbose-build --ignore-git --no-examples --parallel {jobs} install",

	],

	"platform:linux" : {

		"environment" : {

			"LD_LIBRARY_PATH" : "{buildDir}/lib",

		},

	},

	"platform:osx" : {

		"environment" : {

			"DYLD_FRAMEWORK_PATH" : "{buildDir}/lib",

		},

	},

}
