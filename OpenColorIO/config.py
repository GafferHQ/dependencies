{

	"downloads" : [

		"https://github.com/AcademySoftwareFoundation/OpenColorIO/archive/refs/tags/v2.2.1.tar.gz",

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

	],

	"manifest" : [

		"include/OpenColorIO",
		"lib/libOpenColorIO*{sharedLibraryExtension}*",
		"python/PyOpenColorIO*",

	],

}
