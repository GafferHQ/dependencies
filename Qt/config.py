{

	"downloads" : [

		"http://download.qt.io/official_releases/qt/5.12/5.12.8/single/qt-everywhere-src-5.12.8.tar.xz"

	],

	"dependencies" : [ "LibPNG", "LibTIFF", "LibJPEG-Turbo", "FreeType" ],

	"license" : "LICENSE.LGPLv21",

	"environment" : {

		"PKG_CONFIG_PATH" : "{buildDir}/lib/pkgconfig",

	},

	"commands" : [

		"./configure"
			" -prefix {buildDir}"
			" -plugindir {buildDir}/qt/plugins"
			" -release"
			" -opensource -confirm-license"
			" -no-rpath"
			" -no-dbus"
			" -skip qtconnectivity"
			" -skip qtwebengine"
			" -skip qt3d"
			" -skip qtdeclarative"
			" -skip qtwebchannel"
			" -no-libudev"
			" -no-icu"
			" -qt-pcre"
			" -nomake examples"
			" -nomake tests"
			" {extraArgs}"
			" -I {buildDir}/include -I {buildDir}/include/freetype2"
			" -L {buildDir}/lib"
		,

		"make -j {jobs} && make install",

	],

	"manifest" : [

		"bin/moc",
		"bin/qmake",
		"bin/rcc",
		"bin/uic",

		"include/Qt*",

		"lib/libQt*",
		"lib/Qt*.framework",

		"mkspecs",
		"qt",
		"lib/cmake",

	],

	"platform:linux" : {

		"environment" : {

			"LD_LIBRARY_PATH" : "{buildDir}/lib",

		},

		"variables" : {

			"extraArgs" : "-qt-xcb",

		},

	},

	"platform:osx" : {

		"variables" : {

			"extraArgs" : "-no-freetype",

		},

	},

}
