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
		"{sharedLibraryDir}/libwebp*{sharedLibraryExtension}*",
		"{sharedLibraryDir}/libwebpdemux*{sharedLibraryExtension}*",
		"{sharedLibraryDir}/libsharpyuv*{sharedLibraryExtension}*",
		"lib/libwebp*.lib",
		"lib/libsharpyuv*.lib",

	],

	"platform:windows" : {

		"commands" : [

			"nmake /f Makefile.vc CFG=release-dynamic RTLIBCFG=dynamic objdir=output"

		],

		"postMovePaths" :  {
			"output/release-dynamic/x64/bin/libwebp*.dll" : "{buildDir}/bin",
            "output/release-dynamic/x64/lib/libwebp*.lib" : "{buildDir}/lib",
            "output/release-dynamic/x64/bin/libsharpyuv*.dll" : "{buildDir}/bin",
            "output/release-dynamic/x64/lib/libsharpyuv*.lib" : "{buildDir}/lib",
			"sharpyuv/*.h" : "{buildDir}/include/webp/sharpyuv",
			"src/webp/*.h" : "{buildDir}/include/webp",
		},

	}

}
