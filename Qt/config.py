{

	"downloads" : [

		"https://download.qt.io/official_releases/qt/6.5/6.5.8/src/single/qt-everywhere-opensource-src-6.5.8.tar.xz"

	],

	"dependencies" : [ "LibPNG", "LibTIFF", "LibJPEG-Turbo", "FreeType", "LLVM" ],

	"url" : "https://www.qt.io",

	"license" : "LICENSES/LGPL-3.0-only.txt",

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
			" -skip qtwayland"
			" -skip qtdoc"
			" -skip qthttpserver"
			" -skip qtlocation"
			" -skip qtlottie"
			" -skip qtmqtt"
			" -skip qtopcua"
			" -skip qtquick3d"
			" -skip qtquick3dphysics"
			" -skip qtquickeffectmaker"
			" -skip qtquicktimeline"
			" -skip qtvirtualkeyboard"
			" -skip qtwebsockets"
			" -skip qtwebview"
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

		"cmake --build . --parallel {jobs} && cmake --install .",

		"cp {buildDir}/libexec/moc {buildDir}/bin",
		"cp {buildDir}/libexec/rcc {buildDir}/bin",
		"cp {buildDir}/libexec/uic {buildDir}/bin",

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
			"LLVM_INSTALL_DIR" : "{buildDir}",

		},

		"variables" : {

			"extraArgs" : "-xcb -bundled-xcb-xinput",

		},

	},

	"platform:macos" : {

		"environment" : {

			"DYLD_FALLBACK_LIBRARY_PATH" : "{buildDir}/lib",

		},

		"variables" : {

			"extraArgs" : "-no-freetype QMAKE_APPLE_DEVICE_ARCHS=arm64",

		},

	},

	"platform:windows" : {

		"variables" : {

			"extraArgs" : "",

		}

	},

}
