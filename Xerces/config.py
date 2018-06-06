{

	"downloads" : [

		"http://apache.mirror.anlx.net//xerces/c/3/sources/xerces-c-3.2.1.tar.gz"

	],

	"license" : "LICENSE",

	"commands" : [

		"./configure --prefix={buildDir} --without-icu",
		"make -j {jobs}",
		"make install",

	],

}
