{

	"downloads" : [

		"https://github.com/facebook/zstd/releases/download/v1.5.0/zstd-1.5.0.tar.gz"

	],

	"url" : "http://zstd.net",

	"license" : "LICENSE",

	"commands" : [

		"mkdir gafferBuild",
		"cd gafferBuild &&"
			" cmake"
			" -D CMAKE_CXX_STANDARD={c++Standard}"
			" -D CMAKE_INSTALL_PREFIX={buildDir}"
			" -D CMAKE_INSTALL_LIBDIR={buildDir}/lib"
			" -D CMAKE_BUILD_TYPE=Release"
			" -D ZSTD_BUILD_PROGRAMS=OFF"
			" -D ZSTD_BUILD_TESTS=OFF"
			" ../build/cmake",

		"cd gafferBuild && make -j {jobs} && make install",

	],

	"manifest" : [

		"include/zstd.h",
		"{sharedLibraryDir}/{libraryPrefix}zstd*{sharedLibraryExtension}*",
		"lib/{libraryPrefix}zstd*.lib",

	],
	
	"platform:windows" : {
		
		"commands" : [
			
			"mkdir gafferBuild",
			"cd gafferBuild && "
				" cmake"
				" -G {cmakeGenerator}"
				" -D CMAKE_BUILD_TYPE={cmakeBuildType}"
				" -D CMAKE_CXX_STANDARD={c++Standard}"
				" -D CMAKE_INSTALL_PREFIX={buildDir}"
				" -D CMAKE_INSTALL_LIBDIR={buildDir}/lib"
				" -D CMAKE_BUILD_TYPE=Release"
				" -D ZSTD_BUILD_PROGRAMS=OFF"
				" -D ZSTD_BUILD_TESTS=OFF"
				" ../build/cmake",

			"cd gafferBuild && cmake --build . --config {cmakeBuildType} --target install -- -j {jobs}",
		]
	}

}
