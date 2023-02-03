{

	"downloads" : [

		"https://github.com/jbeder/yaml-cpp/archive/refs/tags/yaml-cpp-0.7.0.tar.gz",

	],

	"url" : "https://github.com/jbeder/yaml-cpp",

	"license" : "LICENSE",

	"commands" : [

		"mkdir build",
		"cd build &&"
			" cmake"
			" -D CMAKE_INSTALL_PREFIX={buildDir}"
			" -D CMAKE_INSTALL_LIBDIR={buildDir}/lib"
			" -D CMAKE_BUILD_TYPE=Release"
			" -D BUILD_SHARED_LIBS=ON"
			" -D YAML_CPP_BUILD_TOOLS=OFF"
			" -D YAML_CPP_BUILD_TESTS=OFF"
			" -D YAML_CPP_BUILD_CONTRIB=OFF"
			" ..",
		"cd build && make -j {jobs} && make install",

	],

	"manifest" : [

		"lib/libyaml-cpp*{sharedLibraryExtension}*",

	],

}
