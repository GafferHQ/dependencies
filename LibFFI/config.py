{

	"downloads" : [

		"https://github.com/libffi/libffi/releases/download/v3.8.0/libffi-3.8.0.tar.gz"

	],

	"url" : "https://sourceware.org/libffi/",
	"license" : "LICENSE",

	"commands" : [

		"./configure --prefix={buildDir} --libdir={buildDir}/lib --disable-multi-os-directory --without-gcc-arch",
		"make -j {jobs}",
		"make install",

	],

	"manifest" : [

		"lib/libffi*{sharedLibraryExtension}*",

	],

}
