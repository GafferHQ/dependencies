{

	"downloads" : [

		"https://github.com/alembic/alembic/archive/1.7.8.tar.gz"

	],

	"license" : "LICENSE.txt",

	"commands" : [

		"cmake"
			" -D CMAKE_INSTALL_PREFIX={buildDir}"
			" -D CMAKE_PREFIX_PATH={buildDir}"
			" -D Boost_NO_SYSTEM_PATHS=TRUE"
			" -D Boost_NO_BOOST_CMAKE=TRUE"
			" -D BOOST_ROOT={buildDir}"
			" -D ILMBASE_ROOT={buildDir}"
			" -D HDF5_ROOT={buildDir}"
			" -D USE_HDF5=TRUE"
			" -D USE_PYILMBASE=FALSE"
			" -D USE_PYALEMBIC=FALSE"
			" -D USE_ARNOLD=FALSE"
			" -D USE_PRMAN=FALSE"
			" -D USE_MAYA=FALSE"
			" ."
		,

		"make VERBOSE=1 -j {jobs}",
		"make install",

	],

	"platform:windows" : {

		"commands" : [

			"cmake"
				" -G {cmakeGenerator}"
				" -D CMAKE_BUILD_TYPE={cmakeBuildType}"
				" -D CMAKE_INSTALL_PREFIX={buildDir}"
				" -D CMAKE_PREFIX_PATH={buildDir}"
				" -D ALEMBIC_NO_TESTS=ON"
				" -D ALEMBIC_NO_OPENGL=ON"
				" -D Boost_NO_SYSTEM_PATHS=ON"
				" -D Boost_NO_BOOST_CMAKE=ON"
				" -D BOOST_ROOT={buildDir}"
				" -D HDF5_ROOT={buildDir}"
				" -D ILMBASE_ROOT={buildDir}"
				" -D USE_TESTS=OFF"
				" -D USE_HDF5=ON"
				" -D USE_PYILMBASE=OFF"
				" -D USE_PYALEMBIC=OFF"
				" -D USE_ARNOLD=OFF"
				" -D USE_PRMAN=OFF"
				" -D USE_MAYA=OFF"
				" -D ALEMBIC_LIB_USES_BOOST=TRUE"
				" -D ALEMBIC_ILMBASE_HALF_LIB={buildDir}\\lib\\half.lib"
				" -D ALEMBIC_ILMBASE_IEX_LIB={buildDir}\\lib\\Iex-2_2.lib"
				" -D ALEMBIC_ILMBASE_IEXMATH_LIB={buildDir}\\lib\\IexMath-2_2.lib"
				" -D ALEMBIC_ILMBASE_ILMTHREAD_LIB={buildDir}\\lib\\IlmThread-2_2.lib"
				" -D ALEMBIC_ILMBASE_IMATH_LIB={buildDir}\\lib\\Imath-2_2.lib"
				" .",
			"cmake --build . --config {cmakeBuildType} --target install"

		],

	},

}
