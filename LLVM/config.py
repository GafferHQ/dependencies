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

}
