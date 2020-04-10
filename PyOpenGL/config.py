{

	"downloads" : [

		"https://files.pythonhosted.org/packages/d7/8a/5db9096aa6506e405309c400bd0feb41997689cbba30683479c30dba6355/PyOpenGL-3.1.4.tar.gz"

	],

	"license" : "license.txt",

	"dependencies" : [ "Python" ],

	"environment" : {

		"LD_LIBRARY_PATH" : "{buildDir}/lib",
		"DYLD_FRAMEWORK_PATH" : "{buildDir}/lib",
		"PYTHONPATH" : "{buildDir}/python",

	},

	"commands" : [

		"{buildDir}/bin/python setup.py install --prefix {buildDir} --install-lib {buildDir}/python",

	],

	"manifest" : [

		"python/OpenGL",

	],

}
