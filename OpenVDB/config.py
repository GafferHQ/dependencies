{

	"downloads" : [

		"https://github.com/AcademySoftwareFoundation/openvdb/archive/refs/tags/v8.1.0.tar.gz"

	],

	"url" : "http://www.openvdb.org",

	"license" : "LICENSE",

	"dependencies" : [ "Blosc", "TBB", "OpenEXR", "Python", "Boost" ],

	"environment" : {

		"LD_LIBRARY_PATH" : "{buildDir}/lib",

	},

	"commands" : [

		"mkdir build",
		"cd build && cmake"
			" -D CMAKE_INSTALL_PREFIX={buildDir}"
			" -D CMAKE_PREFIX_PATH={buildDir}"
			# OpenVDB's CMake setup uses GNUInstallDirs, which unhelpfully
			# puts the libraries in `lib64`. Coax them back.
			" -D CMAKE_INSTALL_LIBDIR={buildDir}/lib"
			" -D OPENVDB_BUILD_PYTHON_MODULE=ON"
			" -D OPENVDB_ENABLE_RPATH=OFF"
			" -D CONCURRENT_MALLOC=None"
			" -D PYOPENVDB_INSTALL_DIRECTORY={buildDir}/python"
			" -D Python_ROOT_DIR={buildDir}"
			" -D Python_FIND_STRATEGY=LOCATION"
			" -D BOOST_ROOT={buildDir}"
			" -D Boost_NO_SYSTEM_PATHS=ON"
			" .."
		,

		"cd build && make VERBOSE=1 -j {jobs} && make install",

	],

	"manifest" : [

		"include/openvdb",
		"include/pyopenvdb.h",
		"lib/libopenvdb*{sharedLibraryExtension}*",
		"python/pyopenvdb*",

	],

}
