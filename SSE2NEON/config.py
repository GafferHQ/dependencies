{

	"downloads" : [

		"https://github.com/DLTcollab/sse2neon/archive/refs/tags/v1.9.1.tar.gz"

	],

	"url" : "https://github.com/DLTcollab/sse2neon",

	"license" : "LICENSE",

	"commands" : [

		"cmake -E copy sse2neon.h {buildDir}/include/sse2neon.h",

	],

	# sse2neon is only required on macOS
	"enabled" : False,

	"platform:macos" : {

		"enabled" : True,

	},

	"manifest" : [

		"include/sse2neon.h",

	],

}
