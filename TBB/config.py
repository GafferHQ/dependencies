{

	"downloads" : [

		"https://github.com/01org/tbb/archive/2018_U5.tar.gz"

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
