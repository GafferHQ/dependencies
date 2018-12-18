{

	"downloads" : [

		"https://github.com/GafferHQ/resources/archive/0.54.2.0.tar.gz"

	],

	"url" : "https://www.gafferhq.org",

	"license" : None,

	"commands" : [

		"cp -r resources {buildDir}",

	],

	"manifest" : [

		"resources/hdri",
		"resources/cow",
		"resources/gafferBot",
		# Can't include `resources/images` as a directory, because that
		# catches a bunch of MaterialX stuff we don't want.
		"resources/images/macaw.exr",
		"resources/cortex",

	],
	"platform:windows" : {

		"commands" : [

			"xcopy /s /e /h /y /i resources {buildDir}\\resources",

		],

	}

}
