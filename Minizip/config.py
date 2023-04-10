{

	"downloads" : [

		"https://github.com/zlib-ng/minizip-ng/archive/refs/tags/3.0.9.tar.gz",

	],

	"url" : "https://github.com/zlib-ng/minizip-ng",

	"license" : "LICENSE",

	"commands" : [

		"mkdir build",
		"cd build &&"
			" cmake"
			" -D CMAKE_INSTALL_PREFIX={buildDir}"
			" -D CMAKE_INSTALL_LIBDIR={buildDir}/lib"
			" -D CMAKE_BUILD_TYPE=Release"
			" -D BUILD_SHARED_LIBS=OFF"
			# Arguments taken from OpenColorIO's
			# `share/cmake/modules/Findminizip-ng.cmake`.
			" -D MZ_OPENSSL=OFF"
			" -D MZ_LIBBSD=OFF"
			" -D MZ_BUILD_TESTS=OFF"
			" -D MZ_COMPAT=OFF"
			" -D MZ_BZIP2=OFF"
			" -D MZ_LZMA=OFF"
			" -D MZ_LIBCOMP=OFF"
			" -D MZ_ZSTD=OFF"
			" -D MZ_PKCRYPT=OFF"
			" -D MZ_WZAES=OFF"
			" -D MZ_SIGNING=OFF"
			" -D MZ_ZLIB=ON"
			" -D MZ_ICONV=OFF"
			" -D MZ_FETCH_LIBS=OFF"
			" -D MZ_FORCE_FETCH_LIBS=OFF"
			" ..",
		"cd build && make -j {jobs} && make install",

	],

	"manifest" : [

		# We're only building static libraries for OpenColorIO
		# to use, so don't need to package anything.

	],
    
	"platform:windows" : {

		"commands" : [

			"mkdir build",
			"cd build &&"
				" cmake"
                " -G {cmakeGenerator}"
				" -D CMAKE_INSTALL_PREFIX={buildDir}"
				" -D CMAKE_INSTALL_LIBDIR={buildDir}/lib"
				" -D CMAKE_BUILD_TYPE=Release"
				" -D BUILD_SHARED_LIBS=OFF"
				# Arguments taken from OpenColorIO's
				# `share/cmake/modules/Findminizip-ng.cmake`.
				" -D MZ_OPENSSL=OFF"
				" -D MZ_LIBBSD=OFF"
				" -D MZ_BUILD_TESTS=OFF"
				" -D MZ_COMPAT=OFF"
				" -D MZ_BZIP2=OFF"
				" -D MZ_LZMA=OFF"
				" -D MZ_LIBCOMP=OFF"
				" -D MZ_ZSTD=OFF"
				" -D MZ_PKCRYPT=OFF"
				" -D MZ_WZAES=OFF"
				" -D MZ_SIGNING=OFF"
				" -D MZ_ZLIB=ON"
				" -D MZ_ICONV=OFF"
				" -D MZ_FETCH_LIBS=OFF"
				" -D MZ_FORCE_FETCH_LIBS=OFF"
				" ..",
			"cd build && cmake --build . --config {cmakeBuildType} --target install -- -j {jobs}",

		]
	},

}
