{

	"downloads" : [
	
		"https://github.com/materialx/MaterialX/releases/download/v1.38.0/MaterialX-1.38.0.tar.gz",
	
	],

	"url" : "https://www.materialx.org/",

	"license" : "LICENSE.txt",

	"dependencies" : [ "OpenColorIO", "OpenImageIO", "OpenShadingLanguage", "PugiXML", "PyBind11", "Python", "GLEW" ],

	"environment" : {

		# Needed because the build process runs oslc, which
		# needs to link to the OIIO libraries.
		"DYLD_FALLBACK_LIBRARY_PATH" : "{buildDir}/lib",
		"LD_LIBRARY_PATH" : "{buildDir}/lib",
		"PYTHONPATH" : "{buildDir}/python",

	},

	"commands" : [

		"cmake"
			
			" -D CMAKE_INSTALL_PREFIX={buildDir}"
			" -D CMAKE_PREFIX_PATH={buildDir}"

			####################################################
			# UNCOMMENT TO USE MATERIALX PACKAGED PYBIND11     #
			####################################################
			" -D MATERIALX_PYTHON_PYBIND11_DIR=source/PyMaterialX/PyBind11"
			####################################################

			####################################################
			# UNCOMMENT TO USE GAFFER DEPENDENCIES PYBIND11    #
			####################################################
			#" -D CMAKE_MODULE_PATH={buildDir}/share/cmake/pybind11"
			####################################################

			" -D MATERIALX_BUILD_SHARED_LIBS=TRUE"
			" -D MATERIALX_BUILD_GEN_MDL=FALSE"
			
			" -D MATERIALX_BUILD_PYTHON=TRUE"
			" -D MATERIALX_PYTHON_EXECUTABLE={buildDir}/bin/python"
			" -D MATERIALX_PYTHON_OCIO_DIR={buildDir}/openColorIO"
			" {pythonArguments}"
			
			" -D MATERIALX_BUILD_OIIO=TRUE"
			" -D MATERIALX_OIIO_DIR={buildDir}"
			" -D MATERIALX_OSL_INCLUDE_PATH={buildDir}/include"
			" -D MATERIALX_OSLC_EXECUTABLE={buildDir}/bin/oslc"
			" -D MATERIALX_TEST_RENDER=FALSE"

			" -D MATERIALX_INSTALL_STDLIB_PATH=materialX/libraries"
			" ."
		,
		
		"make install -j {jobs}",

	],

	"variant:Python:2" : { 

		"variables" : { 

			"pythonArguments" : "-D MATERIALX_PYTHON_VERSION=2",

		},

	},  

	"variant:Python:3" : { 

		"variables" : { 

			"pythonArguments" : "-D MATERIALX_PYTHON_VERSION=3",

		},

	},

	"manifest" : [

		"include/MaterialX*",
		"lib/libMaterialX*{sharedLibraryExtension}*",
		"python/MaterialX",
		"materialX/libraries",

	],

}
