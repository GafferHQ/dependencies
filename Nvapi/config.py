{

	"downloads" : [

		"https://github.com/NVIDIA/nvapi/archive/3d34a4faf095996663321646ebe003539a908f89.zip",

	],

	"url" : "https://developer.nvidia.com/nvapi",

	"license" : "License.txt",

	"platform:macos" : {

		"eanbled" : False

	},

	"platform:linux" : {

		"eanbled" : False

	},

	"commands" : [],

	"postMovePaths" : {

		"*.h" : "{buildDir}/include",
		"amd64/nvapi64.lib" : "{buildDir}/lib",

	},

	"manifest" : [

		"include/nvapi*.h",
		"include/NvApi*.h",
		"include/nvHLSL*.h",
		"include/nvShader*.h",
		"lib/nvapi64.lib",

	],

}