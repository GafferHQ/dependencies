{

	"downloads" : [

		"https://files.pythonhosted.org/packages/b2/5e/3a21abf3cd467d7876045335e681d276ac32492febe6d98ad89562d1a7e1/Jinja2-3.1.3.tar.gz"

	],

	"url" : "https://palletsprojects.com/p/jinja/",

	"license" : "LICENSE.rst",

	"dependencies" : [ "Python", "MarkupSafe" ],

	"environment" : {

		"PATH" : "{buildDir}/bin:$PATH",
		"DYLD_FRAMEWORK_PATH" : "{buildDir}/lib",
		"LD_LIBRARY_PATH" : "{buildDir}/lib",
		"PYTHONPATH" : "{buildDir}/python"

	},

	"commands" : [

		"{buildDir}/bin/python setup.py install --root / --prefix {buildDir} {extraArgs}",

	],

	"variables" : {

		"extraArgs" : "",

	},

	"platform:macos" : {

		"variables" : {

			"extraArgs" : "--install-lib {pythonLibDir}/python{pythonVersion}/site-packages",

		}

	},

}
