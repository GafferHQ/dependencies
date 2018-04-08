{

	"downloads" : [

		"https://download.qt.io/official_releases/QtForPython/pyside2/PySide2-5.15.12-src/pyside-setup-opensource-src-5.15.12.tar.xz"

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

		"downloads" : [

			# Avoids a symlink introduced in 5.15.4 that causes a warning that causes build.py to make a poor decision when testing `len( dirs ) == 1`
			"https://download.qt.io/official_releases/QtForPython/pyside2/PySide2-5.15.4-src/pyside-setup-opensource-src-5.15.4.zip"

		],

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
			"xcopy /s /e /h /y /i %ROOT_DIR%\\PySide\\working\\pyside-setup-opensource-src-5.15.4 %ROOT_DIR%\\PySide\\working\\p",	# Shorten overall paths to avoid Windows command character limit
			"cd ..\\p && python setup.py install"
				" --ignore-git"
				" --qmake=%BUILD_DIR%\\bin\\qmake.exe"
				" --openssl={buildDir}\\lib"
				" --cmake=\"C:\\Program Files\\CMake\\bin\\cmake.exe\""
				" --parallel {jobs}"
				" --no-examples",
				
				"xcopy /s /e /h /y /i {buildDir}\\lib\\site-packages\\PySide2 {buildDir}\\python\\PySide2",
				"xcopy /s /e /h /y /i {buildDir}\\lib\\site-packages\\shiboken2 {buildDir}\\python\\shiboken2",
				"xcopy /s /e /h /y /i {buildDir}\\lib\\site-packages\\shiboken2_generator {buildDir}\\python\\shiboken2_generator",
                "copy {buildDir}\\python\\PySide2\\uic.exe {buildDir}\\bin\\pyside2-uic.exe"

		]
	},

}
