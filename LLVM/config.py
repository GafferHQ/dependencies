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
		"cd build && make install -j {jobs}"

	],

}
