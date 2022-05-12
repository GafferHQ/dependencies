{

	"downloads" : [

		"https://files.pythonhosted.org/packages/32/c8/564be4d12629b912ea431f1a50eb8b3b9d00f1a0b1ceff17f266be190007/subprocess32-3.5.4.tar.gz"
	],

	"url" : "https://github.com/google/python-subprocess32",

	"license" : "LICENSE",

	"dependencies" : [ "Python" ],

	"environment" : {

		"PATH" : "{buildDir}/bin:$PATH",
		"DYLD_FRAMEWORK_PATH" : "{buildDir}/lib",
		"LD_LIBRARY_PATH" : "{buildDir}/lib"

	},

	"commands" : [

		"python setup.py install --single-version-externally-managed --root /",

	],

}
