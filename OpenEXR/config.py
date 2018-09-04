{

	"downloads" : [

		"https://github.com/openexr/openexr/archive/v2.2.0.tar.gz"

	],

	"license" : "OpenEXR/COPYING",

	"environment" : {

		"DYLD_FALLBACK_LIBRARY_PATH" : "{buildDir}/lib",
		"LD_LIBRARY_PATH" : "{buildDir}/lib",

	},

	"commands" : [

		"cmake -E copy IlmBase/COPYING {buildDir}/doc/licenses/IlmBase",
		"mkdir IlmBase/gafferBuild",
		"cd IlmBase/gafferBuild &&"
			" cmake"
			" -G \"Unix Makefiles\""
			" -D CMAKE_INSTALL_PREFIX={buildDir}"
			" -D CMAKE_PREFIX_PATH={buildDir}"
			" -D BUILD_SHARED_LIBS=ON"
			" ..",
		"cd IlmBase/gafferBuild && make install -j {jobs} VERBOSE=1",
		"cmake -E copy PyIlmBase/COPYING {buildDir}/doc/licenses/PyIlmBase",
		"mkdir PyIlmBase/gafferBuild",
		"cd PyIlmBase/gafferBuild &&"
			" cmake"
			" -G \"Unix Makefiles\""
			" -D CMAKE_INSTALL_PREFIX={buildDir}"
			" -D CMAKE_PREFIX_PATH={buildDir}"
			" -D BUILD_SHARED_LIBS=ON"
			" -D ILMBASE_PACKAGE_PREFIX={buildDir}"
			" -D PYTHON_LIBRARY={buildDir}/lib"
			" -D PYTHON_INCLUDE_DIR={buildDir}/include/python2.7"
			" -D BOOST_ROOT={buildDir}"
			" ..",
		"cd PyIlmBase/gafferBuild && make install -j {jobs} VERBOSE=1",
		"cmake -E make_directory {buildDir}/python &&"
		"cmake -E rename {buildDir}/lib/python*/site-packages/iexmodule.so {buildDir}/python/iexmodule.so &&"
		"cmake -E rename {buildDir}/lib/python*/site-packages/imathmodule.so {buildDir}/python/imathmodule.so",
		"mkdir OpenEXR/gafferBuild",
		"cd OpenEXR/gafferBuild &&"
			" cmake"
			" -G \"Unix Makefiles\""
			" -D CMAKE_INSTALL_PREFIX={buildDir}"
			" -D CMAKE_PREFIX_PATH={buildDir}"
			" -D BUILD_SHARED_LIBS=ON"
			" -D ILMBASE_PACKAGE_PREFIX={buildDir}"
			" ..",
		"cd OpenEXR/gafferBuild && make install -j {jobs} VERBOSE=1"

	],

}
