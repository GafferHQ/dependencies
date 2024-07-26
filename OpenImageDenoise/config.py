{

	"downloads" : [

		"https://github.com/OpenImageDenoise/oidn/releases/download/v2.3.0/oidn-2.3.0.src.tar.gz",
		# The OIDN build system will look for ISPC in a directory next to the source, so we can download a platform
		# specific ISPC release here and it will be extracted to a location where it will be automatically found.
		"{ispcDownload}"

	],

	"url" : "https://www.openimagedenoise.org/",

	"license" : "LICENSE.txt",

	"dependencies" : [ "TBB", "OpenImageIO" ],

	"commands" : [

		"mkdir build",
		"cd build &&"
			" cmake"
			" -D CMAKE_INSTALL_PREFIX={buildDir}"
			" -D CMAKE_INSTALL_LIBDIR=lib"
			" -D CMAKE_PREFIX_PATH={buildDir}"
			" -D CMAKE_BUILD_TYPE=Release"
			" -D OIDN_APPS=ON"
			" -D OIDN_APPS_OPENIMAGEIO=ON"
			" -D OIDN_FILTER_RTLIGHTMAP=OFF"
			" {extraArguments}"
			" ..",
		"cd build && cmake --build . --config Release --target install -- -j {jobs}",

	],

	"manifest" : [

		"bin/oidnDenoise",
		"include/OpenImageDenoise*",
		"lib/*OpenImageDenoise*",

	],

	"variables" : {

		"extraArguments" : "",
		"ispcDownload" : "https://github.com/ispc/ispc/releases/download/v1.21.1/ispc-v1.21.1-linux-oneapi.tar.gz",

	},

	"platform:linux" : {

		"environment" : {

			## \todo : This environment variable should instead be defined in the build container.
			"CUDA_PATH" : "/usr/local/cuda",

		},

		"variables" : {

			"extraArguments" : "-D OIDN_DEVICE_CUDA=ON",

		},

	},

	"platform:macos" : {

		"variables" : {

			"ispcDownload" : "https://github.com/ispc/ispc/releases/download/v1.21.1/ispc-v1.21.1-macOS.universal.tar.gz",

		},

	},

}
