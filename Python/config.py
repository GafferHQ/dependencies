{

	"downloads" : [

		"https://www.python.org/ftp/python/2.7.15/Python-2.7.15.tgz",

	],

	"license" : "LICENSE",

	"commands" : [

		"./configure --prefix={buildDir} {libraryType} --enable-unicode=ucs4",
		"make -j {jobs}",
		"make install",

	],

	"variables" : {

		"libraryType" : "--enable-shared",

	},

	"platform:osx" : {

		"variables" : {

			"libraryType" : "--enable-framework={buildDir}/lib",

		},

		"symbolicLinks" : [

			( "{buildDir}/bin/python", "../lib/Python.framework/Versions/Current/bin/python" ),
			( "{buildDir}/bin/python2", "../lib/Python.framework/Versions/Current/bin/python2" ),
			( "{buildDir}/bin/python2.7", "../lib/Python.framework/Versions/Current/bin/python2.7" ),

		],

	},

	"platform:windows" : {

		"downloads" : [

			"https://www.python.org/ftp/python/2.7.15/Python-2.7.15.tgz",
			"https://github.com/python-cmake-buildsystem/python-cmake-buildsystem/archive/master.tar.gz",

		],

		"commands" : [
			"move ..\\python-cmake-buildsystem-master .\\python-cmake-buildsystem",

			"mkdir gafferBuild",

			"cmake"
				" -Wno-dev"
				" -G {cmakeGenerator}"
				" -D CMAKE_BUILD_TYPE={cmakeBuildType}"
				" -D CMAKE_INSTALL_PREFIX={buildDir}"
				" -D PYTHON_VERSION=2.7.15"
				" -D DOWNLOAD_SOURCES=OFF"
				" -D BUILD_LIBPYTHON_SHARED=ON"
				" -D Py_UNICODE_SIZE=4"
				" -D USE_LIB64=ON"
				" -D INSTALL_TEST=OFF"
				" python-cmake-buildsystem",
			"cmake --build . --config {cmakeBuildType} --target install -- -j {jobs}",
			"copy {buildDir}\\libs\\*.lib {buildDir}\\lib",
		]

	}

}
