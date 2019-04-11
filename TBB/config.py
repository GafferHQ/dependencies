{

	"downloads" : [

		"https://github.com/01org/tbb/archive/2017_U6.tar.gz"

	],

	"license" : "LICENSE",

	"commands" : [

		"make -j {jobs} stdver=c++11",
		"cp -r include/tbb {buildDir}/include",
		"{installLibsCommand}",

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
