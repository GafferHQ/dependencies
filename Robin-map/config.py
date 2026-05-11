{

	"downloads" : [

		"https://github.com/Tessil/robin-map/archive/refs/tags/v1.4.1.tar.gz"

	],

	"url" : "https://github.com/Tessil/robin-map",

	"license" : "LICENSE",

	"commands" : [

		"mkdir build",
		"cd build && cmake -D CMAKE_INSTALL_PREFIX={buildDir} ..",
		"cd build && cmake --build . && cmake --install .",

	],

}
