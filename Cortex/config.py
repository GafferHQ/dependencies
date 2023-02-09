{

	"downloads" : [

		"https://github.com/ImageEngine/cortex/archive/refs/tags/10.4.5.0.tar.gz"

	],

	"url" : "https://github.com/ImageEngine/cortex",

	"license" : "LICENSE",

	"dependencies" : [
		"Python", "OpenImageIO", "OpenEXR", "Boost", "OpenShadingLanguage",
		"Blosc", "FreeType", "GLEW", "Appleseed", "TBB", "OpenVDB", "USD", "Six"
	],

	"environment" : {

		"LD_LIBRARY_PATH" : "{buildDir}/lib",

	},

	"commands" : [

		# Build first.
		"scons {args}",
		# The install separately. This avoids a problem with parallel builds - see
		# https://github.com/ImageEngine/cortex/issues/1308.
		"scons install {args}",

	],

	"variables" : {

		"args" :
			" -j {jobs}"
			" CXX=`which g++`"
			" CXXSTD=c++{c++Standard}"
			" INSTALL_PREFIX={buildDir}"
			" INSTALL_DOC_DIR={buildDir}/doc/cortex"
			" INSTALL_PYTHON_DIR={buildDir}/python"
			" INSTALL_IECORE_OPS=''"
			" PYTHON_CONFIG={buildDir}/bin/python{pythonMajorVersion}-config"
			" PYTHON={buildDir}/bin/python"
			" BOOST_INCLUDE_PATH={buildDir}/include/boost"
			" LIBPATH={buildDir}/lib"
			" BOOST_LIB_SUFFIX=''"
			" OPENEXR_INCLUDE_PATH={buildDir}/include"
			" ILMBASE_INCLUDE_PATH={buildDir}/include"
			" VDB_INCLUDE_PATH={buildDir}/include"
			" TBB_INCLUDE_PATH={buildDir}/include"
			" OIIO_INCLUDE_PATH={buildDir}/include"
			" OSL_INCLUDE_PATH={buildDir}/include"
			" BLOSC_INCLUDE_PATH={buildDir}/include"
			" FREETYPE_INCLUDE_PATH={buildDir}/include/freetype2"
			" WITH_GL=1"
			" GLEW_INCLUDE_PATH={buildDir}/include/GL"
			" NUKE_ROOT="
			" APPLESEED_ROOT={buildDir}/appleseed"
			" APPLESEED_INCLUDE_PATH={buildDir}/appleseed/include"
			" APPLESEED_LIB_PATH={buildDir}/appleseed/lib"
			" USD_LIB_PREFIX=usd_"
			" ENV_VARS_TO_IMPORT='LD_LIBRARY_PATH TERM'"
			" OPTIONS=''"
			" {extraArgs}"
			" SAVE_OPTIONS=gaffer.options",

		"extraArgs" : "",

	},

	"manifest" : [

		"include/IECore*",
		"lib/libIECore*{sharedLibraryExtension}",
		"python/IECore*",
		"appleseedDisplays",
		"glsl/IECoreGL",
		"glsl/*.frag",
		"glsl/*.vert",

	],

	"platform:macos" : {

		"variables" : {

			# On Mac, `python3-config --ldflags` is broken, so we specify the flags explicitly.
			"extraArgs" :
				" PYTHON_LIB_PATH={buildDir}/lib/Python.framework/Versions/{pythonVersion}/lib"
				" PYTHON_LINK_FLAGS=-lpython{pythonVersion}"

		},

	},


}
