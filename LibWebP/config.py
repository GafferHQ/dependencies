{

	"downloads" : [

		"https://storage.googleapis.com/downloads.webmproject.org/releases/webp/libwebp-1.3.2.tar.gz"

	],

	"url" : "https://chromium.googlesource.com/webm/libwebp",
	"license" : "COPYING",

	"commands" : [

		"./configure --prefix={buildDir}",
		"make -j {jobs}",
		"make install",

	],

	"manifest" : [

		"include/webp",
		"lib/libwebp*{sharedLibraryExtension}*",
		"lib/libwebpdemux*{sharedLibraryExtension}*",
		"lib/libsharpyuv*{sharedLibraryExtension}*",

	],

}
