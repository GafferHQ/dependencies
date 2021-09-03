{

	"downloads" : [

		"https://github.com/pybind/pybind11/archive/v2.6.2.tar.gz"

	],

	"url" : "https://pybind11.readthedocs.io",

	"license" : "LICENSE",

	"dependencies" : [ "Python" ],

	"environment" : {

		"PATH" : "{buildDir}/bin:$PATH",
		"LD_LIBRARY_PATH" : "{buildDir}/lib",

	},

	"commands" : [

		"cmake"
		" -D CMAKE_INSTALL_PREFIX={buildDir} ."
		" -D PYBIND11_TEST=0"
		" -D PYBIND11_FINDPYTHON=1"
		" -D Python_ROOT_DIR={buildDir}"
		" -D Python_FIND_STRATEGY=LOCATION",
		"make install",

	],

	"manifest" : [

		"include/pybind11",

	],

}
