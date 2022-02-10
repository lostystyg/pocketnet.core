# Allow user to pass a path instead of guessing
if(MINIUPNPC_ROOT)
    set(_MINIUPNPC_PATHS_MANUAL "${MINIUPNPC_ROOT}")
elseif(CMAKE_SYSTEM_NAME MATCHES ".*[wW]indows.*")
    # MATCHES is used to work on any devies with windows in the name
    # Shameless copy-paste from FindOpenSSL.cmake v3.8
    file(TO_CMAKE_PATH "$ENV{PROGRAMFILES}" _programfiles)
    list(APPEND _MINIUPNPC_HINTS "${_programfiles}")

    # TODO probably other default paths?
    foreach(_TARGET_MINIUPNPC_PATH "libminiupnpc")
        list(APPEND _MINIUPNPC_PATHS
                "${_programfiles}/${_TARGET_MINIUPNPC_PATH}"
                "C:/Program Files (x86)/${_TARGET_MINIUPNPC_PATH}"
                "C:/Program Files/${_TARGET_MINIUPNPC_PATH}"
                "C:/${_TARGET_MINIUPNPC_PATH}"
                )
    endforeach()
else()
    # Paths for anything other than Windows
    list(APPEND _MINIUPNPC_PATHS
            "/usr"
            "/usr/local"
            #            "/usr/local/Cellar/libminiupnpc" TODO: mac os?
            "/opt"
            "/opt/local"
            )
endif()
if(_MINIUPNPC_PATHS_MANUAL)
    find_path(Miniupnpc_INCLUDE_DIRS
            NAMES "miniupnpc/miniupnpc.h"
            PATH_SUFFIXES "include" "includes"
            PATHS ${_MINIUPNPC_PATHS_MANUAL}
            NO_DEFAULT_PATH
            )
else()
    find_path(Miniupnpc_INCLUDE_DIRS
            NAMES "miniupnpc.h"
            HINTS ${_MINIUPNPC_HINTS}
            PATH_SUFFIXES "include/miniupnpc" "includes/miniupnpc"
            PATHS ${_MINIUPNPC_PATHS}
            )
endif()
# Find includes path


# Checks if the version file exists, save the version file to a var, and fail if there's no version file
if(NOT Miniupnpc_INCLUDE_DIRS)
    if(MINIUPNPC_FIND_REQUIRED)
        message(FATAL_ERROR "Failed to find libminiupnpc's header file \"miniupnpc.h\"! Try setting \"MINIUPNPC_ROOT\" when initiating Cmake.")
    elseif(NOT MINIUPNPC_FIND_QUIETLY)
        message(WARNING "Failed to find libminiupnpc's header file \"miniupnpc.h\"! Try setting \"MINIUPNPC_ROOT\" when initiating Cmake.")
    endif()
    # Set some garbage values to the versions since we didn't find a file to read
endif()

# Finds the target library for miniupnpc, since they all follow the same naming conventions
macro(findpackage_miniupnpc_get_lib _MINIUPNPC_OUTPUT_VARNAME _TARGET_MINIUPNPC_LIB)
    # Different systems sometimes have a version in the lib name...
    # and some have a dash or underscore before the versions.
    # CMake recommends to put unversioned names before versioned names
    if(_MINIUPNPC_PATHS_MANUAL)
        find_library(${_MINIUPNPC_OUTPUT_VARNAME}
                NAMES
                "${_TARGET_MINIUPNPC_LIB}"
                "lib${_TARGET_MINIUPNPC_LIB}"
                PATH_SUFFIXES "lib" "lib64" "libs" "libs64"
                PATHS ${_MINIUPNPC_PATHS_MANUAL}
                NO_DEFAULT_PATH
                )
    else()
        find_library(${_MINIUPNPC_OUTPUT_VARNAME}
                NAMES
                "${_TARGET_MINIUPNPC_LIB}"
                "lib${_TARGET_MINIUPNPC_LIB}"
                HINTS ${_MINIUPNPC_HINTS}
                PATH_SUFFIXES "lib" "lib64" "libs" "libs64"
                PATHS ${_MINIUPNPC_PATHS}
                )
    endif()

    # If the library was found, add it to our list of libraries
    if(${_MINIUPNPC_OUTPUT_VARNAME})
        # If found, append to our libraries variable
        # The ${{}} is because the first expands to target the real variable, the second expands the variable's contents...
        # and the real variable's contents is the path to the lib. Thus, it appends the path of the lib to Miniupnpc_LIBRARIES.
        list(APPEND Miniupnpc_LIBRARIES "${${_MINIUPNPC_OUTPUT_VARNAME}}")
    endif()
endmacro()

# Find and set the paths of the specific library to the variable
findpackage_miniupnpc_get_lib(MINIUPNPC_LIB "miniupnpc")

# Needed for find_package_handle_standard_args()
include(FindPackageHandleStandardArgs)
# Fails if required vars aren't found, or if the version doesn't meet specifications.
find_package_handle_standard_args(Miniupnpc
        FOUND_VAR Miniupnpc_FOUND
        REQUIRED_VARS
            Miniupnpc_INCLUDE_DIRS
            MINIUPNPC_LIB
            Miniupnpc_LIBRARIES
        )

# Create an imported lib for easy linking by external projects
if(Miniupnpc_FOUND AND Miniupnpc_LIBRARIES AND NOT TARGET Miniupnpc::miniupnpc)
    add_library(Miniupnpc::miniupnpc INTERFACE IMPORTED)
    set_target_properties(Miniupnpc::miniupnpc PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${Miniupnpc_INCLUDE_DIRS}"
            IMPORTED_LOCATION "${MINIUPNPC_LIB}"
            INTERFACE_LINK_LIBRARIES "${Miniupnpc_LIBRARIES}"
            )
endif()

include(FindPackageMessage)
# A message that tells the user what includes/libs were found, and obeys the QUIET command.
find_package_message(Miniupnpc
        "Found miniupnpc libraries: ${Miniupnpc_LIBRARIES}"
        "[${Miniupnpc_LIBRARIES}[${Miniupnpc_INCLUDE_DIRS}]]"
        )