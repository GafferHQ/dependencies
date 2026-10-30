{

	"downloads" : [

		"https://github.com/llvm/llvm-project/releases/download/llvmorg-20.1.8/llvm-20.1.8.src.tar.xz",
		"https://github.com/llvm/llvm-project/releases/download/llvmorg-20.1.8/clang-20.1.8.src.tar.xz",
		"https://github.com/llvm/llvm-project/releases/download/llvmorg-20.1.8/cmake-20.1.8.src.tar.xz"

	],

	"url" : "https://llvm.org",

	"license" : "LICENSE.TXT",

	"commands" : [

		"mv ../clang* tools/clang",
		"mv ../cmake* ../cmake",
		"mkdir build",
		"cd build &&"
			" cmake"
			" -DCMAKE_CXX_STANDARD={c++Standard}"
			" -DCMAKE_INSTALL_PREFIX={buildDir}"
			" -DGCC_INSTALL_PREFIX={compilerRoot}"
			" -DUSE_DEPRECATED_GCC_INSTALL_PREFIX=ON"
			" -DCMAKE_BUILD_TYPE=Release"
			" -DLLVM_ENABLE_RTTI=ON"
			" -DLLVM_ENABLE_LIBXML2=OFF"
			" -DLLVM_ENABLE_ZSTD=OFF"
			" -DLLVM_INCLUDE_BENCHMARKS=OFF"
			" -DLLVM_INCLUDE_TESTS=OFF"
			" -DLLVM_INCLUDE_UTILS=OFF"
			" -DCLANG_ENABLE_STATIC_ANALYZER=OFF"
			" -DCLANG_ENABLE_ARCMT=OFF"
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

}
