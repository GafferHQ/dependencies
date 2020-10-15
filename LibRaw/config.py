{

	"downloads" : [

		"https://www.libraw.org/data/LibRaw-0.19.5.tar.gz",

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
		"lib/libraw*{sharedLibraryExtension}*",
		"lib/libraw{staticLibraryExtension}",

	],

	"platform:windows" : {

		"commands" : [

			"nmake -f Makefile.msvc",
			"xcopy /s /e /h /y /i libraw include\\libraw",
			"copy bin\\libraw.dll lib\\libraw.dll"

		],

	},

	

}
