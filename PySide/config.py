{

	"downloads" : [

		"https://download.qt.io/official_releases/QtForPython/pyside2/PySide2-5.15.14-src/pyside-setup-opensource-src-5.15.14.tar.xz"

	],

	"url" : "http://www.pyside.org",

	"license" : "LICENSE.LGPLv3",

	# PySide uses clang for parsing headers, hence the LLVM dependency.
	"dependencies" : [ "Python", "Qt", "LLVM" ],

	"environment" : {

		"PATH" : "{buildDir}/bin:$PATH",

	},

	"commands" : [

		"python setup.py --parallel {jobs} install",

	],

	"manifest" : [

		"include/PySide2",
		"lib/python{pythonVersion}/site-packages/PySide2",
		"lib/python{pythonVersion}/site-packages/pyside2uic",

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

			( "{buildDir}/bin/pyside2-uic", "../lib/Python.framework/Versions/Current/bin/pyside2-uic" ),

		]

	},

	"platform:windows" : {

		"manifest" : [

			"include/PySide2",
			"python/PySide2",
			"python/pyside2uic.exe",
			"python/shiboken2",
			"python/shiboken2_generator",

		],

		"environment" : {

			"PATH" : "{buildDir}\\bin;{buildDir}\\lib;%PATH%",
			"VERSION" : "",	# PySide will pull in VERSION from the environment if it exists and cause a failure because the --ignore-git conflicts with VERSION

		},

		"commands" : [
			"xcopy /s /e /h /y /i %ROOT_DIR%\\PySide\\working\\pyside-setup-opensource-src-5.15.14 %ROOT_DIR%\\PySide\\working\\p",	# Shorten overall paths to avoid Windows command character limit
			"cd ..\\p && python setup.py install"
				" --ignore-git"
				" --qmake={buildDir}\\bin\\qmake.exe"
				" --openssl={buildDir}\\bin"
				" --cmake=\"C:\\Program Files\\CMake\\bin\\cmake.exe\""
				" --parallel {jobs}"
				" --no-examples",
			# This shouldn't be copied like this, pyside2-uic.exe and uic.exe are not equivalent
			# lambda c : shutil.copy( pathlib.Path( c["variables"]["buildDir"] ) / "lib" / "site-packages" / "PySide2" / "uic.exe", pathlib.Path( c["variables"]["buildDir"] ) / "lib" / "site-packages" / "PySide2" / "pyside2-uic.exe" )

		],

		"postMovePaths" : {

			# "{buildDir}/lib/site-packages/PySide2/pyside2-uic.exe" : "{buildDir}/bin",
			"{buildDir}/lib/site-packages/PySide2/uic.exe" : "{buildDir}/bin",
			"{buildDir}/lib/site-packages/PySide2" : "{buildDir}/python",
			"{buildDir}/lib/site-packages/shiboken2" : "{buildDir}/python",
			"{buildDir}/lib/site-packages/shiboken2_generator" : "{buildDir}/python",

		},

	},

}
