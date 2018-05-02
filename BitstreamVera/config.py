{

	"downloads" : [

		"https://ftp.gnome.org/pub/GNOME/sources/ttf-bitstream-vera/1.10/ttf-bitstream-vera-1.10.tar.gz"
	],

	"url" : "https://www.gnome.org/fonts",

	"license" : "COPYRIGHT.TXT",

	"commands" : [

		"mkdir -p {buildDir}/fonts",
		"cp *.ttf {buildDir}/fonts"

	],

	"manifest" : [

		"fonts",

	],
	"platform:windows" : {

		"commands" : [

			"if not exist \"{buildDir}\\fonts\" mkdir {buildDir}\\fonts",
			"copy *.ttf {buildDir}\\fonts"

		]

	}

}
