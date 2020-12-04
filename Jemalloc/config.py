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
		"lib/{libraryPrefix}jemalloc*{sharedLibraryExtension}*",
		"lib/{libraryPrefix}jemalloc*.obj",

	],

	"platform:windows" : {

		"commands" : [

			"sh -c \"CC=cl ./autogen.sh\" --prefix={buildDir}",
			"msbuild msvc\jemalloc_vc2017.sln -target:jemalloc /property:Configuration=\"Release\" /property:WindowsTargetPlatformVersion=10.0.19041.0 /property:PlatformToolset=v142" ,
			"copy msvc\\x64\\Release\\jemalloc.dll {buildDir}\\lib",
			"copy msvc\\x64\\Release\\jemalloc.lib {buildDir}\\lib",
			"xcopy /s /e /h /y /i include\\jemalloc {buildDir}\\include\\jemalloc",
			"xcopy /s /e /h /y /i include\\msvc_compat {buildDir}\\include\\msvc_compat",
	
		],

	},
}
