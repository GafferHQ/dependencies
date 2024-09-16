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

			"mkdir gafferBuild",
			"cd gafferBuild && "
				" cmake"
				" -G {cmakeGenerator}"
				" -D CMAKE_BUILD_TYPE={cmakeBuildType}"
				" -D CMAKE_INSTALL_PREFIX={buildDir}"
				" -D ZLIB_INCLUDE_DIR={buildDir}\\include"
				" -D BUILD_SHARED_LIBS=1"
				" ..",

			"cd gafferBuild && cmake --build . --config {cmakeBuildType} --target install -- -j {jobs}",

		],

	}

}
