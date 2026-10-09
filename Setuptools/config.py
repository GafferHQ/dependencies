{

	"downloads" : [

		"https://files.pythonhosted.org/packages/0d/1c/73e719955c59b8e424d015ab450f51c0af856ae46ea2da83eba51cc88de1/setuptools-81.0.0.tar.gz"

	],

	"url" : "https://setuptools.pypa.io",

	"license" : "LICENSE",

	"dependencies" : [ "Python" ],

	"environment" : {

		"LD_LIBRARY_PATH" : "{buildDir}/lib",
		"DYLD_FRAMEWORK_PATH" : "{buildDir}/lib",
		"PYTHONPATH" : "{buildDir}/python",

	},

	"commands" : [

		"{buildDir}/bin/python setup.py install --prefix {buildDir} --install-lib {buildDir}/python",

	],

}
