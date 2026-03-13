cmake_minimum_required(VERSION 3.10)

# Uncomment this and rebuild artifacts to enable debugging
set(CMAKE_BUILD_TYPE Debug)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(slothmodel VERSION 1.0.0 DESCRIPTION "Simple Logical Tautology Handler (SLoTH) Model Shared Library")

# https://www.intel.com/content/www/us/en/develop/documentation/oneapi-dpcpp-cpp-compiler-dev-guide-and-reference/top/compiler-setup/use-the-command-line/use-cmake-with-the-compiler.html
# Incidentally... MUST COME AFTER `project(...)`!
if (INTEL_DPCPP)
    #cmake_minimum_required(VERSION 3.20)
    find_package(IntelDPCPP REQUIRED)
endif()

if(WIN32)
    add_library(slothmodel src/sloth.cpp)
else()
    add_library(slothmodel SHARED src/sloth.cpp)
endif()

include_directories(PRIVATE include)
#target_include_directories(slothmodel PRIVATE include)
#Where to look for bmi header, depending on where cmake is run from
include_directories(PRIVATE extern/bmi-cxx)
#target_include_directories(slothmodel PRIVATE ../bmi-cxx/ )

# -----------------------------------------------------------------------------
# Find the Boost library and configure usage
set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)
find_package(Boost 1.79.0 REQUIRED COMPONENTS serialization)

target_link_libraries(slothmodel PRIVATE Boost::serialization)

set_target_properties(slothmodel PROPERTIES VERSION ${PROJECT_VERSION})

set_target_properties(slothmodel PROPERTIES PUBLIC_HEADER include/sloth.hpp)

include(GNUInstallDirs)

install(TARGETS slothmodel
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

configure_file(slothmodel.pc.in slothmodel.pc @ONLY)

option(PACKAGE_TESTS "Build the tests" ON)
if(PACKAGE_TESTS)
    enable_testing()
    include(GoogleTest)
    add_subdirectory(test)
endif()

install(FILES ${CMAKE_BINARY_DIR}/slothmodel.pc DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/pkgconfig)
