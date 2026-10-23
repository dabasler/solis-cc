target_sources( ${PROJECT_NAME}
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/SOLIS.h
        ${CMAKE_CURRENT_LIST_DIR}/SOLISBatch.h
        ${CMAKE_CURRENT_LIST_DIR}/SOLISCommand.h
        ${CMAKE_CURRENT_LIST_DIR}/SOLISCleanupDlg.h
        ${CMAKE_CURRENT_LIST_DIR}/SOLISCleanupCommand.h
        ${CMAKE_CURRENT_LIST_DIR}/SOLISResults.h
        ${CMAKE_CURRENT_LIST_DIR}/SOLISBackend.h
        ${CMAKE_CURRENT_LIST_DIR}/SOLISContext.h
        ${CMAKE_CURRENT_LIST_DIR}/SOLISSolarModel.h
        ${CMAKE_CURRENT_LIST_DIR}/SOLISVersion.h
        ${CMAKE_CURRENT_LIST_DIR}/ccSolisDlg.h
        ${CMAKE_CURRENT_LIST_DIR}/SOLISExposure.h
        ${CMAKE_CURRENT_LIST_DIR}/SOLISExposureMath.h
        ${CMAKE_CURRENT_LIST_DIR}/SOLISCrownExposure.h
        ${CMAKE_CURRENT_LIST_DIR}/SOLISCrownExposureMath.h
        ${CMAKE_CURRENT_LIST_DIR}/SOLISCrownExport.h
        ${CMAKE_CURRENT_LIST_DIR}/SOLISCrownExportCommand.h
        ${CMAKE_CURRENT_LIST_DIR}/SOLISExposureCommand.h
        ${CMAKE_CURRENT_LIST_DIR}/ccSolisExposureDlg.h
        ${CMAKE_CURRENT_LIST_DIR}/qSOLIS.h
)

target_include_directories( ${PROJECT_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)
