{

	"downloads" : [

		"https://github.com/anholt/libepoxy/archive/refs/tags/1.5.10.tar.gz"

	],

	"url" : "https://github.com/anholt/libepoxy",
	"license" : "COPYING",

	"commands" : [

		"meson gafferBuild/",
		"meson configure gafferBuild/"
		" -Dprefix={buildDir}"
		" -Dtests=false",
		"ninja -C gafferBuild/ install",
		"mv {buildDir}/lib64/libepoxy* {buildDir}/lib/",

	],

	"manifest" : [

		"include/epoxy",
		"lib/libepoxy*{sharedLibraryExtension}*",

	],

}
