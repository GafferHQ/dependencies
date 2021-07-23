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

		"cmake/MaterialXConfig.cmake",
		"cmake/MaterialXConfig-noconfig.cmake",
		"cmake/MaterialXConfigVersion.cmake"
		"lib/libMaterialXCore.so.1.38.0",
		"lib/libMaterialXCore.so.1",
		"lib/libMaterialXCore.so",
		"lib/libMaterialXFormat.so.1.38.0",
		"lib/libMaterialXFormat.so.1",
		"lib/libMaterialXFormat.so"
		"lib/libMaterialXGenShader.so.1.38.0",
		"lib/libMaterialXGenShader.so.1",
		"lib/libMaterialXGenShader.so",
		"lib/libMaterialXGenGlsl.so.1.38.0",
		"lib/libMaterialXGenGlsl.so.1",
		"lib/libMaterialXGenGlsl.so",
		"lib/libMaterialXGenOsl.so.1.38.0",
		"lib/libMaterialXGenOsl.so.1",
		"lib/libMaterialXGenOsl.so",
		"lib/libMaterialXRender.so.1.38.0",
		"lib/libMaterialXRender.so.1",
		"lib/libMaterialXRender.so",
		"lib/libMaterialXRenderOsl.so.1.38.0",
		"lib/libMaterialXRenderOsl.so.1",
		"lib/libMaterialXRenderOsl.so",
		"lib/libMaterialXRenderHw.so.1.38.0",
		"lib/libMaterialXRenderHw.so.1",
		"lib/libMaterialXRenderHw.so",
		"lib/libMaterialXRenderGlsl.so.1.38.0",
		"lib/libMaterialXRenderGlsl.so.1",
		"lib/libMaterialXRenderGlsl.so",
		"include/MaterialXCore",
		"include/MaterialXFormat",
		"include/MaterialXGenShader",
		"include/MaterialXGenGlsl",
		"include/MaterialXGenOsl",
		"include/MaterialXRender",
		"include/MaterialXRenderOsl",
		"include/MaterialXRenderHw",
		"include/MaterialXRenderGlsl",
		"resources/Geometry",
		"resources/Images",
		"resources/Lights",
		"resources/Materials",
		"libraries/bxdf",
		"libraries/lights",
		"libraries/pbrlib",
		"libraries/stdlib",
		"python/MaterialX",

	],

}
