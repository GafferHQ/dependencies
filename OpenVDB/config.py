{

	"downloads" : [

		"https://github.com/AcademySoftwareFoundation/openvdb/archive/refs/tags/v10.1.0.tar.gz"

	],

	"url" : "http://www.openvdb.org",

	"license" : "LICENSE",

	"dependencies" : [ "Blosc", "TBB", "OpenEXR", "Python", "Boost", "PyBind11" ],

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
			" -D OPENVDB_BUILD_NANOVDB=ON"
			" -D OPENVDB_ENABLE_RPATH=OFF"
			" -D CONCURRENT_MALLOC=None"
			" -D PYOPENVDB_INSTALL_DIRECTORY={buildDir}/python"
			" -D Python_ROOT_DIR={buildDir}"
			" -D Python_FIND_STRATEGY=LOCATION"
			" -D Python_FIND_VERSION={pythonVersion}"
			" -D Python_FIND_VERSION_MAJOR={pythonMajorVersion}"
			" -D BOOST_ROOT={buildDir}"
			" -D Boost_NO_SYSTEM_PATHS=ON"
			" .."
		,

		"cd build && make VERBOSE=1 -j {jobs} && make install",

	],

	"manifest" : [

		"include/openvdb",
		"include/pyopenvdb.h",
		"include/nanovdb",
		"{sharedLibraryDir}/{libraryPrefix}openvdb*{sharedLibraryExtension}*",
		"lib/{libraryPrefix}openvdb*.lib",
		"python/pyopenvdb*",

	],

	"platform:windows" : {

		"variables" : {

			"cmakeGenerator" : "\"Visual Studio 17 2022\"",
		},

		"environment" : {

			"PATH" : "{buildDir}\\bin;%PATH%",

		},

		"commands" : [
			# OpenVDB requests Python 2.7 specifically but Boost doesn't add version numbers until v1.67
			"mkdir gafferBuild",
			"cd gafferBuild && "
				" cmake"
				" -Wno-dev"
				" -G {cmakeGenerator}"
				" -D CMAKE_BUILD_TYPE={cmakeBuildType}"
				" -D CMAKE_INSTALL_PREFIX={buildDir}"
				" -D OPENVDB_BUILD_UNITTESTS=OFF"
				" -D OPENVDB_BUILD_DOCS=OFF"
				" -D OPENVDB_BUILD_PYTHON_MODULE=ON"
				" -D OPENVDB_BUILD_NANOVDB=ON"
				" -D USE_GLFW3=OFF"
				" -D TBB_LOCATION={buildDir}"
				" -D BOOST_ROOT={buildDir}"
				" -D GLEW_LOCATION={buildDir}"
				" -D ILMBASE_LOCATION={buildDir}"
				" -D OPENEXR_LOCATION={buildDir}"
				" -D BLOSC_LOCATION={buildDir}"
				" -D OPENVDB_ENABLE_3_ABI_COMPATIBLE=OFF"
				" -D CMAKE_CXX_FLAGS=\"/EHsc -DHAVE_SNPRINTF\""
				" -D Python_ROOT_DIR={buildDir}"
				" -D Python_FIND_STRATEGY=LOCATION"
				" -D Python_FIND_VERSION={pythonVersion}"
				" -D Python_FIND_VERSION_MAJOR={pythonMajorVersion}"
				" ..",

			"cd gafferBuild && cmake --build . --config {cmakeBuildType} --target install --parallel {jobs}",
		],

		"postMovePaths" : {

			"{buildDir}/lib/python{pythonVersion}/site-packages/pyopenvdb*.pyd" : "{buildDir}/python",
			"{buildDir}/lib/python{pythonVersion}/site-packages/*.lib" : "{buildDir}/lib",

		}

	},

}
