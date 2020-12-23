option( PLUGIN_STANDARD_QSOLIS "Install qSOLIS plugin" OFF )

if( PLUGIN_STANDARD_QSOLIS )
	project( QSOLIS_PLUGIN )
	
	find_package( OpenGL REQUIRED )
	if( NOT OPENGL_FOUND )
		message( FATAL_ERROR "OpenGL required by SOLIS plugin" )
	endif()
	
	AddPlugin( NAME ${PROJECT_NAME} )

	add_subdirectory( include )
	add_subdirectory( src )
	add_subdirectory( ui )
	
	target_include_directories( ${PROJECT_NAME} PRIVATE ${OpenGL_INCLUDE_DIR} )
	
	target_link_libraries( ${PROJECT_NAME} ${OPENGL_LIBRARIES} )
endif()
