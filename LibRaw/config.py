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

	"manifest" : [

		"include/libraw",
		"lib/libraw*{sharedLibraryExtension}*",

	],

}
