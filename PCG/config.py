{

	"downloads" : [

		"https://github.com/imneme/pcg-cpp/archive/428802d.tar.gz"

	],

	"url" : "https://www.pcg-random.org",

	"license" : "LICENSE-MIT.txt",


	"commands" : [
		"mkdir -p {buildDir}/include/pcg",
		"cp include/*.hpp {buildDir}/include/pcg",

	],

	"manifest" : [

		"include/pcg",

	],
    
	"platform:windows" : {

		"commands" : [
			"if not exist {buildDir}\\include\\pcg mkdir {buildDir}\\include\\pcg",
			"copy include\\*.hpp {buildDir}\\include\\pcg",
		],

	},

}
