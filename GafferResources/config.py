{

	"downloads" : [

		"https://github.com/GafferHQ/resources/archive/0.47.0.0.tar.gz"

	],

	"license" : None,

	"commands" : [

		"cp -r resources {buildDir}",

	],

	"platform:windows" : {

		"commands" : [

			"xcopy /s /e /h /y /i resources {buildDir}\\resources",

		],

	}

}
