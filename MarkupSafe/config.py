{

	"downloads" : [

		"https://files.pythonhosted.org/packages/87/5b/aae44c6655f3801e81aa3eef09dbbf012431987ba564d7231722f68df02d/MarkupSafe-2.1.5.tar.gz"

	],

	"url" : "https://palletsprojects.com/p/markupsafe/",

	"license" : "LICENSE.rst",

	"dependencies" : [ "Python" ],

	"environment" : {

		"PATH" : "{buildDir}/bin:$PATH",
		"DYLD_FRAMEWORK_PATH" : "{buildDir}/lib",
		"LD_LIBRARY_PATH" : "{buildDir}/lib",
		"PYTHONPATH" : "{buildDir}/python"

	},

	"commands" : [

		"{buildDir}/bin/python setup.py install --root / --prefix {buildDir}",

	],

}
