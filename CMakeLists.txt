cmake_minimum_required(VERSION 3.16)
project(EShot VERSION 2.4.3 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/version.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/version.h
    @ONLY
)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network)

set(SOURCES
    src/main.cpp
    src/core/HotkeyManager.cpp
    src/core/OcrEngine.cpp
    src/core/ImageUploader.cpp
    src/core/CatboxUploader.cpp
    src/core/UguuUploader.cpp
    src/capture/CaptureOverlay.cpp
    src/capture/PinnedWindow.cpp
    src/capture/PinManager.cpp
    src/annotation/AnnotationEngine.cpp
    src/ui/AnnotationToolbar.cpp
    src/ui/SettingsDialog.cpp
    src/ui/AboutDialog.cpp
    src/ui/FirstRunWizard.cpp
    src/ui/OcrDialog.cpp
    src/ui/UploadDialog.cpp
    src/recording/GifEncoder.cpp
    src/recording/ScreenRecorder.cpp
    src/recording/RecordingIndicator.cpp
)

set(HEADERS
    src/core/HotkeyManager.h
    src/core/OcrEngine.h
    src/core/ImageUploader.h
    src/core/CatboxUploader.h
    src/core/UguuUploader.h
    src/capture/CaptureOverlay.h
    src/capture/PinnedWindow.h
    src/capture/PinManager.h
    src/annotation/AnnotationEngine.h
    src/ui/AnnotationToolbar.h
    src/ui/SettingsDialog.h
    src/ui/AboutDialog.h
    src/ui/FirstRunWizard.h
    src/ui/OcrDialog.h
    src/ui/UploadDialog.h
    src/recording/GifEncoder.h
    src/recording/ScreenRecorder.h
    src/recording/RecordingIndicator.h
)

set(RESOURCES
    resources/icons.qrc
)

# Embed admin manifest into the executable (MSVC auto-embeds .manifest files
# listed as sources; for MinGW we use a .rc file)
if(MINGW)
    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/EShot.manifest
        ${CMAKE_CURRENT_BINARY_DIR}/EShot.manifest
        COPYONLY
    )
    set(RC_FILE ${CMAKE_CURRENT_BINARY_DIR}/EShot_manifest.rc)
    file(WRITE ${RC_FILE} "1 24 \"${CMAKE_CURRENT_BINARY_DIR}/EShot.manifest\"\n")
    list(APPEND SOURCES ${RC_FILE})
elseif(MSVC)
    list(APPEND SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/app.rc)
endif()

qt_add_executable(${PROJECT_NAME} WIN32
    ${SOURCES}
    ${HEADERS}
    ${RESOURCES}
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    WIN32_EXECUTABLE TRUE
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    User32
    Gdi32
    Shell32
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Embed manifest after build
if(MSVC)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND mt.exe -manifest ${CMAKE_CURRENT_SOURCE_DIR}/EShot.manifest -outputresource:$<TARGET_FILE:${PROJECT_NAME}>\;1
        COMMENT "Embedding manifest into executable"
    )
endif()

install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)

# Bundle Tesseract OCR (third_party/tesseract/) next to the executable so the
# app can find tesseract.exe + tessdata at runtime without requiring a system
# install. Skip silently if the folder is missing.
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/third_party/tesseract/tesseract.exe)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/third_party/tesseract
            $<TARGET_FILE_DIR:${PROJECT_NAME}>/tesseract
        COMMENT "Copying bundled Tesseract OCR to output directory"
    )
    install(DIRECTORY third_party/tesseract
        DESTINATION bin
        FILES_MATCHING PATTERN "*"
    )
endif()
