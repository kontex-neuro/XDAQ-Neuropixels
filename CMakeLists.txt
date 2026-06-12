cmake_minimum_required(VERSION 3.25.0)
cmake_policy(SET CMP0087 NEW)
if (NOT DEFINED GUI_BASE_DIR)
	if (DEFINED ENV{GUI_BASE_DIR})
		set(GUI_BASE_DIR $ENV{GUI_BASE_DIR})
	else()
		set(GUI_BASE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../plugin-GUI)
	endif()
endif()
get_filename_component(GUI_BASE_DIR ${GUI_BASE_DIR} ABSOLUTE)

set(PLUGIN_NAME XDAQ-Neuropixels)
project(${PLUGIN_NAME} VERSION 0.5.0 LANGUAGES CXX)

option(XDAQ_NP_BUILD_INSTALLER "Configure a CPack/NSIS Windows installer" OFF)

set(GUI_COMMONLIB_DIR ${GUI_BASE_DIR}/installed_libs)

set(CONFIGURATION_FOLDER $<$<CONFIG:Debug>:Debug>$<$<CONFIG:RelWithDebInfo>:RelWithDebInfo>$<$<CONFIG:Release>:Release> )

list(APPEND CMAKE_PREFIX_PATH ${GUI_COMMONLIB_DIR} ${GUI_COMMONLIB_DIR}/${CONFIGURATION_FOLDER})

# Create plugin target
set(SOURCE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/Source)
file(GLOB_RECURSE SRC_FILES LIST_DIRECTORIES false "${SOURCE_PATH}/*.cpp" "${SOURCE_PATH}/*.h")
if (APPLE)
	add_library(${PLUGIN_NAME} MODULE ${SRC_FILES})
else()
	add_library(${PLUGIN_NAME} SHARED ${SRC_FILES})
endif()

target_compile_definitions(${PLUGIN_NAME} PRIVATE
    OEPLUGIN
    $<$<PLATFORM_ID:Linux>:JUCE_DISABLE_NATIVE_FILECHOOSERS=1>
    $<$<CONFIG:Debug>:DEBUG=1>
    $<$<CONFIG:Debug>:_DEBUG=1>
    $<$<CONFIG:Release>:NDEBUG=1>
	$<$<PLATFORM_ID:Linux>:OS_LINUX>
	$<$<PLATFORM_ID:Windows>:OS_WINDOWS>
	$<$<PLATFORM_ID:Darwin>:OS_MACOS>
	PLUGIN_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
	PLUGIN_VERSION_MINOR=${PROJECT_VERSION_MINOR}
	PLUGIN_VERSION_PATCH=${PROJECT_VERSION_PATCH}
)

target_compile_features(${PLUGIN_NAME} PUBLIC cxx_auto_type cxx_generalized_initializers cxx_std_23)
target_include_directories(${PLUGIN_NAME} PUBLIC ${GUI_BASE_DIR}/JuceLibraryCode ${GUI_BASE_DIR}/JuceLibraryCode/modules ${GUI_BASE_DIR}/Plugins/Headers ${GUI_COMMONLIB_DIR}/include)

#Libraries and compiler options
find_package(fmt REQUIRED)
find_package(libxdaqnp REQUIRED)
target_link_libraries(${PLUGIN_NAME} PRIVATE
	fmt::fmt
	xdaq::xdaq_npapi
)

set_target_properties(${PLUGIN_NAME} PROPERTIES CXX_VISIBILITY_PRESET hidden)
if(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
	target_compile_definitions(${PLUGIN_NAME} PRIVATE
		NOMINMAX
		WIN32_LEAN_AND_MEAN
		NOGDI
		"JUCE_API=__declspec(dllimport)"
	)
	target_link_libraries(${PLUGIN_NAME} PRIVATE
		${GUI_BASE_DIR}/Build/${CONFIGURATION_FOLDER}/open-ephys.lib
	)
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
	target_link_libraries(${PLUGIN_NAME} PRIVATE
		GL X11 Xext Xinerama asound dl freetype pthread rt
	)
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
	set_target_properties(${PLUGIN_NAME} PROPERTIES BUNDLE TRUE)
	target_link_options(${PLUGIN_NAME} PRIVATE -undefined dynamic_lookup)
endif()


# Install plugin target
set(PLUGIN_INSTALL_DIR plugins CACHE STRING "Plugin installation directory")
set(SHARED_INSTALL_DIR shared CACHE STRING "Shared installation directory")

# When packaging the standalone installer, CMAKE_INSTALL_PREFIX is set explicitly
# (so the default-prefix block below is skipped). Force the api10 layout here so the
# packaged tree matches what the Open Ephys GUI expects.
if(XDAQ_NP_BUILD_INSTALLER)
	set(PLUGIN_INSTALL_DIR plugins-api10 CACHE STRING "Plugin installation directory" FORCE)
	set(SHARED_INSTALL_DIR shared-api10 CACHE STRING "Shared installation directory" FORCE)
endif()

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
	if (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
		set_property(CACHE CMAKE_INSTALL_PREFIX PROPERTY VALUE C:/ProgramData/Open\ Ephys)
		set(PLUGIN_INSTALL_DIR plugins-api10 CACHE STRING "Plugin installation directory" FORCE)
		set(SHARED_INSTALL_DIR shared-api10 CACHE STRING "Shared installation directory" FORCE)
	elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
		set_property(CACHE CMAKE_INSTALL_PREFIX PROPERTY VALUE $ENV{HOME}/Library/Application\ Support/open-ephys)
		set(PLUGIN_INSTALL_DIR plugins-api10 CACHE STRING "Plugin installation directory" FORCE)
		set(SHARED_INSTALL_DIR shared-api10 CACHE STRING "Shared installation directory" FORCE)
	elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
		set_property(CACHE CMAKE_INSTALL_PREFIX PROPERTY VALUE ${GUI_BASE_DIR}/Build)
		set(PLUGIN_INSTALL_DIR ${CONFIGURATION_FOLDER}/plugins CACHE STRING "Plugin installation directory" FORCE)
		set(SHARED_INSTALL_DIR ${CONFIGURATION_FOLDER}/shared CACHE STRING "Shared installation directory" FORCE)
	endif()
endif()

install(TARGETS ${PLUGIN_NAME}
	LIBRARY DESTINATION ${PLUGIN_INSTALL_DIR}
	RUNTIME DESTINATION ${PLUGIN_INSTALL_DIR}
)

if(${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
	set_target_properties(${PLUGIN_NAME} PROPERTIES
		BUILD_WITH_INSTALL_RPATH TRUE
		INSTALL_RPATH "@loader_path/../Frameworks"
	)
	libxdaqnp_install_runtime(
		PLUGIN_DEST  ${PLUGIN_INSTALL_DIR}/${PLUGIN_NAME}.bundle/Contents/Frameworks
		SHARED_DEST  ${PLUGIN_INSTALL_DIR}/${PLUGIN_NAME}.bundle/Contents/Frameworks
		MANAGERS_DEST ${PLUGIN_INSTALL_DIR}/${PLUGIN_NAME}.bundle/Contents/Frameworks/device_managers
	)
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
	set_target_properties(${PLUGIN_NAME} PROPERTIES
		BUILD_WITH_INSTALL_RPATH TRUE
		INSTALL_RPATH "$ORIGIN/../shared/${PLUGIN_NAME}"
	)
	libxdaqnp_install_runtime(
		PLUGIN_DEST  ${PLUGIN_INSTALL_DIR}
		SHARED_DEST  ${SHARED_INSTALL_DIR}/${PLUGIN_NAME}
		MANAGERS_DEST ${SHARED_INSTALL_DIR}/${PLUGIN_NAME}/device_managers
	)
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
	libxdaqnp_install_runtime(
		PLUGIN_DEST  ${SHARED_INSTALL_DIR}
		SHARED_DEST  ${SHARED_INSTALL_DIR}
		MANAGERS_DEST ${SHARED_INSTALL_DIR}/${PLUGIN_NAME}/device_managers
	)
endif()


# ----------------------------------------------------------------------------
# Standalone Windows installer (CPack + NSIS)
#
# Produces a self-contained installer .exe for customers who cannot install the
# plugin through the Open Ephys GUI plugin installer. It drops the plugin and its
# runtime into the per-user Open Ephys data directory:
#     %LOCALAPPDATA%\Open Ephys\plugins-api10
#     %LOCALAPPDATA%\Open Ephys\shared-api10
#
# Requires NSIS (https://nsis.sourceforge.io) on the build machine.
#
# Build it (after configuring with -DXDAQ_NP_BUILD_INSTALLER=ON):
#     cmake --build build/Release --config Release --target package
# ----------------------------------------------------------------------------
if(XDAQ_NP_BUILD_INSTALLER AND ${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
	set(CPACK_GENERATOR "NSIS")
	set(CPACK_PACKAGE_NAME "XDAQ-Neuropixels")
	set(CPACK_PACKAGE_VENDOR "KonteX Neuroscience")
	set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
	set(CPACK_PACKAGE_FILE_NAME "XDAQ-Neuropixels-${PROJECT_VERSION}-windows-x64-installer")
	set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "XDAQ-Neuropixels plugin for the Open Ephys GUI")

	# Install under %LOCALAPPDATA%\Open Ephys (the per-user Open Ephys data dir).
	# Full target path = ${CPACK_NSIS_INSTALL_ROOT}\${CPACK_PACKAGE_INSTALL_DIRECTORY}.
	set(CPACK_NSIS_INSTALL_ROOT "$LOCALAPPDATA")
	set(CPACK_PACKAGE_INSTALL_DIRECTORY "Open Ephys")

	set(CPACK_NSIS_DISPLAY_NAME "XDAQ-Neuropixels Plugin")
	set(CPACK_NSIS_PACKAGE_NAME "XDAQ-Neuropixels Plugin ${PROJECT_VERSION}")
	# Remove a previously installed version before installing the new one.
	set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
	# This is a drop-in for an existing data dir; don't offer to modify PATH.
	set(CPACK_NSIS_MODIFY_PATH OFF)

	include(CPack)
endif()

