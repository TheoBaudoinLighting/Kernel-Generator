cmake_minimum_required(VERSION 3.20)

project(
    KernelGenerator
    VERSION 1.0.0
    DESCRIPTION "Procedural bokeh kernel generator"
    LANGUAGES CXX
)

add_executable(kernel_generator
    bokeh_kernel_system.cpp
)

target_compile_features(kernel_generator PRIVATE cxx_std_17)

set_target_properties(kernel_generator PROPERTIES
    CXX_EXTENSIONS OFF
    OUTPUT_NAME kernel_generator
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

foreach(CONFIG DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set_target_properties(kernel_generator PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY_${CONFIG} "${CMAKE_BINARY_DIR}/bin"
    )
endforeach()

if(MSVC)
    target_compile_definitions(kernel_generator PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

include(GNUInstallDirs)

install(TARGETS kernel_generator
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
