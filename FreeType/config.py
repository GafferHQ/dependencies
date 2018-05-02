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
		"lib/{libraryPrefix}freetype*{sharedLibraryExtension}*",
		"lib/{libraryPrefix}freetype*.lib*",

	],
	"platform:windows" : {

		"commands" : [

			"mkdir gafferBuild",
			"cd gafferBuild && cmake"
				" -Wno-dev"
				" -G {cmakeGenerator}"
				" -D CMAKE_BUILD_TYPE={cmakeBuildType}"
				" -D CMAKE_INSTALL_PREFIX={buildDir}"
				" -D CMAKE_DISABLE_FIND_PACKAGE_PNG=TRUE"
				" ..",
			# FreeType 2.9.1 does not copy the ftconfig.h header
			# the binary directory, help it out here
			"copy include\\freetype\\config\\ftconfig.h gafferBuild\\include\\freetype\\config\\ftconfig.h",
			"cd gafferBuild && cmake --build . --config {cmakeBuildType} --target install -- -j {jobs}",

		]

	}

}
