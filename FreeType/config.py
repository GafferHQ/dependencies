{

	"downloads" : [

		"https://download.savannah.gnu.org/releases/freetype/freetype-2.9.1.tar.gz",

	],

	"license" : "docs/FTL.TXT",

	"environment" : {

		"LDFLAGS" : "-L{buildDir}/lib",

	},

	"commands" : [

		"./configure --prefix={buildDir} --with-harfbuzz=no",
		"make -j {jobs}",
		"make install",

	],

	"platform:windows" : {

		"commands" : [

			"mkdir gafferBuild",
			"cd gafferBuild && cmake"
				" -Wno-dev"
				" -G {cmakeGenerator}"
				" -D CMAKE_BUILD_TYPE={cmakeBuildType}"
				" -D CMAKE_INSTALL_PREFIX={buildDir}"
				" ..",
			# FreeType 2.9.1 does not copy the ftconfig.h header
			# the binary directory, help it out here
			"copy include\\freetype\\config\\ftconfig.h gafferBuild\\include\\freetype\\config\\ftconfig.h",
			"cd gafferBuild && cmake --build . --config {cmakeBuildType} --target install -- -j {jobs}",

		]

	}

}
