{

	"downloads" : [

		"https://www.openssl.org/source/old/1.0.2/openssl-1.0.2h.tar.gz",

	],

	"license" : "LICENSE",

	"commands" : [

		"./config --prefix={buildDir} -fPIC",
		"make -j {jobs}",
		"make install",

	],

	"platform:osx" : {

		"environment" : {

			"KERNEL_BITS" : "64",

		},

	},

}
