cmake_minimum_required(VERSION 3.25)
project(dune_daq_arrow_frame_hdf VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Suppress spurious GCC 14/15 header warnings (else fatal under -Werror).
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "14.1")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "15")
      add_compile_options(-Wno-stringop-overflow)
    endif()
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "16")
      add_compile_options(-Wno-array-bounds)
    endif()
  endif()
endif()

# Link the Spack view's gcc15 libstdc++ explicitly (GLIBCXX/CXXABI resolution),
# same pattern as wire-cell-arrow / dune-daq-wct.
set(DDA_RUNTIME_LIBS "")
foreach(_prefix IN LISTS CMAKE_PREFIX_PATH)
  if(EXISTS "${_prefix}/lib/libstdc++.so.6")
    link_directories(BEFORE "${_prefix}/lib")
    file(REAL_PATH "${_prefix}/lib/libstdc++.so.6" _stdc_real)
    get_filename_component(_stdc_dir "${_stdc_real}" DIRECTORY)
    set(DDA_RUNTIME_LIBS "${_stdc_real}")
    string(APPEND CMAKE_EXE_LINKER_FLAGS " -Wl,-rpath,${_stdc_dir}")
    break()
  endif()
endforeach()

# WireCell / wire-cell-arrow exported targets reference these but their Configs
# do not find them, so find them first (same as wire-cell-phlex-arrow).
find_package(Arrow CONFIG REQUIRED)
find_package(Boost REQUIRED COMPONENTS json)
find_package(spdlog REQUIRED)

# wire_cell_arrow first: its Config puts FindWireCell.cmake on the module path,
# so the WireCell-dependent Configs below resolve.
find_package(wire_cell_arrow CONFIG REQUIRED)
find_package(arrow_hdf CONFIG REQUIRED)
find_package(dune_daq_hdf CONFIG REQUIRED)
find_package(dune_daq_codec CONFIG REQUIRED)

add_compile_options(-Wall -Wpedantic -Werror)

# The DUNE-DAQ -> WCT bridge (ToFrame): formerly the standalone dune-daq-wct
# package, folded in here as its only client and renamed into this package's
# own dune_daq_arrow_frame_hdf namespace / header path. Built as an internal
# static lib shared by the app and the tests below.
add_library(daq_to_frame STATIC dune_daq_arrow_frame_hdf/ToFrame.cpp)
target_include_directories(daq_to_frame PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(daq_to_frame
  PUBLIC
    dune_daq_codec::dune_daq_codec
    WireCell::Aux WireCell::Iface WireCell::Util
    ${DDA_RUNTIME_LIBS})

# ---------------------------------------------------------------------------
# daq_hdf_to_arrow — the end-to-end DAQ-HDF5 -> wc.frame-in-HDF5 pipeline tool
# (ddm-3j8.1.16). Non-Phlex glue across all the pieces; produces the same
# wc.frame Arrow/HDF5 representation the Phlex pipeline writes.
# ---------------------------------------------------------------------------
add_executable(daq_hdf_to_arrow app/daq_hdf_to_arrow.cpp)
target_link_libraries(daq_hdf_to_arrow
  PRIVATE
    dune_daq_hdf::dune_daq_hdf
    dune_daq_codec::dune_daq_codec
    daq_to_frame
    wire_cell_arrow::wire_cell_arrow
    arrow_hdf::arrow_hdf
    ${DDA_RUNTIME_LIBS})

enable_testing()
add_subdirectory(test)

include(GNUInstallDirs)
install(TARGETS daq_hdf_to_arrow RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
