if(POLICY CMP0135)
	cmake_policy(SET CMP0135 NEW)
endif()

include(FetchContent)

function(disable_msvc_warnings target_name)
	if(MSVC AND TARGET ${target_name})
		target_compile_options(${target_name} PRIVATE /wd4244 /wd4267 /wd4305 /wd4996)
	endif()
endfunction()

if(NOT TARGET vorbisfile)
	find_package(Vorbis QUIET)
	if(TARGET Vorbis::vorbisfile)
		add_library(vorbisfile INTERFACE IMPORTED)
		target_link_libraries(vorbisfile INTERFACE Vorbis::vorbisfile)
	else()
		set(INSTALL_CMAKE_PACKAGE_MODULE OFF CACHE BOOL "" FORCE)
		set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
		set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "" FORCE)
		if(MSVC)
			set(OPUS_DISABLE_INTRINSICS ON CACHE BOOL "" FORCE)
		endif()

		FetchContent_Declare(
			ogg
			URL https://downloads.xiph.org/releases/ogg/libogg-1.3.6.tar.xz
			DOWNLOAD_EXTRACT_TIMESTAMP TRUE
		)
		FetchContent_Declare(
			vorbis
			URL https://downloads.xiph.org/releases/vorbis/libvorbis-1.3.7.tar.xz
			DOWNLOAD_EXTRACT_TIMESTAMP TRUE
		)

		FetchContent_MakeAvailable(ogg)
		disable_msvc_warnings(ogg)

		if(TARGET ogg)
			set(OGG_FOUND TRUE CACHE BOOL "" FORCE)
			set(OGG_INCLUDE_DIR "${ogg_SOURCE_DIR}/include" CACHE PATH "" FORCE)
			set(OGG_LIBRARY ogg CACHE STRING "" FORCE)
		endif()

		FetchContent_MakeAvailable(vorbis)
		disable_msvc_warnings(vorbis)
		disable_msvc_warnings(vorbisfile)
		disable_msvc_warnings(vorbisenc)
	endif()
endif()

if(NOT TARGET opusfile)
	find_package(OpusFile QUIET)
	if(TARGET OpusFile::opusfile)
		add_library(opusfile INTERFACE IMPORTED)
		target_link_libraries(opusfile INTERFACE OpusFile::opusfile)
	else()
		set(INSTALL_CMAKE_PACKAGE_MODULE OFF CACHE BOOL "" FORCE)
		set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
		set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "" FORCE)

		if(NOT TARGET ogg)
			FetchContent_Declare(
				ogg
				URL https://downloads.xiph.org/releases/ogg/libogg-1.3.6.tar.xz
				DOWNLOAD_EXTRACT_TIMESTAMP TRUE
			)
			FetchContent_MakeAvailable(ogg)
		endif()

		if(TARGET ogg)
			set(OGG_FOUND TRUE CACHE BOOL "" FORCE)
			set(OGG_INCLUDE_DIR "${ogg_SOURCE_DIR}/include" CACHE PATH "" FORCE)
			set(OGG_LIBRARY ogg CACHE STRING "" FORCE)
		endif()

		FetchContent_Declare(
			opus
			URL https://downloads.xiph.org/releases/opus/opus-1.6.1.tar.gz
			DOWNLOAD_EXTRACT_TIMESTAMP TRUE
		)
		FetchContent_Declare(
			opusfile_source
			URL https://downloads.xiph.org/releases/opus/opusfile-0.12.tar.gz
			DOWNLOAD_EXTRACT_TIMESTAMP TRUE
		)

		FetchContent_MakeAvailable(opus)

		FetchContent_MakeAvailable(opusfile_source)

		if(TARGET opus AND TARGET ogg)
			add_library(opusfile STATIC
				"${opusfile_source_SOURCE_DIR}/src/info.c"
				"${opusfile_source_SOURCE_DIR}/src/internal.c"
				"${opusfile_source_SOURCE_DIR}/src/opusfile.c"
				"${opusfile_source_SOURCE_DIR}/src/stream.c"
			)
			add_library(OpusFile::opusfile ALIAS opusfile)

			set_target_properties(opusfile PROPERTIES POSITION_INDEPENDENT_CODE ON)
			target_include_directories(opusfile PUBLIC
				"${opusfile_source_SOURCE_DIR}/include"
				"${opusfile_source_SOURCE_DIR}/src"
			)
			target_link_libraries(opusfile PUBLIC opus ogg)
			if(WIN32)
				target_compile_definitions(opusfile PRIVATE _CRT_SECURE_NO_WARNINGS)
			endif()
			disable_msvc_warnings(opusfile)
		endif()

		disable_msvc_warnings(opus)
	endif()
endif()

if (WASI)
	add_executable(extension "extension_js.c")
	set_target_properties(extension PROPERTIES SUFFIX ".js")
	target_link_options(extension PRIVATE
		-sWASM=1
		-sMODULARIZE=1
		"-sEXPORT_NAME=MiniaudioModule"
		-sALLOW_MEMORY_GROWTH=1
		"-sEXPORTED_FUNCTIONS=['_ma_js_init','_ma_js_uninit','_ma_js_describe_last_error','_ma_js_buffer_dispose','_ma_js_buffer_from_bytes','_ma_js_buffer_from_pcm_float','_ma_js_buffer_from_pcm_s16','_ma_js_buffer_get_length_samples','_ma_js_buffer_get_duration','_ma_js_buffer_get_duration_seconds','_ma_js_decode_pcm_float','_ma_js_decode_pcm_s16','_ma_js_decoded_channels','_ma_js_decoded_sample_rate','_ma_js_decoded_samples','_ma_js_sound_group_init','_ma_js_sound_group_dispose','_ma_js_sound_group_start','_ma_js_sound_group_stop','_ma_js_sound_group_get_volume','_ma_js_sound_group_set_volume','_ma_js_sound_group_get_pan','_ma_js_sound_group_set_pan','_ma_js_sound_group_get_pan_mode','_ma_js_sound_group_set_pan_mode','_ma_js_sound_group_get_pitch','_ma_js_sound_group_set_pitch','_ma_js_sound_group_get_spatialization_enabled','_ma_js_sound_group_set_spatialization_enabled','_ma_js_sound_init','_ma_js_sound_dispose','_ma_js_sound_start','_ma_js_sound_stop','_ma_js_sound_seek_samples','_ma_js_sound_seek_seconds','_ma_js_sound_seek_milliseconds','_ma_js_sound_get_cursor_samples','_ma_js_sound_is_playing','_ma_js_sound_get_volume','_ma_js_sound_set_volume','_ma_js_sound_get_pan','_ma_js_sound_set_pan','_ma_js_sound_get_pan_mode','_ma_js_sound_set_pan_mode','_ma_js_sound_get_pitch','_ma_js_sound_set_pitch','_ma_js_sound_get_spatialization_enabled','_ma_js_sound_set_spatialization_enabled','_ma_js_sound_get_time','_ma_js_sound_set_time','_ma_js_sound_get_time_seconds','_ma_js_sound_set_time_seconds','_ma_js_sound_get_duration','_ma_js_sound_get_duration_seconds','_ma_js_sound_get_length_samples','_ma_js_sound_set_end_callback_js','_ma_js_sound_clear_end_callback','_ma_js_update','_malloc','_free']"
		"-sEXPORTED_RUNTIME_METHODS=['ccall','getValue','UTF8ToString','HEAPU8']"
	)
else()
	add_library(extension SHARED "extension.c")
	set_target_properties(extension PROPERTIES SUFFIX ".hdll")
endif()

if(CMAKE_VERSION VERSION_GREATER 3.12)
	set_property(TARGET extension PROPERTY C_STANDARD 99)
	set_property(TARGET extension PROPERTY C_STANDARD_REQUIRED ON)
endif()

set_target_properties(extension PROPERTIES OUTPUT_NAME "miniaudio")

if (NOT WASI)
	if (WIN32)
		target_include_directories(extension PRIVATE "$ENV{HASHLINK}/include")
		target_link_libraries(extension PRIVATE "$ENV{HASHLINK}/libhl.lib")
	endif()
endif()

add_subdirectory("lib/miniaudio")
if(NOT TARGET miniaudio_libvorbis)
	message(FATAL_ERROR "libvorbis support is required, but miniaudio_libvorbis was not created")
endif()
if(NOT TARGET miniaudio_libopus)
	message(FATAL_ERROR "libopus support is required, but miniaudio_libopus was not created")
endif()

disable_msvc_warnings(miniaudio)
disable_msvc_warnings(miniaudio_libvorbis)
disable_msvc_warnings(miniaudio_libopus)
disable_msvc_warnings(extension)

target_include_directories(extension PRIVATE miniaudio miniaudio_common_options)
target_link_libraries(extension PRIVATE miniaudio miniaudio_common_options miniaudio_libvorbis miniaudio_libopus)