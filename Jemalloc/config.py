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
		"lib/libjemalloc*{sharedLibraryExtension}*",

	],
}
