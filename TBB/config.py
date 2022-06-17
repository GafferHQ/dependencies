{

	"downloads" : [

		"https://github.com/oneapi-src/oneTBB/archive/refs/tags/v2020.3.tar.gz"

	],

	"url" : "http://threadingbuildingblocks.org/",

	"license" : "LICENSE",

	"commands" : [

		"make -j {jobs} stdver=c++{c++Standard}",
		"mkdir -p {buildDir}/include",
		"cp -r include/tbb {buildDir}/include",
		"mkdir -p {buildDir}/lib",
		"{installLibsCommand}",

	],

	"manifest" : [

		"include/tbb",
		"lib/libtbb*{sharedLibraryExtension}*",

	],

	"platform:linux" : {

		"environment" : {

			"tbb_os" : "linux",

		},

		"variables" : {

			"installLibsCommand" : "cp build/*_release/*.so* {buildDir}/lib",

		},

	},

	"platform:macos" : {

		"environment" : {

			"tbb_os" : "macos",

		},

		"variables" : {

			"installLibsCommand" : "cp build/macos_*_release/*.dylib {buildDir}/lib",

		},

	},

}
