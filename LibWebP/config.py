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
		"lib/libwebp{sharedLibraryExtension}*",
		"lib/libwebpdemux{sharedLibraryExtension}*",
		"lib/libsharpyuv{sharedLibraryExtension}*",

	],

	"platform:windows" : {

		"manifest" : [

			"include/webp",
			"bin/libwebp{sharedLibraryExtension}*",
			"bin/libwebpdemux{sharedLibraryExtension}*",
			"bin/libsharpyuv{sharedLibraryExtension}*",
			"lib/libwebp.lib",
			"lib/libwebpdemux.lib",
			"lib/libsharpyuv.lib",

		],

		"commands" : [

			"mkdir gafferBuild",
			"cd gafferBuild && cmake"
				" -G {cmakeGenerator}"
				" -D CMAKE_INSTALL_PREFIX={buildDir}"
				" -D CMAKE_BUILD_TYPE={cmakeBuildType}"
				" -D BUILD_SHARED_LIBS=ON"
				" ..",
			"cd gafferBuild && cmake --build . --config {cmakeBuildType} --target install",
		]

	}

}
