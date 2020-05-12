{

	"downloads" : [

		"https://download.savannah.gnu.org/releases/freetype/freetype-2.9.1.tar.gz",

	],

	"url" : "http://www.freetype.org",
	"license" : "docs/FTL.TXT",
	"credit" : "Portions of this software are copyright (c) 2018 The FreeType Project (www.freetype.org). All rights reserved.",

	"environment" : {

		"LDFLAGS" : "-L{buildDir}/lib",
		"PKG_CONFIG_PATH" : "{buildDir}/lib/pkgconfig",

	},

	"commands" : [

		"./configure --prefix={buildDir} --with-harfbuzz=no",
		"make -j {jobs}",
		"make install",

	],

	"manifest" : [

		"include/freetype2",
		"lib/libfreetype*{sharedLibraryExtension}*",

	],

}
