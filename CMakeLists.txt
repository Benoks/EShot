cmake_minimum_required(VERSION 3.16)
project(EShot VERSION 4.5.1 LANGUAGES CXX)

include(CTest)

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

set(ESHOT_QT_COMPONENTS Core Gui Widgets Network)
if(UNIX AND NOT APPLE)
    list(APPEND ESHOT_QT_COMPONENTS DBus)
endif()
find_package(Qt6 REQUIRED COMPONENTS ${ESHOT_QT_COMPONENTS})
find_package(Threads REQUIRED)
if(UNIX AND NOT APPLE)
    find_package(X11)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBSECRET REQUIRED IMPORTED_TARGET libsecret-1)
endif()

set(SOURCES
    src/main.cpp
    src/core/HotkeyManager.cpp
    src/core/HotkeySequence.cpp
    src/core/ApplicationInstanceCommand.cpp
    src/core/OcrEngine.cpp
    src/core/OcrLanguageSelector.cpp
    src/core/LinuxPortalGlobalShortcuts.cpp
    src/core/LinuxPortalScreenCast.cpp
    src/core/LinuxPortalScreenshot.cpp
    src/core/LinuxDesktopIntegration.cpp
    src/core/LinuxGnomeShortcutInstaller.cpp
    src/core/LinuxPortalRequest.cpp
    src/core/LinuxPortalHostRegistry.cpp
    src/core/LinuxAutoStartPolicy.cpp
    src/core/LinuxScreenshotPolicy.cpp
    src/core/ImageUploader.cpp
    src/core/SecureCredentialStore.cpp
    src/core/UploadResponseParser.cpp
    src/core/VisualSearch.cpp
    src/core/UpdateAssetSelector.cpp
    src/core/UpdatePolicy.cpp
    src/core/WindowsInstallPolicy.cpp
    src/core/ComponentPaths.cpp
    src/core/WindowsElevatedStartup.cpp
    src/ui/OnboardingTips.cpp
    src/ui/HintBubble.cpp
    src/capture/ShortcutSheet.cpp
    src/core/LinuxUpdateScript.cpp
    src/core/LinuxDependencySelection.cpp
    src/core/CatboxUploader.cpp
    src/core/UguuUploader.cpp
    src/core/UpdateManager.cpp
    src/core/NotificationFolderOpener.cpp
    src/core/DebouncedSettingsWriter.cpp
    src/capture/CaptureGeometry.cpp
    src/capture/CaptureScreenImage.cpp
    src/capture/CaptureScreenViews.cpp
    src/capture/CaptureInteractionPolicy.cpp
    src/capture/WindowSnapPolicy.cpp
    src/capture/WindowsWindowProvider.cpp
    src/capture/CaptureOverlay.cpp
    src/capture/PinnedWindow.cpp
    src/annotation/AnnotationEngine.cpp
    src/annotation/ImageEffects.cpp
    src/annotation/AnnotationRotationGeometry.cpp
    src/ui/AnnotationToolbar.cpp
    src/ui/ApplicationTheme.cpp
    src/ui/OverlayPanelStyle.cpp
    src/ui/SettingsDialog.cpp
    src/ui/SettingsHotkeyPolicy.cpp
    src/ui/SettingsLayoutPolicy.cpp
    src/ui/AboutDialog.cpp
    src/ui/ControlCenterDialog.cpp
    src/ui/FirstRunWizard.cpp
    src/ui/OcrDialog.cpp
    src/ui/UploadDialog.cpp
    src/recording/GifEncoder.cpp
    src/recording/LinuxRecordingSupport.cpp
    src/recording/RecordingSettingsPolicy.cpp
    src/recording/RecordingDrawerPolicy.cpp
    src/recording/VideoRecordingCompletionPolicy.cpp
    src/recording/RecordingControlLayout.cpp
    src/recording/RecordingTimeline.cpp
    src/recording/RecordingFinalizationPolicy.cpp
    src/recording/ScreenRecorder.cpp
    src/recording/AudioDevices.cpp
    src/recording/VideoRecorder.cpp
    src/recording/RecordingIndicator.cpp
)

set(HEADERS
    src/core/HotkeyManager.h
    src/core/HotkeySequence.h
    src/core/ApplicationInstanceCommand.h
    src/core/PlatformHotkey.h
    src/core/OcrEngine.h
    src/core/OcrLanguageSelector.h
    src/core/LinuxPortalGlobalShortcuts.h
    src/core/LinuxPortalScreenCast.h
    src/core/LinuxPortalScreenshot.h
    src/core/LinuxDesktopIntegration.h
    src/core/LinuxGnomeShortcutInstaller.h
    src/core/LinuxPortalRequest.h
    src/core/LinuxPortalHostRegistry.h
    src/core/LinuxAutoStartPolicy.h
    src/core/LinuxScreenshotPolicy.h
    src/core/ImageUploader.h
    src/core/SecureCredentialStore.h
    src/core/UploadResponseParser.h
    src/core/VisualSearch.h
    src/core/UpdateAssetSelector.h
    src/core/UpdatePolicy.h
    src/core/WindowsInstallPolicy.h
    src/core/ComponentPaths.h
    src/core/WindowsElevatedStartup.h
    src/ui/OnboardingTips.h
    src/ui/HintBubble.h
    src/capture/ShortcutSheet.h
    src/core/LinuxUpdateScript.h
    src/core/LinuxDependencySelection.h
    src/core/CatboxUploader.h
    src/core/UguuUploader.h
    src/core/UpdateManager.h
    src/core/NotificationFolderOpener.h
    src/core/DebouncedSettingsWriter.h
    src/capture/CaptureGeometry.h
    src/capture/CaptureScreenImage.h
    src/capture/CaptureScreenViews.h
    src/capture/CaptureInteractionPolicy.h
    src/capture/WindowSnapPolicy.h
    src/capture/WindowsWindowProvider.h
    src/capture/CaptureOverlay.h
    src/capture/PinnedWindow.h
    src/annotation/AnnotationEngine.h
    src/annotation/ImageEffects.h
    src/annotation/AnnotationRotationGeometry.h
    src/ui/AnnotationToolbar.h
    src/ui/ApplicationTheme.h
    src/ui/OverlayPanelStyle.h
    src/ui/SettingsDialog.h
    src/ui/SettingsHotkeyPolicy.h
    src/ui/SettingsLayoutPolicy.h
    src/ui/AboutDialog.h
    src/ui/ControlCenterDialog.h
    src/ui/FirstRunWizard.h
    src/ui/OcrDialog.h
    src/ui/UploadDialog.h
    src/recording/GifEncoder.h
    src/recording/LinuxRecordingSupport.h
    src/recording/RecordingSettingsPolicy.h
    src/recording/RecordingDrawerPolicy.h
    src/recording/VideoRecordingCompletionPolicy.h
    src/recording/RecordingControlLayout.h
    src/recording/RecordingTimeline.h
    src/recording/RecordingFinalizationPolicy.h
    src/recording/ScreenRecorder.h
    src/recording/AudioDevices.h
    src/recording/VideoRecorder.h
    src/recording/RecordingIndicator.h
)

set(RESOURCES
    resources/icons.qrc
)

if(UNIX AND NOT APPLE)
    list(APPEND SOURCES src/core/LinuxKGlobalAccelShortcuts.cpp)
    list(APPEND HEADERS src/core/LinuxKGlobalAccelShortcuts.h)
    list(APPEND SOURCES src/core/LinuxDesktopNotification.cpp)
    list(APPEND HEADERS src/core/LinuxDesktopNotification.h)
endif()

# Embed the application manifest into the executable (MSVC auto-embeds .manifest files
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

if(WIN32)
    set(ESHOT_EXECUTABLE_OPTIONS WIN32)
endif()

qt_add_executable(${PROJECT_NAME} ${ESHOT_EXECUTABLE_OPTIONS}
    ${SOURCES}
    ${HEADERS}
    ${RESOURCES}
)

if(WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Threads::Threads
)

if(UNIX AND NOT APPLE)
    target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::DBus PkgConfig::LIBSECRET)
endif()

if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        User32
        Gdi32
        Shell32
        Dwmapi
        Advapi32
        Ole32
        OleAut32
    )
endif()

if(UNIX AND NOT APPLE AND X11_FOUND)
    target_compile_definitions(${PROJECT_NAME} PRIVATE ESHOT_HAVE_X11=1)
    target_include_directories(${PROJECT_NAME} PRIVATE ${X11_INCLUDE_DIR})
    target_link_libraries(${PROJECT_NAME} PRIVATE ${X11_LIBRARIES})
endif()

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
        # mt.exe is supplied by the Visual Studio developer environment. Some
        # CMake generators leave CMAKE_MT unset even though mt.exe is on PATH.
        COMMAND mt.exe -manifest ${CMAKE_CURRENT_SOURCE_DIR}/EShot.manifest -outputresource:$<TARGET_FILE:${PROJECT_NAME}>\;1
        COMMENT "Embedding manifest into executable"
    )
endif()

# Bundle FFmpeg for MP4 screen recording when a local binary is provided.
# Expected layout: third_party/ffmpeg/bin/ffmpeg.exe
if(WIN32 AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/third_party/ffmpeg/bin/ffmpeg.exe)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/third_party/ffmpeg/bin
            $<TARGET_FILE_DIR:${PROJECT_NAME}>/ffmpeg
        COMMENT "Copying bundled FFmpeg to output directory"
    )
    install(DIRECTORY third_party/ffmpeg/bin/
        DESTINATION bin/ffmpeg
        FILES_MATCHING PATTERN "*"
    )
endif()

install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)

if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)

    # eshot_add_test(<target> <test name> SOURCES <files...> LIBS <libs...>)
    # Builds a Qt Test executable with src/ on the include path, links LIBS
    # plus Qt6::Test and registers it with CTest under <test name>.
    function(eshot_add_test target test_name)
        cmake_parse_arguments(ARG "" "" "SOURCES;LIBS" ${ARGN})
        qt_add_executable(${target} ${ARG_SOURCES})
        target_include_directories(${target} PRIVATE src)
        target_link_libraries(${target} PRIVATE ${ARG_LIBS} Qt6::Test)
        add_test(NAME ${test_name} COMMAND ${target})
    endfunction()

    eshot_add_test(eshot_annotation_engine_tests annotation_engine
        SOURCES
            tests/AnnotationEngineTests.cpp
            src/annotation/AnnotationEngine.cpp
            src/annotation/AnnotationEngine.h
            src/annotation/ImageEffects.cpp
            src/annotation/ImageEffects.h
        LIBS Qt6::Core Qt6::Gui
    )

    eshot_add_test(eshot_annotation_rotation_geometry_tests annotation_rotation_geometry
        SOURCES
            tests/AnnotationRotationGeometryTests.cpp
            src/annotation/AnnotationRotationGeometry.cpp
            src/annotation/AnnotationRotationGeometry.h
        LIBS Qt6::Core
    )

    eshot_add_test(eshot_recording_drawer_policy_tests recording_drawer_policy
        SOURCES
            tests/RecordingDrawerPolicyTests.cpp
            src/recording/RecordingDrawerPolicy.cpp
            src/recording/RecordingDrawerPolicy.h
        LIBS Qt6::Core
    )

    eshot_add_test(eshot_overlay_panel_style_tests overlay_panel_style
        SOURCES
            tests/OverlayPanelStyleTests.cpp
            src/ui/OverlayPanelStyle.cpp
            src/ui/OverlayPanelStyle.h
        LIBS Qt6::Core Qt6::Widgets
    )

    eshot_add_test(eshot_video_recording_completion_policy_tests video_recording_completion_policy
        SOURCES
            tests/VideoRecordingCompletionPolicyTests.cpp
            src/recording/VideoRecordingCompletionPolicy.cpp
            src/recording/VideoRecordingCompletionPolicy.h
        LIBS Qt6::Core
    )

    eshot_add_test(eshot_application_instance_command_tests application_instance_command
        SOURCES
            tests/ApplicationInstanceCommandTests.cpp
            src/core/ApplicationInstanceCommand.cpp
            src/core/ApplicationInstanceCommand.h
        LIBS Qt6::Core
    )

    eshot_add_test(eshot_visual_search_tests visual_search
        SOURCES
            tests/VisualSearchTests.cpp
            src/core/VisualSearch.cpp
            src/core/VisualSearch.h
        LIBS Qt6::Core
    )

    eshot_add_test(eshot_upload_response_parser_tests upload_response_parser
        SOURCES
            tests/UploadResponseParserTests.cpp
            src/core/UploadResponseParser.cpp
            src/core/UploadResponseParser.h
        LIBS Qt6::Core
    )

    eshot_add_test(eshot_secure_credential_store_tests secure_credential_store
        SOURCES
            tests/SecureCredentialStoreTests.cpp
            src/core/SecureCredentialStore.cpp
            src/core/SecureCredentialStore.h
        LIBS Qt6::Core
    )
    if(UNIX AND NOT APPLE)
        target_link_libraries(eshot_secure_credential_store_tests PRIVATE PkgConfig::LIBSECRET)
    endif()
    if(WIN32)
        target_link_libraries(eshot_secure_credential_store_tests PRIVATE Advapi32)
    endif()

    eshot_add_test(eshot_settings_translation_tests settings_translation
        SOURCES
            tests/SettingsTranslationTests.cpp
        LIBS Qt6::Core
    )

    eshot_add_test(eshot_capture_geometry_tests capture_geometry
        SOURCES
            tests/CaptureGeometryTests.cpp
            src/capture/CaptureGeometry.cpp
            src/capture/CaptureGeometry.h
        LIBS Qt6::Core
    )

    eshot_add_test(eshot_capture_interaction_policy_tests capture_interaction_policy
        SOURCES
            tests/CaptureInteractionPolicyTests.cpp
            src/capture/CaptureInteractionPolicy.cpp
            src/capture/CaptureInteractionPolicy.h
        LIBS Qt6::Core Qt6::Gui
    )

    eshot_add_test(eshot_capture_screen_image_tests capture_screen_image
        SOURCES
            tests/CaptureScreenImageTests.cpp
            src/capture/CaptureScreenImage.cpp
            src/capture/CaptureScreenImage.h
            src/capture/CaptureGeometry.cpp
            src/capture/CaptureGeometry.h
        LIBS Qt6::Core Qt6::Gui
    )

    eshot_add_test(eshot_capture_screen_views_tests capture_screen_views
        SOURCES
            tests/CaptureScreenViewsTests.cpp
            src/capture/CaptureScreenViews.cpp
            src/capture/CaptureScreenViews.h
            src/capture/CaptureScreenImage.cpp
            src/capture/CaptureGeometry.cpp
        LIBS Qt6::Core Qt6::Gui Qt6::Widgets
    )
    set_tests_properties(capture_screen_views PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

    eshot_add_test(eshot_debounced_settings_writer_tests debounced_settings_writer
        SOURCES
            tests/DebouncedSettingsWriterTests.cpp
            src/core/DebouncedSettingsWriter.cpp
            src/core/DebouncedSettingsWriter.h
        LIBS Qt6::Core
    )

    eshot_add_test(eshot_window_snap_policy_tests window_snap_policy
        SOURCES
            tests/WindowSnapPolicyTests.cpp
            src/capture/WindowSnapPolicy.cpp
            src/capture/WindowSnapPolicy.h
            src/capture/WindowsWindowProvider.cpp
            src/capture/WindowsWindowProvider.h
            src/capture/CaptureGeometry.cpp
            src/capture/CaptureGeometry.h
        LIBS Qt6::Core
    )
    if(WIN32)
        target_link_libraries(eshot_window_snap_policy_tests PRIVATE User32 Dwmapi)
    endif()

    eshot_add_test(eshot_recording_settings_policy_tests recording_settings_policy
        SOURCES
            tests/RecordingSettingsPolicyTests.cpp
            src/recording/RecordingSettingsPolicy.cpp
            src/recording/RecordingSettingsPolicy.h
        LIBS Qt6::Core
    )

    eshot_add_test(eshot_recording_finalization_policy_tests recording_finalization_policy
        SOURCES
            tests/RecordingFinalizationPolicyTests.cpp
            src/recording/RecordingFinalizationPolicy.cpp
            src/recording/RecordingFinalizationPolicy.h
        LIBS Qt6::Core
    )

    eshot_add_test(eshot_settings_layout_policy_tests settings_layout_policy
        SOURCES
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/SettingsLayoutPolicyTests.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/SettingsLayoutPolicy.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/SettingsLayoutPolicy.h
        LIBS Qt6::Core
    )

    eshot_add_test(eshot_settings_hotkey_policy_tests settings_hotkey_policy
        SOURCES
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/SettingsHotkeyPolicyTests.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/SettingsHotkeyPolicy.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/SettingsHotkeyPolicy.h
        LIBS Qt6::Core
    )

    eshot_add_test(eshot_application_theme_tests application_theme
        SOURCES
            tests/ApplicationThemeTests.cpp
            src/ui/ApplicationTheme.cpp
            src/ui/ApplicationTheme.h
        LIBS Qt6::Core Qt6::Gui Qt6::Widgets
    )

    eshot_add_test(eshot_about_dialog_tests about_dialog
        SOURCES
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/AboutDialogTests.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/AboutDialog.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/AboutDialog.h
            ${CMAKE_CURRENT_SOURCE_DIR}/resources/icons.qrc
        LIBS Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network
    )

    eshot_add_test(eshot_control_center_dialog_tests control_center_dialog
        SOURCES
            tests/ControlCenterDialogTests.cpp
            src/ui/ControlCenterDialog.cpp
            src/ui/ControlCenterDialog.h
            resources/icons.qrc
        LIBS Qt6::Core Qt6::Gui Qt6::Widgets
    )
    set_tests_properties(control_center_dialog PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

    eshot_add_test(eshot_update_asset_tests update_asset
        SOURCES
            tests/UpdateAssetSelectorTests.cpp
            src/core/UpdateAssetSelector.cpp
            src/core/UpdateAssetSelector.h
        LIBS Qt6::Core
    )

    eshot_add_test(eshot_update_policy_tests update_policy
        SOURCES
            tests/UpdatePolicyTests.cpp
            src/core/UpdatePolicy.cpp
            src/core/UpdatePolicy.h
        LIBS Qt6::Core
    )

    eshot_add_test(eshot_windows_install_policy_tests windows_install_policy
        SOURCES
            tests/WindowsInstallPolicyTests.cpp
            src/core/WindowsInstallPolicy.cpp
            src/core/WindowsInstallPolicy.h
        LIBS Qt6::Core
    )

    eshot_add_test(eshot_linux_update_script_tests linux_update_script
        SOURCES
            tests/LinuxUpdateScriptTests.cpp
            src/core/LinuxUpdateScript.cpp
            src/core/LinuxUpdateScript.h
        LIBS Qt6::Core
    )

    eshot_add_test(eshot_linux_dependency_selection_tests linux_dependency_selection
        SOURCES
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/LinuxDependencySelectionTests.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/src/core/LinuxDependencySelection.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/src/core/LinuxDependencySelection.h
        LIBS Qt6::Core
    )

    eshot_add_test(eshot_ocr_language_selector_tests ocr_language_selector
        SOURCES
            tests/OcrLanguageSelectorTests.cpp
            src/core/OcrLanguageSelector.cpp
            src/core/OcrLanguageSelector.h
        LIBS Qt6::Core
    )

    if(UNIX AND NOT APPLE)
        eshot_add_test(eshot_linux_gnome_shortcut_installer_tests linux_gnome_shortcut_installer
            SOURCES
                tests/LinuxGnomeShortcutInstallerTests.cpp
                src/core/LinuxGnomeShortcutInstaller.cpp
                src/core/LinuxGnomeShortcutInstaller.h
            LIBS Qt6::Core
        )

        eshot_add_test(eshot_linux_autostart_policy_tests linux_autostart_policy
            SOURCES
                tests/LinuxAutoStartPolicyTests.cpp
                src/core/LinuxAutoStartPolicy.cpp
                src/core/LinuxAutoStartPolicy.h
                ${CMAKE_CURRENT_SOURCE_DIR}/src/core/LinuxDesktopIntegration.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/src/core/LinuxDesktopIntegration.h
            LIBS Qt6::Core
        )

        eshot_add_test(eshot_linux_screenshot_policy_tests linux_screenshot_policy
            SOURCES
                tests/LinuxScreenshotPolicyTests.cpp
                src/core/LinuxPortalScreenshot.cpp
                src/core/LinuxPortalScreenshot.h
                ${CMAKE_CURRENT_SOURCE_DIR}/src/core/LinuxDesktopIntegration.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/src/core/LinuxDesktopIntegration.h
                ${CMAKE_CURRENT_SOURCE_DIR}/src/core/LinuxPortalRequest.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/src/core/LinuxPortalRequest.h
                src/core/LinuxScreenshotPolicy.cpp
                src/core/LinuxScreenshotPolicy.h
            LIBS Qt6::Core Qt6::Gui Qt6::Widgets Qt6::DBus
        )
        set_tests_properties(linux_screenshot_policy PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

        eshot_add_test(eshot_linux_portal_shortcut_tests linux_portal_shortcut
            SOURCES
                tests/LinuxPortalShortcutTests.cpp
                src/core/LinuxPortalGlobalShortcuts.cpp
                src/core/LinuxPortalGlobalShortcuts.h
                src/core/LinuxGnomeShortcutInstaller.cpp
                src/core/LinuxGnomeShortcutInstaller.h
                src/core/LinuxKGlobalAccelShortcuts.cpp
                src/core/LinuxKGlobalAccelShortcuts.h
                src/core/LinuxPortalScreenCast.cpp
                src/core/LinuxPortalScreenCast.h
                ${CMAKE_CURRENT_SOURCE_DIR}/src/core/LinuxDesktopIntegration.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/src/core/LinuxDesktopIntegration.h
                ${CMAKE_CURRENT_SOURCE_DIR}/src/core/LinuxPortalRequest.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/src/core/LinuxPortalRequest.h
                ${CMAKE_CURRENT_SOURCE_DIR}/src/core/LinuxPortalHostRegistry.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/src/core/LinuxPortalHostRegistry.h
                src/core/PlatformHotkey.h
            LIBS Qt6::Core Qt6::Gui Qt6::Widgets
        )
        target_link_libraries(eshot_linux_portal_shortcut_tests PRIVATE Qt6::DBus)

        add_test(
            NAME linux_packagekit_installer
            COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/linux/packagekit-installer-tests.sh
        )

        if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/linux/apprun-integration-tests.sh)
            add_test(
                NAME linux_apprun_integration
                COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/linux/apprun-integration-tests.sh
            )
        endif()

        if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/linux/aur-package-contract-tests.sh)
            add_test(
                NAME linux_aur_package_contract
                COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/linux/aur-package-contract-tests.sh
            )
        endif()

        if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/linux/package-contract-tests.sh)
            add_test(
                NAME linux_package_contract
                COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/linux/package-contract-tests.sh
            )
        endif()

        if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/linux/runtime-common-tests.sh)
            add_test(
                NAME linux_runtime_common
                COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/linux/runtime-common-tests.sh
            )
        endif()

        if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/linux/local-runner-overlay-contract-tests.sh)
            add_test(
                NAME linux_local_runner_overlay_contract
                COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/linux/local-runner-overlay-contract-tests.sh
            )
        endif()

        if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/linux/native-wayland-overlay-tests.sh)
            add_test(
                NAME linux_native_wayland_overlay
                COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/linux/native-wayland-overlay-tests.sh
            )
        endif()

        if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/linux/annotation-gesture-wiring-tests.sh)
            add_test(
                NAME linux_annotation_gesture_wiring
                COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/linux/annotation-gesture-wiring-tests.sh
            )
        endif()

        if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/linux/recording-drawer-input-contract-tests.sh)
            add_test(
                NAME linux_recording_drawer_input_contract
                COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/linux/recording-drawer-input-contract-tests.sh
            )
        endif()
    endif()

    eshot_add_test(eshot_linux_recording_support_tests linux_recording_support
        SOURCES
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/LinuxRecordingSupportTests.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/src/recording/LinuxRecordingSupport.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/src/recording/LinuxRecordingSupport.h
        LIBS Qt6::Core
    )

    eshot_add_test(eshot_recording_control_layout_tests recording_control_layout
        SOURCES
            tests/RecordingControlLayoutTests.cpp
            src/recording/RecordingControlLayout.cpp
            src/recording/RecordingControlLayout.h
        LIBS Qt6::Core
    )

    eshot_add_test(eshot_recording_indicator_tests recording_indicator
        SOURCES
            tests/RecordingIndicatorTests.cpp
            src/recording/RecordingIndicator.cpp
            src/recording/RecordingIndicator.h
            src/recording/RecordingControlLayout.cpp
            src/recording/RecordingControlLayout.h
            resources/icons.qrc
        LIBS Qt6::Core Qt6::Gui Qt6::Widgets
    )
    set_tests_properties(recording_indicator PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

    eshot_add_test(eshot_recording_timeline_tests recording_timeline
        SOURCES
            tests/RecordingTimelineTests.cpp
            src/recording/RecordingTimeline.cpp
            src/recording/RecordingTimeline.h
        LIBS Qt6::Core
    )

    eshot_add_test(eshot_gif_encoder_tests gif_encoder
        SOURCES
            tests/GifEncoderTests.cpp
            src/recording/GifEncoder.cpp
            src/recording/GifEncoder.h
        LIBS Qt6::Core Qt6::Gui
    )

    eshot_add_test(eshot_screen_recorder_pause_api_tests screen_recorder_pause_api
        SOURCES
            tests/ScreenRecorderPauseApiTests.cpp
        LIBS Qt6::Core Qt6::Gui
    )

    eshot_add_test(eshot_notification_folder_opener_tests notification_folder_opener
        SOURCES
            tests/NotificationFolderOpenerTests.cpp
            src/core/NotificationFolderOpener.cpp
            src/core/NotificationFolderOpener.h
        LIBS Qt6::Core
    )

    if(UNIX AND NOT APPLE)
        eshot_add_test(eshot_linux_desktop_notification_tests linux_desktop_notification
            SOURCES
                tests/LinuxDesktopNotificationTests.cpp
                src/core/LinuxDesktopNotification.cpp
                src/core/LinuxDesktopNotification.h
            LIBS Qt6::Core Qt6::DBus
        )
    endif()
endif()

if(UNIX AND NOT APPLE)
    install(PROGRAMS scripts/linux/eshot-launcher
        DESTINATION bin
    )
    install(CODE "execute_process(COMMAND \${CMAKE_COMMAND} -E create_symlink eshot-launcher \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/bin/eshot\")")
    install(FILES packaging/linux/io.github.benoks.EShot.desktop
        DESTINATION share/applications
    )
    install(FILES packaging/linux/io.github.benoks.EShot.svg
        DESTINATION share/icons/hicolor/scalable/apps
    )
    install(FILES packaging/linux/io.github.benoks.EShot.svg
        DESTINATION share/icons/hicolor/scalable/apps
        RENAME io.github.benoks.EShot-v4.svg
    )
    install(FILES packaging/linux/io.github.benoks.EShot.svg
        DESTINATION share/pixmaps
    )
    install(FILES packaging/linux/io.github.benoks.EShot.svg
        DESTINATION share/pixmaps
        RENAME io.github.benoks.EShot-v4.svg
    )
    install(FILES packaging/linux/io.github.benoks.EShot.metainfo.xml
        DESTINATION share/metainfo
    )
    install(PROGRAMS
        scripts/linux/runtime-common.sh
        scripts/linux/install-runtime-deps.sh
        scripts/linux/packagekit-install.sh
        DESTINATION lib/eshot
    )
    # NOTE: the runtime helper scripts live under lib/eshot while the launcher
    # and the app go to bin/ (and package-linux.sh mirrors this layout for the
    # deb). Unifying this install layout is a packaging refactor that is
    # intentionally out of scope here.
endif()

# Bundle Tesseract OCR (third_party/tesseract/) next to the executable so the
# app can find tesseract.exe + tessdata at runtime without requiring a system
# install. Skip silently if the folder is missing.
if(WIN32 AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/third_party/tesseract/tesseract.exe)
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
