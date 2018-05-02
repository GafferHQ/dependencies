{

	"downloads" : [

		"https://github.com/llvm/llvm-project/releases/download/llvmorg-11.1.0/llvm-11.1.0.src.tar.xz",
		"https://github.com/llvm/llvm-project/releases/download/llvmorg-11.1.0/clang-11.1.0.src.tar.xz"

	],

	"url" : "https://llvm.org",

	"license" : "LICENSE.TXT",

	"commands" : [

		"mv ../clang* tools/clang",
		"mkdir build",
		"cd build &&"
			" cmake"
			" -DCMAKE_INSTALL_PREFIX={buildDir}"
			" -DGCC_INSTALL_PREFIX={compilerRoot}"
			" -DCMAKE_BUILD_TYPE=Release"
			" -DLLVM_ENABLE_RTTI=ON"
			" -DLLVM_ENABLE_LIBXML2=OFF"
			" ..",
		"cd build && cmake --build . --config {cmakeBuildType} --target install -- -j {jobs}"

	],

	"platform:windows" : {

		"commands" : [

			"move ..\\clang* tools\\clang",
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
			"cd build && cmake --build . --config {cmakeBuildType} --target install -- -j {jobs}",

		],

	}

}
