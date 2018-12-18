{

	"downloads" : [

		"https://github.com/alembic/alembic/archive/refs/tags/1.8.3.tar.gz"

	],

	"url" : "http://www.alembic.io",

	"license" : "LICENSE.txt",

	"dependencies" : [ "Python", "OpenEXR", "Boost", "HDF5" ],

	"commands" : [

		"cmake"
			" -D CMAKE_INSTALL_PREFIX={buildDir}"
			" -D CMAKE_PREFIX_PATH={buildDir}"
			" -D Boost_NO_SYSTEM_PATHS=TRUE"
			" -D Boost_NO_BOOST_CMAKE=TRUE"
			" -D BOOST_ROOT={buildDir}"
			" -D ILMBASE_ROOT={buildDir}"
			" -D HDF5_ROOT={buildDir}"
			" -D ALEMBIC_PYILMBASE_INCLUDE_DIRECTORY={buildDir}/include/OpenEXR"
			" -D USE_HDF5=TRUE"
			" -D USE_TESTS=FALSE"
			" -D USE_ARNOLD=FALSE"
			" -D USE_PRMAN=FALSE"
			" -D USE_MAYA=FALSE"
			" ."
		,

		"make VERBOSE=1 -j {jobs}",
		"make install",

	],

	"manifest" : [

		"bin/abcconvert{executableExtension}",
		"bin/abcecho{executableExtension}",
		"bin/abcechobounds{executableExtension}",
		"bin/abcls{executableExtension}",
		"bin/abcstitcher{executableExtension}",
		"bin/abctree{executableExtension}",

		"include/Alembic",

		"{sharedLibraryDir}/{libraryPrefix}Alembic*",
		"lib/{libraryPrefix}Alembic*.lib",

		"python/alembic*",

	],

	"platform:windows" : {

		"commands" : [

			"cmake"
				" -G {cmakeGenerator}"
				" -D CMAKE_BUILD_TYPE={cmakeBuildType}"
				" -D CMAKE_INSTALL_PREFIX={buildDir}"
				" -D CMAKE_PREFIX_PATH={buildDir}"
				" -D HDF5_ROOT={buildDir}"
				" -D USE_STATIC_HDF5=ON"
				" -D ILMBASE_ROOT={buildDir}"
				" -D USE_TESTS=OFF"
				" -D USE_HDF5=ON"
				" -D USE_PYALEMBIC=OFF"
				" -D USE_ARNOLD=OFF"
				" -D USE_PRMAN=OFF"
				" -D USE_MAYA=OFF"
				" -D ALEMBIC_ILMBASE_HALF_LIB={buildDir}\\lib\\half.lib"
				" -D ALEMBIC_ILMBASE_IEX_LIB={buildDir}\\lib\\Iex.lib"
				" -D ALEMBIC_ILMBASE_IEXMATH_LIB={buildDir}\\lib\\IexMath.lib"
				" -D ALEMBIC_ILMBASE_ILMTHREAD_LIB={buildDir}\\lib\\IlmThread.lib"
				" -D ALEMBIC_ILMBASE_IMATH_LIB={buildDir}\\lib\\Imath.lib"
				" .",
			"cmake --build . --config {cmakeBuildType} --target install"

		],

		"postMovePaths" : {

			"{buildDir}/lib/Alembic.dll" : "{buildDir}/bin",

		}

	},

}
