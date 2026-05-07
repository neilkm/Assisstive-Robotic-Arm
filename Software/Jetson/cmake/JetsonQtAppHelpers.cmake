include(CMakeParseArguments)

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

    if(APPLE)
        set_target_properties(${target_name} PROPERTIES
            MACOSX_BUNDLE TRUE
        )
    endif()
endfunction()
