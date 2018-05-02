{

	"downloads" : [

		"https://github.com/mottosso/Qt.py/archive/1.2.5.tar.gz"
	],

	"url" : "https://github.com/mottosso/Qt.py",

	"license" : "LICENSE",

	"dependencies" : [ "Python" ],

	"commands" : [

		"cp Qt.py {buildDir}/python",

	],

	"manifest" : [

		"python/Qt.py",

	],

	"platform:windows" : {

		"commands" : [

			"copy Qt.py {buildDir}\\python",

		]

	}

}