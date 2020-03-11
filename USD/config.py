{

	"downloads" : [

		"https://github.com/PixarAnimationStudios/USD/archive/v20.02.tar.gz"

	],

	"license" : "LICENSE.txt",

	"dependencies" : [ "Boost", "Python", "OpenImageIO", "TBB", "Alembic" ],

	"environment" : {

		"LD_LIBRARY_PATH" : "{buildDir}/lib",

	},

	"commands" : [

		"cmake"
			" -D CMAKE_INSTALL_PREFIX={buildDir}"
			" -D CMAKE_PREFIX_PATH={buildDir}"
			" -D Boost_NO_SYSTEM_PATHS=TRUE"
			" -D Boost_NO_BOOST_CMAKE=TRUE"
			" -D PXR_BUILD_IMAGING=FALSE"
			" -D PXR_BUILD_TESTS=FALSE"
			" -D PXR_BUILD_ALEMBIC_PLUGIN=TRUE"
			" -D PXR_ENABLE_HDF5_SUPPORT=FALSE"
			" -D ALEMBIC_DIR={buildDir}/lib"
			" -D OPENEXR_LOCATION={buildDir}/lib"
			# Disable Python support until USD supports Python 3.
			" -D PXR_ENABLE_PYTHON_SUPPORT=FALSE"
			# Needed to prevent CMake picking up system python libraries on Mac.
			" -D CMAKE_FRAMEWORK_PATH={pythonLibDir}"
			" ."
		,

		"make VERBOSE=1 -j {jobs}",
		"make install",

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
		"lib/usd",

		"python/pxr",

		"share/usd",

	],

}
