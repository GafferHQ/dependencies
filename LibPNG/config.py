{

	"downloads" : [

		"https://download.sourceforge.net/libpng/libpng-1.6.3.tar.gz"

	],

	"license" : "LICENSE",

	"commands" : [

		"./configure --prefix={buildDir}",
		"make -j {jobs}",
		"make install",

	],

}
