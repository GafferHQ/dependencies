{

	"downloads" : [

		"https://github.com/AcademySoftwareFoundation/openvdb/archive/v6.0.0.tar.gz"

	],

	"license" : "LICENSE",

	"dependencies" : [ "Blosc", "TBB", "OpenEXR", "Python" ],

	"variables" : {

		"pythonVersion" : "2.7",

	},

	"commands" : [

		"cd openvdb && make install"
			" -j {jobs} "
			" DESTDIR={buildDir}"
			" BOOST_INCL_DIR={buildDir}/include"
			" BOOST_LIB_DIR={buildDir}/lib"
			" BOOST_PYTHON_LIB_DIR={buildDir}/lib"
			" BOOST_PYTHON_LIB=-lboost_python"
			" EXR_INCL_DIR={buildDir}/include"
			" EXR_LIB_DIR={buildDir}/lib"
			" TBB_INCL_DIR={buildDir}/include"
			" TBB_LIB_DIR={buildDir}/lib"
			" PYTHON_VERSION={pythonVersion}"
			" PYTHON_INCL_DIR={pythonIncludeDir}"
			" PYTHON_LIB_DIR={pythonLibDir}"
			" BLOSC_INCL_DIR={buildDir}/include"
			" BLOSC_LIB_DIR={buildDir}/lib"
			" NUMPY_INCL_DIR="
			" CONCURRENT_MALLOC_LIB="
			" GLFW_INCL_DIR="
			" LOG4CPLUS_INCL_DIR="
			" EPYDOC=",

		"mv {buildDir}/python/lib/python{pythonVersion}/pyopenvdb.so {buildDir}/python",
		"mv {buildDir}/python/include/python{pythonVersion}/pyopenvdb.h {buildDir}/include",

	],

	"platform:linux" : {

		"variables" : {

			"pythonIncludeDir" : "{buildDir}/include/python{pythonVersion}",
			"pythonLibDir" : "{buildDir}/lib",

		},

	},

	"platform:osx" : {

		"variables" : {

			"pythonIncludeDir" : "{buildDir}/lib/Python.framework/Headers",
			"pythonLibDir" : "{buildDir}/lib/Python.framework/Versions/{pythonVersion}",

		},

	},

}
