{

	"downloads" : [

		"https://download.qt.io/official_releases/qt/5.15/5.15.12/single/qt-everywhere-opensource-src-5.15.12.tar.xz"

	],

	"dependencies" : [ "LibPNG", "LibTIFF", "LibJPEG-Turbo", "FreeType" ],

	"url" : "https://www.qt.io",

	"license" : "LICENSE.LGPLv21",

	"environment" : {

		"PKG_CONFIG_PATH" : "{buildDir}/lib/pkgconfig",

	},

	"commands" : [

		"./configure"
			" -prefix {buildDir} "
			" -plugindir {buildDir}/qt/plugins"
			" -release"
			" -opensource -confirm-license"
			" -no-rpath"
			" -no-dbus"
			" -no-egl"
			" -no-gstreamer"
			" -skip qtconnectivity"
			" -skip qtwebengine"
			" -skip qt3d"
			" -skip qtdeclarative"
			" -skip qtwebchannel"
			" -skip qtnetworkauth"
			" -skip qtpurchasing"
			" -skip wayland"
			" -no-libudev"
			" -no-icu"
			" -qt-pcre"
			" -qt-harfbuzz"
			" -nomake examples"
			" -nomake tests"
			" {extraArgs}"
			" -I {buildDir}/include -I {buildDir}/include/freetype2"
			" -L {buildDir}/lib"
		,

		"make -j {jobs} && make install",

	],

	"manifest" : [

		"bin/moc{executableExtension}",
		"bin/qmake{executableExtension}",
		"bin/rcc{executableExtension}",
		"bin/uic{executableExtension}",

		"include/Qt*",

		"lib/{libraryPrefix}Qt*",
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

			"extraArgs" : "-xcb -bundled-xcb-xinput",

		},

	},

	"platform:macos" : {

		"variables" : {

			"extraArgs" : "-no-freetype QMAKE_APPLE_DEVICE_ARCHS=arm64",

		},

	},

	"platform:windows" : {

		"environment" : {

			"PATH" : "%ROOT_DIR%\\Qt\\working\\qt-everywhere-src-5.15.12\\qtbase\\lib;{buildDir}\\lib;{buildDir}\\bin;%PATH%",

		},

		"commands" : [

			"copy {buildDir}\\lib\\zlib.lib {buildDir}\\lib\\zdll.lib",
			"copy {buildDir}\\lib\\libpng16.lib {buildDir}\\lib\\libpng.lib",
			"copy {buildDir}\\lib\\jpeg.lib {buildDir}\\lib\\libjpeg.lib",
			# help Qt find the right zlib.dll
			"copy {buildDir}\\bin\\zlib.dll %ROOT_DIR%\\Qt\\working\\qt-everywhere-src-5.15.12\\qtbase\\bin\\zlib.dll",
			"call configure.bat"
				" -prefix {buildDir}"
				" -plugindir {buildDir}\\qt\\plugins"
				" -release"
				" -opensource"
				" -confirm-license"
				" -opengl desktop"
				" -no-angle"
				" -no-rpath"
				" -no-dbus"
				" -skip qt3d"
				" -skip qtcharts"
				" -skip qtconnectivity"
				" -skip qtdatavis3d"
				" -skip qtdeclarative"
				" -skip qtgamepad"
				" -skip qtnetworkauth"
				" -skip qtpurchasing"
				" -skip qtremoteobjects"
				" -skip qtsensors"
				" -skip qtserialbus"
				" -skip qtserialport"
				" -skip qtspeech"
				" -skip qtwebchannel"
				" -skip qtwebengine"
				" -no-libudev"
				" -no-icu"
				" -qt-pcre"
				" -nomake examples"
				" -nomake tests"
				" -system-zlib"
				" -I {buildDir}\\include"
				" -L {buildDir}\\lib",
			"jom.exe",
			"jom.exe install",
			"copy {buildDir}\\bin\\{libraryPrefix}Qt* {buildDir}\\lib\\",

		]
	}

}
