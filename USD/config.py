{

	"downloads" : [

		"https://github.com/PixarAnimationStudios/USD/archive/refs/tags/v23.02.tar.gz"

	],

	"url" : "https://graphics.pixar.com/usd",

	"license" : "LICENSE.txt",

	"dependencies" : [ "Boost", "Python", "OpenImageIO", "TBB", "Alembic", "OpenSubdiv", "OpenVDB", "PyOpenGL", "GLEW", "PySide", "Embree" ],

	"environment" : {

		"LD_LIBRARY_PATH" : "{buildDir}/lib",
		"PYTHONPATH" : "{buildDir}/python",

	},

	"variables" : {

		"extraArguments" : "",

	},

	"commands" : [

		"cmake"
			" -D CMAKE_INSTALL_PREFIX={buildDir}"
			" -D CMAKE_PREFIX_PATH={buildDir}"
			" -D Boost_NO_SYSTEM_PATHS=TRUE"
			" -D Boost_NO_BOOST_CMAKE=TRUE"
			" -D PXR_ENABLE_OPENVDB_SUPPORT=TRUE"
			" -D PXR_ENABLE_PTEX_SUPPORT=FALSE"
			" -D PXR_BUILD_TESTS=FALSE"
			" -D PXR_BUILD_OPENIMAGEIO_PLUGIN=TRUE"
			" -D PXR_BUILD_ALEMBIC_PLUGIN=TRUE"
			" -D PXR_BUILD_EMBREE_PLUGIN=TRUE"
			" -D PXR_ENABLE_HDF5_SUPPORT=FALSE"
			" -D PXR_PYTHON_SHEBANG='/usr/bin/env python'"
			" -D Python3_ROOT_DIR={buildDir}"
			" -D Python3_FIND_STRATEGY=LOCATION"
			" -D ALEMBIC_DIR={buildDir}/lib"
			" -D OPENEXR_LOCATION={buildDir}/lib"
			# Needed to prevent CMake picking up system python libraries on Mac.
			" -D CMAKE_FRAMEWORK_PATH={pythonLibDir}"
			" {extraArguments}"
			" ."
		,

		"make VERBOSE=1 -j {jobs}",
		"make install",

		"rm -rf {buildDir}/python/pxr",
		"mv {buildDir}/lib/python/pxr {buildDir}/python",

	],

	"manifest" : [

		"bin/usd*",
		"bin/sdfdump",

		"include/pxr",

		"lib/libtrace{sharedLibraryExtension}",
		"lib/libarch{sharedLibraryExtension}",
		"lib/libtf{sharedLibraryExtension}",
		"lib/libjs{sharedLibraryExtension}",
		"lib/libwork{sharedLibraryExtension}",
		"lib/libplug{sharedLibraryExtension}",
		"lib/libkind{sharedLibraryExtension}",
		"lib/libgf{sharedLibraryExtension}",
		"lib/libvt{sharedLibraryExtension}",
		"lib/libar{sharedLibraryExtension}",
		"lib/libsdf{sharedLibraryExtension}",
		"lib/libpcp{sharedLibraryExtension}",
		"lib/libusd*{sharedLibraryExtension}",
		"lib/libndr{sharedLibraryExtension}",
		"lib/libsdr{sharedLibraryExtension}",
		"lib/libhd{sharedLibraryExtension}",
		"lib/libhdx{sharedLibraryExtension}",
		"lib/libhdSt{sharedLibraryExtension}",
		"lib/libhio{sharedLibraryExtension}",
		"lib/libglf{sharedLibraryExtension}",
		"lib/libgarch{sharedLibraryExtension}",
		"lib/libhgi{sharedLibraryExtension}",
		"lib/libhgiInterop{sharedLibraryExtension}",
		"lib/libhgiGL{sharedLibraryExtension}",
		"lib/libhf{sharedLibraryExtension}",
		"lib/libcameraUtil{sharedLibraryExtension}",
		"lib/libpxOsd{sharedLibraryExtension}",
		"lib/usd",

		"python/pxr",

		"plugin/usd",
		"share/usd",

	],

	"variant:Python:3" : {

		"variables" : {

			"extraArguments" : "-D PXR_USE_PYTHON_3=TRUE",

		},

	},

}
