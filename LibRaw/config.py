{

	"downloads" : [

		"https://www.libraw.org/data/LibRaw-0.21.4.tar.gz",

	],

	"url" : "https://www.libraw.org/",

	"license" : "LICENSE.LGPL",

	"commands" : [

		"./configure --disable-examples --prefix={buildDir}",
		"make -j {jobs}",
		"make install"

	],

	# The "lib" prefix is correct for all platforms including Windows
	"manifest" : [

		"include/libraw",
		"{sharedLibraryDir}/libraw*{sharedLibraryExtension}*",
		"lib/libraw{staticLibraryExtension}",

	],

	"platform:windows" : {

		"commands" : [

			"msbuild LibRaw.sln -target:libraw /property:Configuration=\"Release\" /property:PlatformToolset=v143 /property:WindowsTargetPlatformVersion=10.0.20348.0",

		],
		
		"postMovePaths" : {
			"libraw/*.h" : "{buildDir}/include/libraw",
			"buildfiles/release-x86_64/libraw.dll" : "{buildDir}/bin",
			"buildfiles/release-x86_64/libraw.lib" : "{buildDir}/lib",
		}

	},

	

}
