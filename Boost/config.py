{

	"downloads" : [

		"https://sourceforge.net/projects/boost/files/boost/1.61.0/boost_1_61_0.tar.gz"

	],

	"license" : "LICENSE_1_0.txt",

	"environment" : {

		# Without this, boost build will still pick up the system python framework,
		# even though we tell it quite explicitly to use the one in {buildDir}.
		"DYLD_FALLBACK_FRAMEWORK_PATH" : "{buildDir}/lib",
		"MACOSX_DEPLOYMENT_TARGET" : "10.9",
		# Give a helping hand to find the python headers, since the bootstrap
		# below doesn't always seem to get it right.
		"CPLUS_INCLUDE_PATH" : "{buildDir}/include/python2.7",

	},

	"commands" : [

		"./bootstrap.sh --prefix={buildDir} --with-python={buildDir}/bin/python --with-python-root={buildDir} --without-libraries=log --without-icu",
		"./bjam -d+2 -j {jobs} --disable-icu cxxflags='-std=c++11' variant=release link=shared threading=multi install",

	],

}
