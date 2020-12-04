{

	"downloads" : [

		"https://github.com/jemalloc/jemalloc/releases/download/3.6.0/jemalloc-3.6.0.tar.bz2",

	],

	"url" : "http://jemalloc.net/",

	"license" : "COPYING",

	"commands" : [

		"./configure --prefix={buildDir}",
		"make -j {jobs}",
		"make install"

	],

	"manifest" : [

		"include/jemalloc",
		"{sharedLibraryDir}/{libraryPrefix}jemalloc*{sharedLibraryExtension}*",
		"lib/{libraryPrefix}jemalloc*.obj",

	],

	"platform:windows" : {

		"commands" : [

			"sh -c \"CC=cl ./autogen.sh\" --prefix={buildDir}",
			"msbuild msvc\jemalloc_vc2017.sln -target:jemalloc /property:Configuration=\"Release\" /property:WindowsTargetPlatformVersion=10.0.20348.0" ,

		],

		"postMovePaths" : {

			"msvc/x64/Release/jemalloc.dll" : "{buildDir}/bin",
			"msvc/x64/Release/jemalloc.lib" : "{buildDir}/lib",
			"include/jemalloc" : "{buildDir}/include",
			"include/msvc_compat" : "{buildDir}/include",

		}

	},
}
