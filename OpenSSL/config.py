{

	"downloads" : [

		"https://www.openssl.org/source/openssl-1.1.1i.tar.gz",

	],

	"url" : "https://www.openssl.org",

	"license" : "LICENSE",

	"commands" : [

		"./config --prefix={buildDir} -fPIC",
		"make -j {jobs}",
		"make install_sw",

	],

	"manifest" : [

		"lib/libssl*{sharedLibraryExtension}*",
		"lib/libcrypto*{sharedLibraryExtension}*",

	],

	"platform:macos" : {

		"environment" : {

			"KERNEL_BITS" : "64",

		},

	},

}
