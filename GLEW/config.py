{

	"downloads" : [

		"https://downloads.sourceforge.net/project/glew/glew/2.1.0/glew-2.1.0.tgz"

	],

	"url" : "http://glew.sourceforge.net",
	"license" : "LICENSE.txt",

	"commands" : [

		"mkdir -p {buildDir}/lib64/pkgconfig",
		"make clean && make -j {jobs} install GLEW_DEST={buildDir} LIBDIR={buildDir}/lib",

	],

	"manifest" : [

		"include/GL",
		"{sharedLibraryDir}/{libraryPrefix}GLEW*{sharedLibraryExtension}*",
		"lib/{libraryPrefix}GLEW*.lib",

	],
	"platform:windows" : {

		"commands" : [

			"if not exist \"build\" mkdir build",
			"cd build &&"
				" cmake"
				" -Wno-dev"
				" -G {cmakeGenerator}"
				" -DCMAKE_INSTALL_PREFIX={buildDir}"
				" ..\\build\\cmake",
			"cd build && cmake --build . --config {cmakeBuildType} --target install -- -j {jobs}",
		]

	}

}
