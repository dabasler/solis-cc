add_executable( qsolis_solar_model_tests
    test_solar_model.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../src/SOLISSolarModel.cpp
)

target_include_directories( qsolis_solar_model_tests
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/../include
)

target_link_libraries( qsolis_solar_model_tests PRIVATE CCCoreLib )
target_compile_features( qsolis_solar_model_tests PRIVATE cxx_std_17 )
set_target_properties( qsolis_solar_model_tests PROPERTIES AUTOMOC OFF AUTORCC OFF AUTOUIC OFF )

add_test( NAME qsolis_solar_model COMMAND qsolis_solar_model_tests )
