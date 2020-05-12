{

	"downloads" : [

		"https://www.autodesk.com/content/dam/autodesk/www/Company/files/PySide2-Maya-2018_6.tgz"

	],

	"url" : "http://www.pyside.org",

	"license" : "LICENSE.LGPLv21",

	"dependencies" : [ "Python", "Qt" ],

	"environment" : {

		"PATH" : "{buildDir}/bin:$PATH",

	},

	"commands" : [

		"python setup.py --ignore-git --no-examples --jobs {jobs} --osx-use-libc++ install"


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
