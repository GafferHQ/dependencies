{

	"downloads" : [

		"https://github.com/blender/cycles/archive/refs/tags/v4.4.0.tar.gz",

	],

	"url" : "https://www.cycles-renderer.org/",

	"license" : "LICENSE",

	"dependencies" : [ "Boost", "OpenJPEG", "OpenImageIO", "TBB", "Alembic", "Embree", "OpenColorIO", "OpenVDB", "OpenShadingLanguage", "OpenSubdiv", "OpenPGL", "LibWebP", "Zstandard", "Optix" ],

	"environment" : {

		# Needed because the build process runs oslc.
		"DYLD_FALLBACK_LIBRARY_PATH" : "{buildDir}/lib",

	},

	"commands" : [

		# The Cycles archive includes empty folders under `./lib`
		# named `{platform}_{architecture}`. The existence of a folder
		# in lib matching the current platform and architecture causes
		# the build to only look for dependencies within it, so we
		# remove them to allow dependencies to be found in `{buildDir}`.
		"rm -r ./lib/*",

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

			"rmdir lib\\windows_x64",
			"mkdir build",
			"cd build &&"
				" cmake"
				" -W-nodev -G {cmakeGenerator}"
				" -D CMAKE_CXX_COMPILER={cmakeCompiler}"
				" -D CMAKE_C_COMPILER={cmakeCompiler}"
				" -D CMAKE_INSTALL_PREFIX={buildDir}/cycles"
				" -D CMAKE_PREFIX_PATH={buildDir}"
				" -D CMAKE_BUILD_TYPE={cmakeBuildType}"
				" -D WITH_CYCLES_OPENIMAGEDENOISE=OFF"
				" -D WITH_CYCLES_PATH_GUIDING=ON"
				" -D WITH_CYCLES_CUDA_BINARIES=ON"
				" -D WITH_CYCLES_DEVICE_CUDA=ON"
				" -D WITH_CYCLES_DEVICE_HIP=OFF"
				" -D WITH_CYCLES_DEVICE_OPTIX=ON"
				" -D OPTIX_ROOT_DIR={buildDir}"
				" -D CUDA_TOOLKIT_ROOT_DIR=\"%CUDA_TOOLKIT_ROOT_DIR%\""
				" -D CMAKE_POSITION_INDEPENDENT_CODE=ON"
				" -D TBB_ROOT_DIR={buildDir}"
				" -D OPENVDB_ROOT_DIR={buildDir}"
				" -D NANOVDB_INCLUDE_DIR={buildDir}/include"
				" -D OPENPGL_ROOT_DIR={buildDir}"
				" -D JPEG_ROOT_DIR={buildDir}"
				" -D WITH_CYCLES_HYDRA_RENDER_DELEGATE=OFF"
				" -D WITH_CYCLES_USD=OFF"
				" -D PLATFORM_BUNDLED_LIBRARY_DIRS={buildDir}\\bin;{buildDir}\\lib;%PATH%"
				" ..",
			"cd build && cmake --build . --config {cmakeBuildType} --target install",
			# Copy headers to their own location for `postMovePaths`. We don't copy to `buildDir`
			# because the `/`s in `buildDir` are interpreted by `xcopy` as switches and it gets
			# confused.
			"xcopy /syi src\*.h include",
		],

		"postMovePaths" : {

			"build/bin/cycles.*" : "{buildDir}/cycles/bin",
			"build/lib/*" : "{buildDir}/cycles/lib",
			"third_party/atomic/*" : "{buildDir}/cycles/include",
			"include" : "{buildDir}/cycles"

		}

	}

}
