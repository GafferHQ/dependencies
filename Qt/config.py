{

	"downloads" : [

		"https://damassets.autodesk.net/content/dam/autodesk/www/Company/files/2018/Qt561ForMaya2018Update4.zip"

	],

	"workingDir" : "qt-adsk-5.6.1-vfx",

	"dependencies" : [ "LibPNG", "LibTIFF", "LibJPEG-Turbo", "FreeType" ],

	"license" : "LICENSE.LGPLv21",

	"commands" : [

		"tar -xf ../qt561-webkit.tgz",

		"./configure"
			" -prefix {buildDir}"
			" -plugindir {buildDir}/qt/plugins"
			" -release"
			" -opensource -confirm-license"
			" -no-rpath -no-gtkstyle"
			" -no-audio-backend -no-dbus"
			" -skip qtconnectivity"
			" -skip qtwebengine"
			" -skip qt3d"
			" -skip qtdeclarative"
			" -no-libudev"
			" -no-gstreamer"
			" -no-icu"
			" -qt-pcre"
			" -nomake examples"
			" -nomake tests"
			" {extraArgs}"
			" -I {buildDir}/include -I {buildDir}/include/freetype2"
			" -L {buildDir}/lib"
			" -c++std c++11"
		,

		"make -j {jobs}",
		"make install",

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

			"extraArgs" : "-no-freetype -platform macx-clang",

		},

	},

}
