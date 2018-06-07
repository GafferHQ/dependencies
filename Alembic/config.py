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

}
