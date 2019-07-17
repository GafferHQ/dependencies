{

	"downloads" : [

		"https://github.com/openexr/openexr/releases/download/v2.3.0/ilmbase-2.3.0.tar.gz"

	],

	"license" : "LICENSE",

	"commands" : [

		"./configure --prefix={buildDir}",
		"make -j {jobs}",
		"make install",

	],

}
