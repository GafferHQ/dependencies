{

	"downloads" : [

		"https://github.com/wjakob/nanobind/archive/refs/tags/v2.12.0.tar.gz"

	],

	"url" : "https://nanobind.readthedocs.io",

	"license" : "LICENSE",

	"dependencies" : [ "Python", "Robin-map" ],

	"commands" : [

		"cmake"
			" -D CMAKE_INSTALL_PREFIX={buildDir}"
			" -D NB_TEST=0"
			" -D NB_USE_SUBMODULE_DEPS=0"
			" .",

		"cmake --install .",

		"cp -r include/nanobind {buildDir}/include",

	],

	"manifest" : [

		"include/nanobind",

	],

}
