{

	"downloads" : [

		"https://github.com/PixarAnimationStudios/USD/archive/v18.09.tar.gz"

	],

	"license" : "LICENSE.txt",

	"commands" : [

		"cmake"
			" -G {cmakeGenerator}"
			" -D CMAKE_BUILD_TYPE={cmakeBuildType}"
			" -D CMAKE_INSTALL_PREFIX={buildDir}"
			" -D CMAKE_PREFIX_PATH={buildDir}"
			" -D Boost_NO_SYSTEM_PATHS=TRUE"
			" -D Boost_NO_BOOST_CMAKE=TRUE"
			" -D PXR_BUILD_IMAGING=FALSE"
			" -D PXR_BUILD_TESTS=FALSE"
			# Needed to prevent CMake picking up system python libraries on Mac.
			" -D CMAKE_FRAMEWORK_PATH={buildDir}/lib/Python.framework/Versions/2.7/lib"
			" ."
		,

		"make VERBOSE=1 -j {jobs}",
		"make install",

		"mv {buildDir}/lib/python/pxr {buildDir}/python",

	],

	"platform:windows" : {

		"commands" : [

			"mkdir gafferBuild",
			"cd gafferBuild && "
				" cmake"
				" -G {cmakeGenerator}"
				" -D CMAKE_BUILD_TYPE={cmakeBuildType}"
				" -D CMAKE_INSTALL_PREFIX={buildDir}"
				" -D CMAKE_PREFIX_PATH={buildDir}"
				" -D Boost_NO_BOOST_CMAKE=TRUE"
				" -D PXR_BUILD_IMAGING=FALSE"
				" -D PXR_BUILD_TESTS=FALSE"
				" ..",

			"cd gafferBuild && cmake --build . --config {cmakeBuildType} --target install -- -j {jobs}",
			"move {buildDir}\\lib\\python\\pxr {buildDir}\\python"

		],

	},

}
