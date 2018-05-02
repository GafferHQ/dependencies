{

	"downloads" : [

		"https://github.com/llvm/llvm-project/releases/download/llvmorg-15.0.7/llvm-15.0.7.src.tar.xz",
		"https://github.com/llvm/llvm-project/releases/download/llvmorg-15.0.7/clang-15.0.7.src.tar.xz",
		"https://github.com/llvm/llvm-project/releases/download/llvmorg-15.0.7/cmake-15.0.7.src.tar.xz"

	],

	"url" : "https://llvm.org",

	"license" : "LICENSE.TXT",

	"commands" : [

		"mv ../clang* tools/clang",
		"mv ../cmake* ../cmake",
		"mkdir build",
		"cd build &&"
			" cmake"
			" -DCMAKE_INSTALL_PREFIX={buildDir}"
			" -DGCC_INSTALL_PREFIX={compilerRoot}"
			" -DCMAKE_BUILD_TYPE=Release"
			" -DLLVM_ENABLE_RTTI=ON"
			" -DLLVM_ENABLE_LIBXML2=OFF"
			" -DLLVM_ENABLE_ZSTD=OFF"
			" -DLLVM_INCLUDE_BENCHMARKS=OFF"
			" -DLLVM_INCLUDE_TESTS=OFF"
			" -DLLVM_TARGETS_TO_BUILD={buildTargets}"
			" ..",
		"cd build && make install -j {jobs}"

	],

	"variables" : {

		"buildTargets" : "'Native'",

	},

	"platform:linux" : {

		"variables" : {

			"buildTargets" : "'Native;NVPTX'",

		},

	},

	"platform:windows" : {

		"variables" : {

			"cmakeGenerator" : "\"Ninja\"",

		},

		"commands" : [

			"move ..\\clang* tools\\clang",
            "patch -p1 < ..\\..\\patches\\windows\\noAST.patch.txt",
            "move ..\\cmake* ..\\cmake",
			"mkdir build",
			"cd build &&"
				" cmake"
				" -G {cmakeGenerator}"
				" -D CMAKE_INSTALL_PREFIX={buildDir}"
				" -D CMAKE_BUILD_TYPE=Release"
				" -D BUILD_SHARED_LIBS=OFF"
				" -D LLVM_REQUIRES_RTTI=ON"
				" -D LLVM_ENABLE_LIBXML2=OFF"
				" -D LLVM_ENABLE_ZSTD=OFF"
				" -D LLVM_INCLUDE_BENCHMARKS=OFF"
				" -D LLVM_INCLUDE_TESTS=OFF"
				" -D LLVM_TARGETS_TO_BUILD='Native;NVPTX'"
				" ..",
			"cd build && cmake --build . --config {cmakeBuildType} --target install",

		],

	}

}
