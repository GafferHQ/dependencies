{

	"downloads" : [

		"https://github.com/imageworks/pystring/archive/refs/tags/v1.1.4.tar.gz",

	],

	"url" : "https://github.com/imageworks/pystring",

	"license" : "LICENSE",

	"commands" : [

		"mkdir build",
		"cd build &&"
			" cmake"
			" -D CMAKE_INSTALL_PREFIX={buildDir}"
			" -D CMAKE_INSTALL_LIBDIR={buildDir}/lib"
			" -D BUILD_SHARED_LIBS=ON"
			" ..",
		"cd build && make -j {jobs} && make install",
		"mkdir -p {buildDir}/include/pystring && cp pystring.h {buildDir}/include/pystring",

	],

	"manifest" : [

		"lib/{libraryPrefix}pystring*{sharedLibraryExtension}*",
        "lib/{libraryPrefix}pystring*.lib",

	],

	"platform:windows" : {
		
		"commands" : [

			"mkdir build",
			"cd build &&"
				" cmake"
                " -G {cmakeGenerator}"
				" -D CMAKE_INSTALL_PREFIX={buildDir}"
				" -D CMAKE_INSTALL_LIBDIR={buildDir}/lib"
                " -D CMAKE_BUILD_TYPE={cmakeBuildType}"
				" -D BUILD_SHARED_LIBS=ON"
				" ..",
			"cd build && cmake --build . --config {cmakeBuildType} --target install -- -j {jobs}",
            "copy {buildDir}\\bin\\pystring.dll {buildDir}\\lib\\",

		],

	},

}
