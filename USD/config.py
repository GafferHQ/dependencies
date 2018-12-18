{

	"downloads" : [

		"https://github.com/PixarAnimationStudios/USD/archive/refs/tags/v23.11.tar.gz"

	],

	"url" : "https://graphics.pixar.com/usd",

	"license" : "LICENSE.txt",

	"dependencies" : [ "Boost", "Python", "OpenImageIO", "TBB", "Alembic", "OpenSubdiv", "OpenVDB", "OpenShadingLanguage", "PyOpenGL", "GLEW", "PySide", "Embree", "MaterialX", "Jinja2" ],

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

		"lib/{libraryPrefix}trace{staticLibraryExtension}",
		"lib/{libraryPrefix}arch{staticLibraryExtension}",
		"lib/{libraryPrefix}tf{staticLibraryExtension}",
		"lib/{libraryPrefix}js{staticLibraryExtension}",
		"lib/{libraryPrefix}work{staticLibraryExtension}",
		"lib/{libraryPrefix}plug{staticLibraryExtension}",
		"lib/{libraryPrefix}kind{staticLibraryExtension}",
		"lib/{libraryPrefix}gf{staticLibraryExtension}",
		"lib/{libraryPrefix}vt{staticLibraryExtension}",
		"lib/{libraryPrefix}ar{staticLibraryExtension}",
		"lib/{libraryPrefix}sdf{staticLibraryExtension}",
		"lib/{libraryPrefix}pcp{staticLibraryExtension}",
		"lib/{libraryPrefix}usd*{staticLibraryExtension}",
		"lib/{libraryPrefix}ndr{staticLibraryExtension}",
		"lib/{libraryPrefix}sdr{staticLibraryExtension}",
		"lib/{libraryPrefix}hd{staticLibraryExtension}",
		"lib/{libraryPrefix}hdx{staticLibraryExtension}",
		"lib/{libraryPrefix}hdSt{staticLibraryExtension}",
		"lib/{libraryPrefix}hio{staticLibraryExtension}",
		"lib/{libraryPrefix}glf{staticLibraryExtension}",
		"lib/{libraryPrefix}garch{staticLibraryExtension}",
		"lib/{libraryPrefix}hgi{staticLibraryExtension}",
		"lib/{libraryPrefix}hgiInterop{staticLibraryExtension}",
		"lib/{libraryPrefix}hgiGL{staticLibraryExtension}",
		"lib/{libraryPrefix}hf{staticLibraryExtension}",
		"lib/{libraryPrefix}cameraUtil{staticLibraryExtension}",
		"lib/{libraryPrefix}pxOsd{staticLibraryExtension}",

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
        
		"environment" : {

			"PATH" : "{buildDir}\\bin;%PATH%",

		},

		"commands" : [

			"mkdir gafferBuild",
			"cd gafferBuild && "
				" cmake"
				" -G {cmakeGenerator}"
				" -D CMAKE_BUILD_TYPE={cmakeBuildType}"
				" -D CMAKE_INSTALL_PREFIX={buildDir}"
				" -D CMAKE_PREFIX_PATH={buildDir}"
				" -D Boost_NO_SYSTEM_PATHS=TRUE"
				" -D Boost_NO_BOOST_CMAKE=TRUE"
                " -D OIIO_BASE_DIR={buildDir}"
				" -D PXR_ENABLE_OPENVDB_SUPPORT=TRUE"
                " -D PXR_ENABLE_OSL_SUPPORT=TRUE"
				" -D PXR_ENABLE_PTEX_SUPPORT=FALSE"
				" -D PXR_BUILD_TESTS=FALSE"
                " -D PXR_BUILD_OPENIMAGEIO_PLUGIN=TRUE"
				" -D PXR_BUILD_ALEMBIC_PLUGIN=TRUE"
				" -D PXR_BUILD_EMBREE_PLUGIN=FALSE"
				" -D PXR_ENABLE_HDF5_SUPPORT=FALSE"
                " -D PXR_ENABLE_MATERIALX_SUPPORT=TRUE"
				" -D MATERIALX_STDLIB_DIR={buildDirFwd}/materialX/libraries"
                " -D PXR_USE_PYTHON_3=TRUE"
                " -D Python3_ROOT_DIR={buildDir}"
				" -D Python3_FIND_STRATEGY=LOCATION"
				" -D ALEMBIC_DIR={buildDir}\\lib"
                " -D OPENEXR_LOCATION={buildDir}\\lib"
				" -D CMAKE_CXX_FLAGS=\"-DBOOST_ALL_NO_LIB -DHAVE_SNPRINTF\""
				" ..",

			"cd gafferBuild && cmake --build . --config {cmakeBuildType} --target install -- -j {jobs}",

			"xcopy /s /e /h /y /i {buildDir}\\lib\\python\\pxr {buildDir}\\python\\pxr",

		],

	},

}
