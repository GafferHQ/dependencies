{

	"downloads" : [

		"https://github.com/oneapi-src/oneTBB/archive/refs/tags/v2020.2.tar.gz"

	],

	"url" : "http://threadingbuildingblocks.org/",

	"license" : "LICENSE",

	"commands" : [

		"make -j {jobs} stdver=c++{c++Standard}",
		"cp -r include/tbb {buildDir}/include",
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

	"platform:osx" : {

		"environment" : {

			"tbb_os" : "macos",

		},

		"variables" : {

			"installLibsCommand" : "cp build/macos_*_release/*.dylib {buildDir}/lib",

		},

	},

}
