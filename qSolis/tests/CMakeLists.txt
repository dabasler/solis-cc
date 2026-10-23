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

add_executable( qsolis_exposure_math_tests test_exposure_math.cpp )
target_include_directories( qsolis_exposure_math_tests PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../include )
target_compile_features( qsolis_exposure_math_tests PRIVATE cxx_std_17 )
set_target_properties( qsolis_exposure_math_tests PROPERTIES AUTOMOC OFF AUTORCC OFF AUTOUIC OFF )
add_test( NAME qsolis_exposure_math COMMAND qsolis_exposure_math_tests )

add_executable( qsolis_crown_exposure_math_tests test_crown_exposure_math.cpp )
target_include_directories( qsolis_crown_exposure_math_tests PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../include )
target_compile_features( qsolis_crown_exposure_math_tests PRIVATE cxx_std_17 )
set_target_properties( qsolis_crown_exposure_math_tests PROPERTIES AUTOMOC OFF AUTORCC OFF AUTOUIC OFF )
add_test( NAME qsolis_crown_exposure_math COMMAND qsolis_crown_exposure_math_tests )

add_executable( qsolis_crown_export_tests
    test_crown_export.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../src/SOLISCrownExport.cpp
)
target_include_directories( qsolis_crown_export_tests PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../include )
target_link_libraries( qsolis_crown_export_tests PRIVATE Qt6::Core )
target_compile_features( qsolis_crown_export_tests PRIVATE cxx_std_17 )
set_target_properties( qsolis_crown_export_tests PROPERTIES AUTOMOC OFF AUTORCC OFF AUTOUIC OFF )
add_test( NAME qsolis_crown_export COMMAND qsolis_crown_export_tests )
set_tests_properties( qsolis_crown_export PROPERTIES TIMEOUT 30 )
