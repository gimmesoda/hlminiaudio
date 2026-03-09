include(FetchContent)

if(NOT TARGET vorbisfile)
	find_package(Vorbis QUIET)
	if(TARGET Vorbis::vorbisfile)
		add_library(vorbisfile INTERFACE IMPORTED)
		target_link_libraries(vorbisfile INTERFACE Vorbis::vorbisfile)
	else()
		set(INSTALL_CMAKE_PACKAGE_MODULE OFF CACHE BOOL "" FORCE)
		set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
		set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "" FORCE)

		FetchContent_Declare(
			ogg
			GIT_REPOSITORY https://github.com/xiph/ogg.git
			GIT_TAG v1.3.6
		)
		FetchContent_Declare(
			vorbis
			GIT_REPOSITORY https://github.com/xiph/vorbis.git
			GIT_TAG v1.3.7
		)

		FetchContent_MakeAvailable(ogg)

		if(TARGET ogg)
			set(OGG_FOUND TRUE CACHE BOOL "" FORCE)
			set(OGG_INCLUDE_DIR "${ogg_SOURCE_DIR}/include" CACHE PATH "" FORCE)
			set(OGG_LIBRARY ogg CACHE STRING "" FORCE)
		endif()

		FetchContent_MakeAvailable(vorbis)
	endif()
endif()

add_library(extension SHARED
	"extension.cpp"

	)

if (CMAKE_VERSION VERSION_GREATER 3.12)
	set_property(TARGET extension PROPERTY CXX_STANDARD 20)
endif()

set_target_properties(extension PROPERTIES
	OUTPUT_NAME "miniaudio"
	SUFFIX ".hdll")

if (WIN32)
	target_include_directories(extension PRIVATE "$ENV{HASHLINK}/include")
	target_link_libraries(extension PRIVATE "$ENV{HASHLINK}/libhl.lib")
endif()

add_subdirectory("lib/miniaudio")
if(NOT TARGET miniaudio_libvorbis)
	message(FATAL_ERROR "libvorbis support is required, but miniaudio_libvorbis was not created")
endif()

target_include_directories(extension PRIVATE miniaudio miniaudio_common_options)
target_link_libraries(extension PRIVATE miniaudio miniaudio_common_options miniaudio_libvorbis)
