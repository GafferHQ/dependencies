{

	"downloads" : [

		"https://github.com/AcademySoftwareFoundation/OpenColorIO/archive/refs/tags/v2.2.1.tar.gz",
		"https://github.com/imageworks/OpenColorIO-Configs/archive/v1.0_r2.tar.gz",

	],

	"url" : "http://opencolorio.org",

	"license" : "LICENSE",

	"dependencies" : [ "Python", "PyBind11", "OpenEXR", "Expat", "YAML-CPP", "PyString", "ZLib", "Minizip" ],

	"environment" : {

		"PATH" : "{buildDir}/bin:$PATH",
		"LD_LIBRARY_PATH" : "{buildDir}/lib:$LD_LIBRARY_PATH",

	},

	"commands" : [

		"mkdir build",
		"cd build && cmake"
			" -D CMAKE_CXX_STANDARD={c++Standard}"
		 	" -D CMAKE_INSTALL_PREFIX={buildDir}"
			" -D CMAKE_INSTALL_LIBDIR={buildDir}/lib"
			" -D CMAKE_PREFIX_PATH={buildDir}"
			" -D Python_ROOT_DIR={buildDir}"
			" -D Python_FIND_STRATEGY=LOCATION"
			" -D pystring_INCLUDE_DIR={buildDir}/include"
			" -D BUILD_SHARED_LIBS=ON"
			" -D OCIO_INSTALL_EXT_PACKAGES=NONE"
			" -D OCIO_BUILD_APPS=OFF"
			" -D OCIO_BUILD_NUKE=OFF"
			" -D OCIO_BUILD_TESTS=OFF"
			" -D OCIO_BUILD_GPU_TESTS=OFF"
			" -D OCIO_PYTHON_VERSION={pythonVersion}"
			" ..",

		"cd build && make clean && make VERBOSE=1 -j {jobs} && make install",

		"mkdir -p {buildDir}/python",
		"mv {buildDir}/lib*/python*/site-packages/PyOpenColorIO* {buildDir}/python",

		"mkdir -p {buildDir}/openColorIO",
		"cp ../OpenColorIO-Configs-1.0_r2/nuke-default/config.ocio {buildDir}/openColorIO",
		"cp -r ../OpenColorIO-Configs-1.0_r2/nuke-default/luts {buildDir}/openColorIO",

	],

	"manifest" : [

		"include/OpenColorIO",
		"{sharedLibraryDir}/{libraryPrefix}OpenColorIO*{sharedLibraryExtension}*",
		"lib/{libraryPrefix}OpenColorIO*.lib",
		"openColorIO",
		"python/PyOpenColorIO*",

	],

	"platform:windows" : {

		"variables" : {

			"cmakeBuildType" : "Release",
			"cmakeGenerator": "\"Visual Studio 17 2022\"",

		},

		"environment" : {

			"PATH" : "{buildDir}\\bin;%PATH%;%ROOT_DIR%\\winbuild\\patch\\bin",

		},

		"commands" : [

			#OCIO is particular about needing all slashes to be forward slashes
			"mkdir build",
			"cd build && cmake"
				" -Wno-dev"
				" -G {cmakeGenerator}"
				" -D CMAKE_BUILD_TYPE={cmakeBuildType}"
				" -D CMAKE_CXX_STANDARD={c++Standard}"
				" -D CMAKE_INSTALL_PREFIX={buildDir}"
				" -D CMAKE_INSTALL_LIBDIR={buildDir}/lib"
				" -D CMAKE_PREFIX_PATH={buildDir}"
				" -D Python_ROOT_DIR={buildDir}"
				" -D Python_FIND_STRATEGY=LOCATION"
				" -D pystring_INCLUDE_DIR={buildDir}/include"
				" -D BUILD_SHARED_LIBS=ON"
				" -D OCIO_INSTALL_EXT_PACKAGES=NONE"
				" -D OCIO_BUILD_APPS=OFF"
				" -D OCIO_BUILD_NUKE=OFF"
				" -D OCIO_BUILD_TESTS=OFF"
				" -D OCIO_BUILD_GPU_TESTS=OFF"
				" -D OCIO_PYTHON_VERSION={pythonVersion}"
				" ..",
			"cd build && cmake --build . --config {cmakeBuildType} --target install",
		],

		"postMovePaths" : {

			"../OpenColorIO-Configs-1.0_r2/nuke-default/config.ocio" : "{buildDir}/openColorIO",
			"../OpenColorIO-Configs-1.0_r2/nuke-default/luts" : "{buildDir}/openColorIO",

		}

	},
}
