{

	"downloads" : [

		"https://github.com/KhronosGroup/Vulkan-Headers/archive/refs/tags/v1.2.190.tar.gz"

	],

	"url" : "https://www.vulkan.org/",

	"license" : "LICENSE.txt",

	"commands" : [

		"mkdir build",
		"cd build &&"
			" cmake"
			" -DCMAKE_INSTALL_PREFIX={buildDir}"
			" -DCMAKE_BUILD_TYPE=Release"
			" ..",
		"cd build && make install -j {jobs}"

	],

	"manifest" : [
		"include/vulkan",
		"include/vk_video",
	],

}
