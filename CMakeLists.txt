cmake_minimum_required(VERSION 3.18)
project(libpst_py LANGUAGES C CXX)

# This CMake project builds ONLY the Python extension module (libpst_py._core).
# The historical autotools build (configure.ac / Makefile.am) is untouched and
# remains the way readpst and friends are built. Here we compile the libpst C
# core straight into the extension so the wheel is self-contained.

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(CheckIncludeFile)
include(CheckFunctionExists)
include(CheckCSourceCompiles)

# --- platform feature detection (replaces autoconf for this build) ---
check_include_file(ctype.h      HAVE_CTYPE_H)
check_include_file(dirent.h     HAVE_DIRENT_H)
check_include_file(errno.h      HAVE_ERRNO_H)
check_include_file(limits.h     HAVE_LIMITS_H)
check_include_file(regex.h      HAVE_REGEX_H)
check_include_file(semaphore.h  HAVE_SEMAPHORE_H)
check_include_file(signal.h     HAVE_SIGNAL_H)
check_include_file(string.h     HAVE_STRING_H)
check_include_file(sys/ipc.h    HAVE_SYS_IPC_H)
check_include_file(sys/shm.h    HAVE_SYS_SHM_H)
check_include_file(sys/stat.h   HAVE_SYS_STAT_H)
check_include_file(sys/types.h  HAVE_SYS_TYPES_H)
check_include_file(sys/wait.h   HAVE_SYS_WAIT_H)
check_include_file(unistd.h     HAVE_UNISTD_H)
check_include_file(wchar.h      HAVE_WCHAR_H)
check_function_exists(get_current_dir_name HAVE_GET_CURRENT_DIR_NAME)

# --- zlib (required by the libpst core for compressed blocks) ---
find_package(ZLIB REQUIRED)

# --- iconv detection ---
# glibc provides iconv in libc; elsewhere it lives in a separate libiconv.
find_path(ICONV_INCLUDE_DIR iconv.h)
find_library(ICONV_LIB NAMES iconv libiconv)
set(ICONV_LIBRARIES "")
if(ICONV_INCLUDE_DIR)
  # Is iconv in libc, or do we need to link a separate library?
  check_c_source_compiles("
    #include <iconv.h>
    int main(void){ iconv_t c = iconv_open(\"UTF-8\",\"UTF-8\"); iconv_close(c); return 0; }
  " ICONV_IN_LIBC)
  if(ICONV_IN_LIBC)
    set(HAVE_ICONV 1)
  elseif(ICONV_LIB)
    set(HAVE_ICONV 1)
    set(ICONV_LIBRARIES ${ICONV_LIB})
  endif()
endif()

# Determine whether iconv's inbuf argument is const.
if(HAVE_ICONV)
  set(CMAKE_REQUIRED_INCLUDES ${ICONV_INCLUDE_DIR})
  set(CMAKE_REQUIRED_LIBRARIES ${ICONV_LIBRARIES})
  check_c_source_compiles("
    #include <iconv.h>
    // Fails to compile if the 2nd arg is (char**) instead of (const char**)
    int main(void){
      iconv_t cd = 0;
      const char *in = 0; size_t il = 0; char *out = 0; size_t ol = 0;
      iconv(cd, &in, &il, &out, &ol);
      return 0;
    }
  " ICONV_SECOND_ARG_IS_CONST)
  if(ICONV_SECOND_ARG_IS_CONST)
    set(ICONV_CONST "const")
  else()
    set(ICONV_CONST "")
  endif()
  unset(CMAKE_REQUIRED_INCLUDES)
  unset(CMAKE_REQUIRED_LIBRARIES)
endif()

if(NOT HAVE_ICONV)
  message(FATAL_ERROR "libpst-py requires iconv (system libc or libiconv)")
endif()

configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/config.h.in
  ${CMAKE_CURRENT_BINARY_DIR}/generated/config.h
)

# --- pybind11 ---
find_package(pybind11 CONFIG REQUIRED)

set(LIBPST_CORE
  src/debug.c
  src/libpst.c
  src/libstrfunc.c
  src/lzfu.c
  src/timeconv.c
  src/vbuf.c
)

pybind11_add_module(_core
  python/pybind_libpst.cpp
  ${LIBPST_CORE}
)

target_compile_definitions(_core PRIVATE HAVE_CONFIG_H)
target_include_directories(_core PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/generated
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${ZLIB_INCLUDE_DIRS}
)
if(ICONV_INCLUDE_DIR)
  target_include_directories(_core PRIVATE ${ICONV_INCLUDE_DIR})
endif()
target_link_libraries(_core PRIVATE ${ZLIB_LIBRARIES} ${ICONV_LIBRARIES})

# The libpst core is C with plenty of legacy casts; keep its own warnings quiet
# here (the -Werror gate lives in the autotools/Linux CI job, not the wheel).
if(NOT MSVC)
  set_source_files_properties(${LIBPST_CORE} PROPERTIES COMPILE_OPTIONS "-w")
endif()

install(TARGETS _core DESTINATION libpst_py)
