{

	"downloads" : [

		"https://github.com/lz4/lz4/archive/v1.9.2.tar.gz"

	],

	"url" : "http://www.lz4.org",
	"license" : "lib/LICENSE",

	"commands" : [

		"make install PREFIX={buildDir}",

	],

	"manifest" : [

		"lib/liblz4*{sharedLibraryExtension}*",

	],

	"platform:windows" : {

		"manifest" : [

			"lib\\liblz4.dll",
			"lib\\liblz4.lib",

		],

		"commands" : [

			# From the LZ4 appveyor script
			"msbuild visual\\VS2017\\lz4.sln /m /verbosity:minimal /property:PlatformToolset=v142 /property:WindowsTargetPlatformVersion=10.0.19041.0 /t:Clean,Build /p:Platform=x64 /p:Configuration=Release",
			"copy visual\\VS2017\\bin\\x64_Release\\liblz4.dll {buildDir}\\lib",
			"copy visual\\VS2017\\bin\\x64_Release\\liblz4.lib {buildDir}\\lib",

		],

	},

}
