{

	"downloads" : [

		"https://boostorg.jfrog.io/artifactory/main/release/1.80.0/source/boost_1_80_0.tar.gz"

	],

	"license" : "LICENSE_1_0.txt",

	"url" : "http://www.boost.org",

	"dependencies" : [ "Python" ],

	"environment" : {

		# Without this, boost build will still pick up the system python framework,
		# even though we tell it quite explicitly to use the one in {buildDir}.
		"DYLD_FALLBACK_FRAMEWORK_PATH" : "{buildDir}/lib",
		"LD_LIBRARY_PATH" : "{buildDir}/lib",
		"MACOSX_DEPLOYMENT_TARGET" : "10.9",
		# Give a helping hand to find the python headers, since the bootstrap
		# below doesn't always seem to get it right.
		"CPLUS_INCLUDE_PATH" : "{pythonIncludeDir}",

	},

	"commands" : [

		"./bootstrap.sh --prefix={buildDir} --with-python={buildDir}/bin/python --with-python-root={buildDir} --without-libraries=log --without-icu",
		"./b2 -d+2 -j {jobs} --disable-icu cxxflags='-std=c++{c++Standard}' cxxstd={c++Standard} variant=release link=shared threading=multi install",

	],

	"manifest" : [

		"include/boost",
		"lib/{libraryPrefix}boost_*{sharedLibraryExtension}*",
		"lib/{libraryPrefix}boost_*.lib",
		"lib/libboost_test_exec_monitor*{staticLibraryExtension}",	# Windows and Linux both use the "lib" prefix

	],

	"platform:windows" : {

		"dependencies" : [ "Python", "ZLib" ],
	
		"environment" : {

			# Boost needs help finding Python
			"PATH" : "{buildDir}\\bin;%PATH%",
			"PYTHONPATH" : "{buildDir};{buildDir}\\bin;{buildDir}\\lib\\python{pythonVersion};{buildDir}\\lib"

		},

		"commands" : [
			# "echo using python : {pythonVersion} : \"{buildDirFwd}/bin/python\" : {pythonIncludeDir} : {pythonLibDir} ; >> tools\\build\\src\\user-config.jam",  # best to use forward slashes in user-config.jam
			"bootstrap.bat --prefix={buildDir} --without-libraries=log",
			"b2 -d+2 --prefix={buildDir} --layout=system --toolset=msvc architecture=x86 address-model=64 variant=release link=shared threading=multi cxxflags=\"/std:c++{c++Standard}\" cxxstd={c++Standard} -s ZLIB_SOURCE=%ROOT_DIR%\\ZLib\\working\\zlib-1.2.11 -s ZLIB_INCLUDE={buildDir}\\include -s ZLIB_LIBPATH={buildDir}\\lib -s ZLIB_BINARY=zlib install"

		],

	},
}
