{

	"downloads" : [

		"https://github.com/dvarrazzo/py-setproctitle/archive/version-1.1.10.tar.gz"
	],

	"url" : "https://github.com/dvarrazzo/py-setproctitle",

	"license" : "COPYRIGHT",

	"dependencies" : [ "Python" ],

	"environment" : {

		"PATH" : "{buildDir}/bin:$PATH",
		"DYLD_FRAMEWORK_PATH" : "{buildDir}/lib",
		"LD_LIBRARY_PATH" : "{buildDir}/lib"

	},

	"commands" : [

		"python setup.py install --root / --prefix {buildDir} --install-lib {buildDir}/python",

	],

	"manifest" : [

		"python/setproctitle*{sharedLibraryExtension}"

	],

}
