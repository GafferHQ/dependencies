{

	"downloads" : [

		"https://files.pythonhosted.org/packages/a2/3f/986cf2693584a8738985a63d62aee32f9317e9895ab3c224cbf17d5ca371/PyOpenGL-3.0.2.tar.gz"

	],

	"license" : "license.txt",

	"environment" : {

		"LD_LIBRARY_PATH" : "{buildDir}/lib",
		"DYLD_FRAMEWORK_PATH" : "{buildDir}/lib",

	},

	"commands" : [

		"{buildDir}/bin/python setup.py install --prefix {buildDir} --install-lib {buildDir}/python",

	],

	"platform:windows" : {

		"commands" : [

			"{buildDir}\\bin\\python setup.py install --prefix {buildDir} --install-lib {buildDir}\\python",

		]

	}

}
