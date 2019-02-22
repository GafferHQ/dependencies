{

	"downloads" : [

		"http://releases.llvm.org/5.0.1/llvm-5.0.1.src.tar.xz",
		"http://releases.llvm.org/5.0.1/cfe-5.0.1.src.tar.xz"

	],

	"license" : "LICENSE.TXT",

	"commands" : [

		"mv ../cfe* tools/clang",
		"mkdir build",
		"cd build &&"
			" cmake"
			" -G {cmakeGenerator}"
			" -D CMAKE_INSTALL_PREFIX={buildDir}"
			" -D CMAKE_BUILD_TYPE={cmakeBuildType}"
			" -D LLVM_ENABLE_RTTI=ON"
			" ..",
		"cd build && cmake --build . --config {cmakeBuildType} --target install -- -j {jobs}"

	],

	"platform:windows" : {

		"commands" : [

			"move ..\\cfe* tools\\clang",
			"mkdir build",
			"cd build &&"
				" cmake"
				" -G {cmakeGenerator}"
				" -D CMAKE_INSTALL_PREFIX={buildDir}"
				" -D CMAKE_BUILD_TYPE={cmakeBuildType}"
				" -D BUILD_SHARED_LIBS=OFF"
				" -D LLVM_REQUIRES_RTTI=ON"
				" -D LLVM_TARGETS_TO_BUILD=\"X86\""
				" ..",
			"cd build && cmake --build . --config {cmakeBuildType} --target install -- -j {jobs}"

		],

	}

}
