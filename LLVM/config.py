{

	"downloads" : [

		"http://releases.llvm.org/5.0.1/llvm-5.0.1.src.tar.xz",
		"http://releases.llvm.org/5.0.1/cfe-5.0.1.src.tar.xz"

	],

	"license" : "LICENSE.TXT",

	"commands" : [

		"echo $BUILD_DIR",
		"mv ../cfe* tools/clang",
		"mkdir build",
		"cd build &&"
		 	" cmake"
		 	" -DCMAKE_INSTALL_PREFIX=$BUILD_DIR"
		 	" -DCMAKE_BUILD_TYPE=Release"
			" -DLLVM_ENABLE_RTTI=ON"
		 	" ..",
		"cd build && make install -j `getconf _NPROCESSORS_ONLN`"

	],

}
