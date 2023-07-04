{

	"downloads" : [

		"https://github.com/blender/cycles/archive/refs/tags/v3.5.0.tar.gz",

	],

	"url" : "https://www.cycles-renderer.org/",

	"license" : "LICENSE",

	"dependencies" : [ "Boost", "OpenJPEG", "OpenImageIO", "TBB", "Alembic", "Embree", "OpenColorIO", "OpenVDB", "OpenShadingLanguage", "OpenSubdiv", "OpenPGL", "OpenImageDenoise" ],

	"commands" : [

		"mkdir build",
		"cd build &&"
			" cmake"
			" -D CMAKE_INSTALL_PREFIX={buildDir}/cycles"
			" -D CMAKE_PREFIX_PATH={buildDir}"
			" -D CMAKE_BUILD_TYPE=Release"
			" -D WITH_CYCLES_OPENIMAGEDENOISE=ON"
			" -D WITH_CYCLES_PATH_GUIDING=ON"
			" -D WITH_CYCLES_DEVICE_CUDA=OFF"
			" -D WITH_CYCLES_DEVICE_OPTIX=OFF"
			" -D WITH_CYCLES_HYDRA_RENDER_DELEGATE=OFF"
			" -D CMAKE_POSITION_INDEPENDENT_CODE=ON"
			" -D WITH_CYCLES_USD=OFF"
			" ..",
		"cd build && make install -j {jobs} VERBOSE=1",

		"mkdir -p {buildDir}/cycles/include",
		"cd src && find . -name '*.h' | cpio -pdm {buildDir}/cycles/include",
		"cp -r third_party/atomic/* {buildDir}/cycles/include",
		"mkdir -p {buildDir}/cycles/bin",
		"mv {buildDir}/cycles/cycles {buildDir}/cycles/bin/cycles",
		"cp -r build/lib {buildDir}/cycles",

	],

	"manifest" : [

		"cycles",

	],

}
