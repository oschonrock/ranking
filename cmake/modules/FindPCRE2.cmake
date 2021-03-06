# - Find pcre
# Find the native PCRE2 headers and libraries.
#
# PCRE2_INCLUDE_DIRS	- where to find pcre.h, etc.
# PCRE2_LIBRARIES	- List of libraries when using pcre.
# PCRE2_FOUND	- True if pcre found.

# Look for the header file.
FIND_PATH(PCRE2_INCLUDE_DIR pcre2.h)

# Look for the library.
FIND_LIBRARY(PCRE2_LIBRARY NAMES libpcre2.a pcre2-8)

# Handle the QUIETLY and REQUIRED arguments and set PCRE_FOUND to TRUE if all listed variables are TRUE.
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(PCRE2 DEFAULT_MSG PCRE2_LIBRARY PCRE2_INCLUDE_DIR)

# Copy the results to the output variables.

MARK_AS_ADVANCED(PCRE2_INCLUDE_DIRS PCRE2_LIBRARIES)

if(PCRE2_FOUND)
  set(PCRE2_LIBRARIES ${PCRE2_LIBRARY})
  set(PCRE2_INCLUDE_DIRS ${PCRE2_INCLUDE_DIR})

  if(NOT TARGET PCRE2::PCRE2)
    add_library(PCRE2::PCRE2 UNKNOWN IMPORTED)
    set_target_properties(PCRE2::PCRE2 PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES "${PCRE2_INCLUDE_DIRS}")

    if(EXISTS "${PCRE2_LIBRARY}")
      set_target_properties(PCRE2::PCRE2 PROPERTIES
        IMPORTED_LINK_INTERFACE_LANGUAGES "C"
        IMPORTED_LOCATION "${PCRE2_LIBRARY}")
    endif()
    if(PCRE2_LIBRARY_RELEASE)
      set_property(TARGET PCRE2::PCRE2 APPEND PROPERTY
        IMPORTED_CONFIGURATIONS RELEASE)
      set_target_properties(PCRE2::PCRE2 PROPERTIES
        IMPORTED_LINK_INTERFACE_LANGUAGES "C"
        IMPORTED_LOCATION_RELEASE "${PCRE2_LIBRARY_RELEASE}")
    endif()
    if(PCRE2_LIBRARY_DEBUG)
      set_property(TARGET PCRE2::PCRE2 APPEND PROPERTY
        IMPORTED_CONFIGURATIONS DEBUG)
      set_target_properties(PCRE2::PCRE2 PROPERTIES
        IMPORTED_LINK_INTERFACE_LANGUAGES "C"
        IMPORTED_LOCATION_DEBUG "${PCRE2_LIBRARY_DEBUG}")
    endif()
  endif()
endif()
