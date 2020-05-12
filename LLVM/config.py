{

	"downloads" : [

		"http://releases.llvm.org/5.0.1/llvm-5.0.1.src.tar.xz",
		"http://releases.llvm.org/5.0.1/cfe-5.0.1.src.tar.xz"

	],

	"url" : "https://llvm.org",

	"license" : "LICENSE.TXT",

	"commands" : [

		"mv ../cfe* tools/clang",
		"mkdir build",
		"cd build &&"
			" cmake"
			" -DCMAKE_INSTALL_PREFIX={buildDir}"
			" -DCMAKE_BUILD_TYPE=Release"
			" -DLLVM_ENABLE_RTTI=ON"
			" ..",
		"cd build && make install -j {jobs}"

	],

}
