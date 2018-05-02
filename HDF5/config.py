{

	"downloads" : [

		"https://support.hdfgroup.org/ftp/HDF5/current18/src/hdf5-1.8.20.tar.gz"

	],

	"license" : "COPYING",

	"commands" : [

		"./configure --prefix={buildDir} --enable-threadsafe --disable-hl --with-pthread=/usr/include",

		"make -j {jobs}",
		"make install",

	],

	
	# Note that JOM can't be used due to it generating the following errors:
	# [  1%] Generating ../H5Tinit.c
	# The process cannot access the file because it is being used by another process.
	# [  1%] Generating ../H5lib_settings.c
	# jom: M:\gafferDependencies\HDF5\working\hdf5-1.8.20\build\src\CMakeFiles\hdf5-shared.dir\build.make [H5Tinit.c] Error 1
	# The process cannot access the file because it is being used by another process.
	# jom: M:\gafferDependencies\HDF5\working\hdf5-1.8.20\build\src\CMakeFiles\hdf5-shared.dir\build.make [H5lib_settings.c] Error 1
	# Scanning dependencies of target hdf5-static
	# jom: M:\gafferDependencies\HDF5\working\hdf5-1.8.20\build\CMakeFiles\Makefile2 [src\CMakeFiles\hdf5-shared.dir\all] Error 2
	# jom: M:\gafferDependencies\HDF5\working\hdf5-1.8.20\build\Makefile [all] Error 2

	"platform:windows" : {

		"variables" : {

			"cmakeGenerator" : "\"Visual Studio 15 2017 Win64\"",

		},

		"commands" : [
			"if not exist \"build\" mkdir build",
			"cd build &&"
				" cmake"
				" -C ..\\config\\cmake\\cacheinit.cmake"
				" -Wno-dev"
				" -G {cmakeGenerator}"
				" -D CMAKE_BUILD_TYPE={cmakeBuildType}"
				" -D CMAKE_INSTALL_PREFIX={buildDir}"
				" -D HDF5_ENABLE_THREADSAFE=ON"
				" -D BUILD_SHARED_LIBS=ON"
				" -D BUILD_TESTING=OFF"
				" -D HDF5_BUILD_EXAMPLES=OFF"
				" -D HDF5_BUILD_FORTRAN=OFF"
				" -D HDF5_ENABLE_SZIP_SUPPORT=OFF"
				" ..",
			"cd build && cmake --build . --config {cmakeBuildType} --target install",

		],

	}

}
