CMAKE_MINIMUM_REQUIRED (VERSION 2.8.11)

SET(PROJ_AUTHOR Tannin)
SET(PROJ_ARCH x86)

macro(add_msvc_precompiled_header PrecompiledHeader PrecompiledSource SourcesVar HeadersVar)
  if(MSVC)
    get_filename_component(PrecompiledBasename ${PrecompiledHeader} NAME_WE)
    set(PrecompiledBinary "${CMAKE_CURRENT_BINARY_DIR}/${PrecompiledBasename}.pch")
    set(Sources ${${SourcesVar}})

    set_source_files_properties(
		${PrecompiledSource} PROPERTIES
		COMPILE_FLAGS "/Yc\"${PrecompiledHeader}\" /Fp\"${PrecompiledBinary}\""
		OBJECT_OUTPUTS "${PrecompiledBinary}")

    set_source_files_properties(
		${Sources} PROPERTIES
		COMPILE_FLAGS "/Yu\"${PrecompiledHeader}\" /FI\"${PrecompiledHeader}\" /Fp\"${PrecompiledBinary}\""
		OBJECT_DEPENDS "${PrecompiledBinary}")

    list(APPEND ${SourcesVar} ${PrecompiledSource})
	list(APPEND ${HeadersVar} ${PrecompiledHeader})
  endif(MSVC)
endmacro(add_msvc_precompiled_header)


set(MOSrcRoot ../src)
set(MOSources)
set(MOHeaders)
set(TestSources)
set(TestHeaders)

macro(AddMOSource name)
	set(cpp ${MOSrcRoot}/${name}.cpp)
	set(h ${MOSrcRoot}/${name}.h)

	if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${cpp})
		set(MOSources ${MOSources} ${cpp})
	endif()

	if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${h})
		set(MOHeaders ${MOHeaders} ${h})
	endif()

	source_group(src FILES ${cpp} ${h})
endmacro()

macro(AddTestSource name)
	set(cpp ${name}.cpp)
	set(h ${name}.h)

	if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${cpp})
		set(TestSources ${TestSources} ${cpp})
	endif()

	if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${h})
		set(TestHeaders ${TestHeaders} ${h})
	endif()

	source_group(tests FILES ${cpp} ${h})
endmacro()

AddMOSource(envfs)
AddMOSource(fileentry)
AddMOSource(originconnection)
AddMOSource(fileregister)
AddMOSource(filesorigin)
AddMOSource(directoryentry)
AddMOSource(directorystructure)
AddMOSource(fileregisterfwd)
AddMOSource(util)

AddTestSource(main)
AddTestSource(directoryentry)
AddTestSource(fileentry)
AddTestSource(fileregister)
AddTestSource(filesorigin)
AddTestSource(originconnection)

set(AllSources ${MOSources} ${TestSources})
set(AllHeaders ${MOHeaders} ${TestHeaders})

add_msvc_precompiled_header("pch.h" "pch.cpp" AllSources AllHeaders)
source_group(src FILES pch.cpp pch.h)

# Qt5
SET(CMAKE_INCLUDE_CURRENT_DIR ON)
SET(CMAKE_AUTOMOC ON)
SET(CMAKE_AUTOUIC ON)
SET(CMAKE_AUTORCC ON)

FIND_PACKAGE(Qt5Widgets REQUIRED)
FIND_PACKAGE(Qt5QuickWidgets REQUIRED)
FIND_PACKAGE(Qt5Quick REQUIRED)
FIND_PACKAGE(Qt5Network REQUIRED)
FIND_PACKAGE(Qt5WinExtras REQUIRED)
FIND_PACKAGE(Qt5WebEngineWidgets REQUIRED)
FIND_PACKAGE(Qt5WebSockets REQUIRED)
FIND_PACKAGE(Qt5Qml REQUIRED)
FIND_PACKAGE(Qt5LinguistTools)

SET(mo_translation_sources ${CMAKE_SOURCE_DIR}/src ${project_path}/uibase/src)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY AUTOGEN_SOURCE_GROUP autogen)
set_property(GLOBAL PROPERTY AUTOMOC_SOURCE_GROUP autogen)
set_property(GLOBAL PROPERTY AUTORCC_SOURCE_GROUP autogen)

INCLUDE_DIRECTORIES(${Qt5Declarative_INCLUDES})

ADD_DEFINITIONS(-DQT_MESSAGELOGCONTEXT)

# Boost
SET(Boost_USE_STATIC_LIBS       OFF)
SET(Boost_USE_MULTITHREADED      ON)
SET(Boost_USE_STATIC_RUNTIME    OFF)
FIND_PACKAGE(Boost REQUIRED)

INCLUDE_DIRECTORIES(${Boost_INCLUDE_DIRS})
LINK_DIRECTORIES(${Boost_LIBRARY_DIRS})

FIND_PACKAGE(zlib REQUIRED)
find_package(fmt REQUIRED)

set(GTEST_ROOT C:/dev/projects/modorganizer/build/googletest/googletest)
find_package(gtest REQUIRED)

INCLUDE_DIRECTORIES(${project_path}/modorganizer/src
					${project_path}/uibase/src
                    ${project_path}/bsatk/src
                    ${project_path}/esptk/src
                    ${project_path}/archive/src
					${project_path}/lootcli/include
                    ${dependency_project_path}/usvfs/include
                    ${project_path}/game_gamebryo/src/gamebryo
                    ${project_path}/game_gamebryo/src/creation
                    ${project_path}/game_features/src
                    ${project_path}/githubpp/src
					${SPDLOG_ROOT}/include
                    ${LZ4_ROOT}/lib)

INCLUDE_DIRECTORIES(${ZLIB_INCLUDE_DIRS})
include_directories(${GTEST_INCLUDE_DIR})

#TODO this should not be a hardcoded path
SET(lib_path "${project_path}/../../install/libs")
LINK_DIRECTORIES(${lib_path}
                 ${ZLIB_ROOT}/lib
                 ${LZ4_ROOT}/bin)
EXECUTE_PROCESS(
  COMMAND git log -1 --format=%h
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  OUTPUT_VARIABLE GIT_COMMIT_HASH
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

ADD_DEFINITIONS(-D_UNICODE -DUNICODE -DNOMINMAX -D_CRT_SECURE_NO_WARNINGS -DBOOST_CONFIG_SUPPRESS_OUTDATED_MESSAGE -DGITID="${GIT_COMMIT_HASH}")

IF("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")
  SET(usvfs_name usvfs_x64)
ELSE()
  SET(usvfs_name usvfs_x86)
ENDIF()

ADD_EXECUTABLE(ModOrganizerTests ${AllSources} ${AllHeaders})

TARGET_LINK_LIBRARIES(ModOrganizerTests
                      Qt5::Widgets Qt5::WinExtras Qt5::WebEngineWidgets Qt5::Quick
                      Qt5::Qml Qt5::QuickWidgets Qt5::Network Qt5::WebSockets
                      ${Boost_LIBRARIES} fmt::fmt
                      zlibstatic
                      uibase esptk bsatk githubpp
                      ${usvfs_name}
                      Dbghelp advapi32 Version Shlwapi liblz4 Crypt32
					  ${GTEST_BOTH_LIBRARIES})

set(GTEST_ROOT C:/dev/projects/modorganizer/build/googletest)

IF (MSVC)
  SET_TARGET_PROPERTIES(ModOrganizerTests PROPERTIES COMPILE_FLAGS "/std:c++latest /permissive-")
ENDIF()
IF (MSVC AND "${CMAKE_SIZEOF_VOID_P}" EQUAL 4)
  # 32 bits
  SET_TARGET_PROPERTIES(ModOrganizerTests PROPERTIES LINK_FLAGS "/LARGEADDRESSAWARE")
ENDIF()

IF (NOT "${OPTIMIZE_COMPILE_FLAGS}" STREQUAL "")
  SET_TARGET_PROPERTIES(ModOrganizerTests PROPERTIES COMPILE_FLAGS_RELWITHDEBINFO ${OPTIMIZE_COMPILE_FLAGS})
ENDIF()
IF (NOT "${OPTIMIZE_LINK_FLAGS}" STREQUAL "")
  SET_TARGET_PROPERTIES(ModOrganizerTests PROPERTIES LINK_FLAGS_RELWITHDEBINFO ${OPTIMIZE_LINK_FLAGS})
ENDIF()
