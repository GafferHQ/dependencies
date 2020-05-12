{

	"downloads" : [

		"https://archive.apache.org/dist/xerces/c/3/sources/xerces-c-3.2.2.tar.gz",

	],

	"url" : "https://xerces.apache.org",

	"license" : "LICENSE",

	"commands" : [

		"./configure --prefix={buildDir} --without-icu",
		"make -j {jobs}",
		"make install",

	],

	"manifest" : [

		"lib/libxerces-c*{sharedLibraryExtension}*",

	],

}
