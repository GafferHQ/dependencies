{

	"downloads" : [

		"https://github.com/blender/cycles/archive/refs/tags/v4.0.2.tar.gz",

	],

	"url" : "https://www.cycles-renderer.org/",

	"license" : "LICENSE",

	"dependencies" : [ "Boost", "OpenJPEG", "OpenImageIO", "TBB", "Alembic", "Embree", "OpenColorIO", "OpenVDB", "OpenShadingLanguage", "OpenSubdiv", "OpenPGL", "LibWebP" ],

	"commands" : [

		"mkdir build",
		"cd build &&"
			" cmake"
			" -D CMAKE_INSTALL_PREFIX={buildDir}/cycles"
			" -D CMAKE_PREFIX_PATH={buildDir}"
			" -D CMAKE_BUILD_TYPE=Release"
			" -D WITH_CYCLES_OPENIMAGEDENOISE=OFF"
			" -D WITH_CYCLES_PATH_GUIDING=ON"
			" -D WITH_CYCLES_CUDA_BINARIES=ON"
			" -D WITH_CYCLES_DEVICE_CUDA=ON"
			" -D WITH_CYCLES_DEVICE_HIP=OFF"
			" -D WITH_CYCLES_DEVICE_OPTIX=ON"
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

	"platform:windows" : {
	
		"environment" : {

			"PATH" : "{buildDir}\\bin;%PATH%",

		},

		"commands" : [

			"mkdir build",
			"cd build &&"
				" cmake"
				" -W-nodev -G {cmakeGenerator}"
				" -D CMAKE_INSTALL_PREFIX={buildDir}/cycles"
				" -D CMAKE_PREFIX_PATH={buildDir}"
				" -D CMAKE_BUILD_TYPE={cmakeBuildType}"
				" -D WITH_CYCLES_OPENIMAGEDENOISE=OFF"
				" -D WITH_CYCLES_PATH_GUIDING=ON"
				" -D WITH_CYCLES_CUDA_BINARIES=ON"
				" -D WITH_CYCLES_DEVICE_CUDA=ON"
				" -D WITH_CYCLES_DEVICE_HIP=OFF"
				" -D WITH_CYCLES_DEVICE_OPTIX=ON"
				" -D OPTIX_ROOT_DIR=\"C:/ProgramData/NVIDIA Corporation/Optix SDK 7.7.0\""
				" -D WITH_CYCLES_HYDRA_RENDER_DELEGATE=OFF"
				" -D CMAKE_POSITION_INDEPENDENT_CODE=ON"
				" -D WITH_CYCLES_USD=OFF"
				" -D TBB_ROOT_DIR={buildDirFwd}"
				" -D OPENVDB_ROOT_DIR={buildDirFwd}"
				" -D NANOVDB_INCLUDE_DIR={buildDirFwd}/include"
				" -D OPENPGL_ROOT_DIR={buildDirFwd}"
				" -D PLATFORM_BUNDLED_LIBRARY_DIRS={buildDir}\\bin;{buildDir}\\lib;%PATH%"
				" ..",
			"cd build && cmake --build . --config {cmakeBuildType} --target install VERBOSE=1 -- -j {jobs}",
			"if not exist \"{buildDir}\\cycles\\bin\" mkdir {buildDir}\\cycles\\bin",
			"if not exist \"{buildDir}\\cycles\\include\" mkdir {buildDir}\\cycles\\include",
			"copy build\\bin\\cycles.* {buildDir}\\cycles\\bin",
			"xcopy /sehyi build\\lib {buildDir}\\cycles\\lib",
			"xcopy /sehyi third_party\\atomic\\* {buildDir}\\cycles\\include",
			"xcopy /shyi src\\*.h {buildDir}\\cycles\\include",

		]

	}

}
