{

	"downloads" : [

		"https://github.com/AcademySoftwareFoundation/OpenColorIO/archive/refs/tags/v2.2.1.tar.gz",
		"https://github.com/imageworks/OpenColorIO-Configs/archive/v1.0_r2.tar.gz",

	],

	"url" : "http://opencolorio.org",

	"license" : "LICENSE",

	"dependencies" : [ "Python", "PyBind11" ],

	"environment" : {

		"PATH" : "{buildDir}/bin:$PATH",
		"LD_LIBRARY_PATH" : "{buildDir}/lib:$LD_LIBRARY_PATH",

	},

	"commands" : [

		"mkdir build",
		"cd build && cmake"
			" -D CMAKE_CXX_STANDARD={c++Standard}"
		 	" -D CMAKE_INSTALL_PREFIX={buildDir}"
			" -D CMAKE_PREFIX_PATH={buildDir}"
			" -D Python_ROOT_DIR={buildDir}"
			" -D Python_FIND_STRATEGY=LOCATION"
			" -D BUILD_SHARED_LIBS=ON"
			" -D OCIO_BUILD_APPS=OFF"
			" -D OCIO_BUILD_NUKE=OFF"
			" -D OCIO_BUILD_TESTS=OFF"
			" -D OCIO_BUILD_GPU_TESTS=OFF"
			# Will need removing when we update to OpenEXR 3
			" -D OCIO_USE_OPENEXR_HALF=ON"
			" -D OCIO_PYTHON_VERSION={pythonVersion}"
			" ..",

		"cd build && make clean && make VERBOSE=1 -j {jobs} && make install",

		"mkdir -p {buildDir}/python",
		"mv {buildDir}/lib*/python*/site-packages/PyOpenColorIO* {buildDir}/python",

		"{libCopyCommand}",

		"mkdir -p {buildDir}/openColorIO",
		"cp ../OpenColorIO-Configs-1.0_r2/nuke-default/config.ocio {buildDir}/openColorIO",
		"cp -r ../OpenColorIO-Configs-1.0_r2/nuke-default/luts {buildDir}/openColorIO",

	],

	"manifest" : [

		"include/OpenColorIO",
		"lib/libOpenColorIO*{sharedLibraryExtension}*",
		"openColorIO",
		"python/PyOpenColorIO*",

	],

	"platform:linux" : {

		"variables" : {

			"libCopyCommand" :
			# OpenColorIO's CMake setup uses GNUInstallDirs, which unhelpfully
			# puts the libraries in `lib64`. Move them back. We'd like to do this
			# in the build itself by passing `-D CMAKE_INSTALL_LIBDIR={buildDir}/lib`
			# but that breaks OpenColorIO's internal libexpat setup.
			"mv {buildDir}/lib*/libOpenColorIO* {buildDir}/lib"

		}

	},

	"platform:macos" : {

		"variables" : {

			"libCopyCommand" : "",

		}

	},

}
