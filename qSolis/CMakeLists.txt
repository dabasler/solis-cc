option( PLUGIN_STANDARD_QSOLIS "Install qSOLIS plugin" OFF )
option( SOLIS_BUILD_TESTS "Build qSOLIS solar-model tests" OFF )

if( PLUGIN_STANDARD_QSOLIS )
    project( QSOLIS_PLUGIN VERSION 1.0.0 )

    find_package( OpenGL REQUIRED )
    find_package( Threads REQUIRED )

    AddPlugin( NAME ${PROJECT_NAME} )

    add_subdirectory( include )
    add_subdirectory( src )
    add_subdirectory( ui )

    target_include_directories( ${PROJECT_NAME} PRIVATE ${OpenGL_INCLUDE_DIR} )
    target_link_libraries( ${PROJECT_NAME} ${OPENGL_LIBRARIES} Threads::Threads )

    if( SOLIS_BUILD_TESTS )
        if( BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_LIST_DIR}/tests/CMakeLists.txt" )
            add_subdirectory( tests )
        else()
            message( WARNING "SOLIS_BUILD_TESTS=ON requires BUILD_TESTING=ON and the qSOLIS tests directory" )
        endif()
    endif()
endif()
