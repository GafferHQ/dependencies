{

	"downloads" : [

		"https://github.com/facebook/zstd/releases/download/v1.5.6/zstd-1.5.6.tar.gz"

	],

	"url" : "https://github.com/facebook/zstd",

	"license" : "LICENSE",

	"commands" : [

		"mkdir gafferBuild",
		"cd gafferBuild &&"
			" cmake"
			" -D CMAKE_CXX_STANDARD={c++Standard}"
			" -D CMAKE_INSTALL_PREFIX={buildDir}"
			" -D CMAKE_INSTALL_LIBDIR={buildDir}/lib"
			" -D CMAKE_PREFIX_PATH={buildDir}"
			" -D ZSTD_BUILD_PROGRAMS=OFF"
			" -D ZSTD_BUILD_SHARED=OFF"
			" -D ZSTD_BUILD_STATIC=ON"
			" -D ZSTD_BUILD_TESTS=OFF"
			" -D ZSTD_LEGACY_SUPPORT=OFF"
			" -D ZSTD_LZ4_SUPPORT=OFF"
			" -D ZSTD_LZMA_SUPPORT=OFF"
			" -D ZSTD_MULTITHREAD_SUPPORT=ON"
			" -D ZSTD_PROGRAMS_LINK_SHARED=OFF"
			" -D ZSTD_USE_STATIC_RUNTIME=OFF"
			" -D ZSTD_ZLIB_SUPPORT=OFF"
			" ../build/cmake",
		"cd gafferBuild && make install -j {jobs}",

	],

	"manifest" : [

		"include/zdict*",
		"include/zstd*",
		"lib/*zstd*",

	],

}
