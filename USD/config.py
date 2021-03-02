{

	"downloads" : [

		"https://github.com/PixarAnimationStudios/USD/archive/refs/tags/v25.05.01.tar.gz"

	],

	"url" : "https://graphics.pixar.com/usd",

	"license" : "LICENSE.txt",

	"dependencies" : [ "Boost", "Python", "OpenImageIO", "TBB", "Alembic", "OpenSubdiv", "OpenVDB", "OpenShadingLanguage", "PyOpenGL", "GLEW", "PySide", "Embree", "MaterialX", "Jinja2" ],

	"environment" : {

		"LD_LIBRARY_PATH" : "{buildDir}/lib",
		"DYLD_FALLBACK_LIBRARY_PATH" : "{buildDir}/lib",
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
			" -D PXR_ENABLE_OSL_SUPPORT=TRUE"
			" -D PXR_ENABLE_PTEX_SUPPORT=FALSE"
			" -D PXR_BUILD_TESTS=FALSE"
			" -D PXR_BUILD_OPENIMAGEIO_PLUGIN=TRUE"
			" -D PXR_BUILD_ALEMBIC_PLUGIN=TRUE"
			" -D PXR_BUILD_EMBREE_PLUGIN=FALSE"
			" -D PXR_ENABLE_HDF5_SUPPORT=FALSE"
			" -D PXR_ENABLE_MATERIALX_SUPPORT=TRUE"
			" -D MATERIALX_STDLIB_DIR={buildDir}/materialX"
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
		"bin/sdfdump{executableExtension}",

		"include/pxr",

		# lib prefix is accurate for all platforms
		"lib/{libraryPrefix}trace{sharedLibraryExtension}",
		"lib/{libraryPrefix}arch{sharedLibraryExtension}",
		"lib/{libraryPrefix}tf{sharedLibraryExtension}",
		"lib/{libraryPrefix}js{sharedLibraryExtension}",
		"lib/{libraryPrefix}work{sharedLibraryExtension}",
		"lib/{libraryPrefix}plug{sharedLibraryExtension}",
		"lib/{libraryPrefix}kind{sharedLibraryExtension}",
		"lib/{libraryPrefix}gf{sharedLibraryExtension}",
		"lib/{libraryPrefix}vt{sharedLibraryExtension}",
		"lib/{libraryPrefix}ar{sharedLibraryExtension}",
		"lib/{libraryPrefix}sdf{sharedLibraryExtension}",
		"lib/{libraryPrefix}pcp{sharedLibraryExtension}",
		"lib/{libraryPrefix}usd*{sharedLibraryExtension}",
		"lib/{libraryPrefix}ndr{sharedLibraryExtension}",
		"lib/{libraryPrefix}sdr{sharedLibraryExtension}",
		"lib/{libraryPrefix}hd{sharedLibraryExtension}",
		"lib/{libraryPrefix}hdx{sharedLibraryExtension}",
		"lib/{libraryPrefix}hdSt{sharedLibraryExtension}",
		"lib/{libraryPrefix}hio{sharedLibraryExtension}",
		"lib/{libraryPrefix}glf{sharedLibraryExtension}",
		"lib/{libraryPrefix}garch{sharedLibraryExtension}",
		"lib/{libraryPrefix}hgi{sharedLibraryExtension}",
		"lib/{libraryPrefix}hgiInterop{sharedLibraryExtension}",
		"lib/{libraryPrefix}hgiGL{sharedLibraryExtension}",
		"lib/{libraryPrefix}hf{sharedLibraryExtension}",
		"lib/{libraryPrefix}cameraUtil{sharedLibraryExtension}",
		"lib/{libraryPrefix}pxOsd{sharedLibraryExtension}",

		"lib/{libraryPrefix}trace.lib",
		"lib/{libraryPrefix}arch.lib",
		"lib/{libraryPrefix}tf.lib",
		"lib/{libraryPrefix}js.lib",
		"lib/{libraryPrefix}work.lib",
		"lib/{libraryPrefix}plug.lib",
		"lib/{libraryPrefix}kind.lib",
		"lib/{libraryPrefix}gf.lib",
		"lib/{libraryPrefix}vt.lib",
		"lib/{libraryPrefix}ar.lib",
		"lib/{libraryPrefix}sdf.lib",
		"lib/{libraryPrefix}pcp.lib",
		"lib/{libraryPrefix}usd*.lib",
		"lib/{libraryPrefix}ndr.lib",
		"lib/{libraryPrefix}sdr.lib",
		"lib/{libraryPrefix}hd.lib",
		"lib/{libraryPrefix}hdx.lib",
		"lib/{libraryPrefix}hdSt.lib",
		"lib/{libraryPrefix}hio.lib",
		"lib/{libraryPrefix}glf.lib",
		"lib/{libraryPrefix}garch.lib",
		"lib/{libraryPrefix}hgi.lib",
		"lib/{libraryPrefix}hgiInterop.lib",
		"lib/{libraryPrefix}hgiGL.lib",
		"lib/{libraryPrefix}hf.lib",
		"lib/{libraryPrefix}cameraUtil.lib",
		"lib/{libraryPrefix}pxOsd.lib",

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

	"platform:windows" : {
        
		# "variables" : {
        #     "cmakeGenerator" : "\"NMake Makefiles JOM\"",
            
		# },

		"environment" : {

			"PATH" : "%PATH%",
			"PXR_PLUGINPATH_NAME" : "{buildDir}/bin/usd",

		},

		"commands" : [

			"mkdir gafferBuild",
			"cd gafferBuild && "
				" cmake"
				" -G {cmakeGenerator}"
				" -D CMAKE_BUILD_TYPE={cmakeBuildType}"
				" -D CMAKE_INSTALL_PREFIX={buildDir}"
				" -D CMAKE_PREFIX_PATH={buildDir}"
				" -D PXR_INSTALL_LOCATION={buildDir}/bin/usd"
				" -D Boost_NO_SYSTEM_PATHS=TRUE"
				" -D Boost_NO_BOOST_CMAKE=TRUE"
				" -D PXR_ENABLE_OPENVDB_SUPPORT=TRUE"
				" -D PXR_ENABLE_OSL_SUPPORT=TRUE"
				" -D PXR_ENABLE_PTEX_SUPPORT=FALSE"
				" -D PXR_BUILD_TESTS=FALSE"
				" -D PXR_BUILD_OPENIMAGEIO_PLUGIN=TRUE"
				" -D PXR_BUILD_ALEMBIC_PLUGIN=TRUE"
				" -D PXR_BUILD_EMBREE_PLUGIN=FALSE"
				" -D PXR_ENABLE_HDF5_SUPPORT=FALSE"
				" -D PXR_ENABLE_MATERIALX_SUPPORT=TRUE"
				" -D MATERIALX_STDLIB_DIR={buildDir}/materialX"
				" -D Python3_ROOT_DIR={buildDir}"
				" -D Python3_FIND_STRATEGY=LOCATION"
				" -D ALEMBIC_DIR={buildDir}\\lib"
				" -D OPENEXR_LOCATION={buildDir}\\lib"
				" -D PXR_USE_PYTHON_3=TRUE"
				" -D OIIO_BASE_DIR={buildDir}"
				" -D PXR_ENABLE_PRECOMPILED_HEADERS=0"
				" -D PYSIDEUICBINARY={buildDir}/bin/uic.exe"
				" -D PYSIDEUIC_EXTRA_ARGS=\"-g python\""
				" -D CMAKE_CXX_FLAGS=\"-DBOOST_ALL_NO_LIB -DHAVE_SNPRINTF\""
				" ..",

			"cd gafferBuild && cmake --build . --config {cmakeBuildType} --target install -- -j {jobs}",

		],

		"postMovePaths" : {

			"{buildDir}/lib/python/pxr" : "{buildDir}/python",

		}

	},

}
