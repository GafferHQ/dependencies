{

	"downloads" : [

		"https://github.com/madler/zlib/releases/download/v1.2.13/zlib-1.2.13.tar.gz",

	],

	"url" : "https://zlib.net/",

	"license" : "LICENSE",

	"commands" : [

		"mkdir build",
		"cd build &&"
			" cmake"
			" -D CMAKE_INSTALL_PREFIX={buildDir}"
			" -D CMAKE_INSTALL_LIBDIR={buildDir}/lib"
			" -D CMAKE_BUILD_TYPE=Release"
			" ..",
		"cd build && make -j {jobs} && make install",

	],

	"manifest" : [

		"include/zlib.h",
		"include/zconf.h",
		"{sharedLibraryDir}/{libraryPrefix}z*{sharedLibraryExtension}*",
		"lib/{libraryPrefix}z*.lib"

	],
    
	"platform:windows" : {

		"commands" : [

			"mkdir gafferBuild",

			"cd gafferBuild && "
				" cmake"
				" -G {cmakeGenerator}"
				" -D CMAKE_BUILD_TYPE={cmakeBuildType}"
				" -D CMAKE_INSTALL_PREFIX={buildDir}"
				" ..",

			"cd gafferBuild && cmake --build . --config {cmakeBuildType} --target install -- -j {jobs}",
			# for some reason ZLib building doesn't put zconf.h in the main source directory alongside zlib.h
			# manually copy it there so Boost can find it
			"cd gafferBuild && copy zconf.h ..",
			"copy {buildDir}\\bin\\{libraryPrefix}zlib*{sharedLibraryExtension}* {buildDir}\\lib\\",
		]

	}

}
