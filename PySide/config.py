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
			"DYLD_FALLBACK_LIBRARY_PATH" : "{buildDir}/lib",

		},

		"symbolicLinks" : [

			( "{buildDir}/bin/pyside6-uic", "../lib/Python.framework/Versions/Current/bin/pyside6-uic" ),

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
			"LLVM_INSTALL_DIR" : "{buildDir}",
			"VERSION" : "",	# PySide will pull in VERSION from the environment if it exists and cause a failure because the --ignore-git conflicts with VERSION

		},

		"commands" : [
			"xcopy /s /e /h /y /i %ROOT_DIR%\\PySide\\working\\pyside-setup-opensource-src-6.5.8 %ROOT_DIR%\\PySide\\working\\p",	# Shorten overall paths to avoid Windows command character limit
			"cd ..\\p && python setup.py install"
				" --ignore-git"
				" --openssl={buildDir}\\bin"
				" --parallel {jobs}"
				" --qtpaths={buildDir}/bin/qtpaths6.exe"
				" install",
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
