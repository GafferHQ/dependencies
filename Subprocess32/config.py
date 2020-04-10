{

	"downloads" : [

		"https://files.pythonhosted.org/packages/28/8d/33ccbff51053f59ae6c357310cac0e79246bbed1d345ecc6188b176d72c3/subprocess32-3.2.6.tar.gz"
	],

	"license" : "LICENSE",

	"dependencies" : [ "Python" ],

	"environment" : {

		"PATH" : "{buildDir}/bin:$PATH",
		"DYLD_FRAMEWORK_PATH" : "{buildDir}/lib",
		"LD_LIBRARY_PATH" : "{buildDir}/lib"

	},

	"commands" : [

		"python setup.py install",

	],

	"variant:Python:3" : {

		"enabled" : False,

	},

}
