include(CMakeParseArguments)

get_filename_component(JETSON_REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(JETSON_BUILD_OUTPUT_DIR "${JETSON_REPO_ROOT}/builds/Jetson" CACHE PATH
    "Central output directory for Jetson build products")

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${JETSON_BUILD_OUTPUT_DIR}/lib")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${JETSON_BUILD_OUTPUT_DIR}/lib")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${JETSON_BUILD_OUTPUT_DIR}/bin")

function(jetsonqt_set_output_directory target_name output_kind)
    if(output_kind STREQUAL "APP")
        set(output_dir "${JETSON_BUILD_OUTPUT_DIR}/apps/${target_name}")
    elseif(output_kind STREQUAL "TEST")
        set(output_dir "${JETSON_BUILD_OUTPUT_DIR}/tests/${target_name}")
    else()
        set(output_dir "${JETSON_BUILD_OUTPUT_DIR}/bin/${target_name}")
    endif()

    set_target_properties(${target_name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${output_dir}"
        BUNDLE_OUTPUT_DIRECTORY "${output_dir}"
    )

    foreach(config DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
        set_target_properties(${target_name} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY_${config} "${output_dir}"
            BUNDLE_OUTPUT_DIRECTORY_${config} "${output_dir}"
        )
    endforeach()
endfunction()

function(jetsonqt_add_library target_name)
    set(options "")
    set(one_value_args "")
    set(multi_value_args
        SOURCES
        PUBLIC_HEADERS
        PUBLIC_LIBS
        PRIVATE_LIBS
        PUBLIC_INCLUDE_DIRS
        PRIVATE_INCLUDE_DIRS
    )

    cmake_parse_arguments(ARG
        "${options}"
        "${one_value_args}"
        "${multi_value_args}"
        ${ARGN}
    )

    add_library(${target_name} STATIC
        ${ARG_SOURCES}
        ${ARG_PUBLIC_HEADERS}
    )

    add_library(JetsonQtApp::${target_name} ALIAS ${target_name})

    target_include_directories(${target_name}
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            ${ARG_PUBLIC_INCLUDE_DIRS}
        PRIVATE
            ${ARG_PRIVATE_INCLUDE_DIRS}
    )

    target_link_libraries(${target_name}
        PUBLIC
            ${ARG_PUBLIC_LIBS}
        PRIVATE
            ${ARG_PRIVATE_LIBS}
    )
endfunction()

function(jetsonqt_add_app target_name)
    set(options "")
    set(one_value_args "")
    set(multi_value_args SOURCES LIBS)

    cmake_parse_arguments(ARG
        "${options}"
        "${one_value_args}"
        "${multi_value_args}"
        ${ARGN}
    )

    add_executable(${target_name}
        ${ARG_SOURCES}
    )

    target_link_libraries(${target_name}
        PRIVATE
            ${ARG_LIBS}
    )

    jetsonqt_set_output_directory(${target_name} APP)

    if(APPLE)
        set_target_properties(${target_name} PROPERTIES
            MACOSX_BUNDLE TRUE
        )
    endif()
endfunction()

function(jetsonqt_add_qtest target_name)
    set(options "")
    set(one_value_args "")
    set(multi_value_args SOURCES LIBS)

    cmake_parse_arguments(ARG
        "${options}"
        "${one_value_args}"
        "${multi_value_args}"
        ${ARGN}
    )

    add_executable(${target_name}
        ${ARG_SOURCES}
    )

    target_link_libraries(${target_name}
        PRIVATE
            Qt${QT_VERSION_MAJOR}::Test
            ${ARG_LIBS}
    )

    jetsonqt_set_output_directory(${target_name} TEST)
    add_test(NAME ${target_name} COMMAND ${target_name})
endfunction()
