cmake_minimum_required(VERSION 3.25)

# Release builds pass the tag's version, for example -DDAC_VERSION=1.2.0.
set(DAC_VERSION "1.0.0" CACHE STRING "dev-asset-cleaner version (major.minor.patch)")
project(dev-asset-cleaner VERSION ${DAC_VERSION} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# The presets name Qt's folder below a root in aqtinstall's layout, for example
# 6.10.3/msvc2022_64. The root is the QT_ROOT environment variable, for a Qt folder that several
# projects share, or else .qt/ in the repository. scripts/setup.* download Qt into the same root.
if(DAC_QT_SUBDIR)
    if(NOT "$ENV{QT_ROOT}" STREQUAL "")
        file(TO_CMAKE_PATH "$ENV{QT_ROOT}" dac_qt_root)
    else()
        set(dac_qt_root "${CMAKE_SOURCE_DIR}/.qt")
    endif()
    list(PREPEND CMAKE_PREFIX_PATH "${dac_qt_root}/${DAC_QT_SUBDIR}")
endif()

find_package(Qt6 6.10 REQUIRED COMPONENTS Widgets Concurrent Network Test)
qt_standard_project_setup()

# Model, scanning and cleaning, shared by the app and the tests.
qt_add_library(dac_core STATIC
    src/model/CleanerType.h src/model/CleanerType.cpp
    src/model/Folder.h src/model/Folder.cpp
    src/model/Group.h
    src/model/Store.h src/model/Store.cpp
    src/clean/KnownCaches.h src/clean/KnownCaches.cpp
    src/clean/Monitor.h src/clean/Monitor.cpp
    src/clean/Scanner.h src/clean/Scanner.cpp
    src/clean/SystemFolders.h src/clean/SystemFolders.cpp
)
target_include_directories(dac_core PUBLIC src)
target_link_libraries(dac_core PUBLIC Qt6::Gui Qt6::Concurrent)
if(WIN32)
    # SystemFolders reads the user's security identifier for the Recycle Bin folders.
    target_link_libraries(dac_core PRIVATE advapi32)
endif()

# Widgets, shared by the app and the GUI test.
qt_add_library(dac_ui STATIC
    src/ui/AppInfo.h
    src/ui/DetectDialog.h src/ui/DetectDialog.cpp
    src/ui/CleanableListView.h src/ui/CleanableListView.cpp
    src/ui/ExcludeDialog.h src/ui/ExcludeDialog.cpp
    src/ui/FolderDelegate.h src/ui/FolderDelegate.cpp
    src/ui/FolderDialog.h src/ui/FolderDialog.cpp
    src/ui/FolderListView.h src/ui/FolderListView.cpp
    src/ui/FolderModel.h src/ui/FolderModel.cpp
    src/ui/GroupDelegate.h src/ui/GroupDelegate.cpp
    src/ui/GroupListView.h src/ui/GroupListView.cpp
    src/ui/GroupModel.h src/ui/GroupModel.cpp
    src/ui/Format.h
    src/ui/Icons.h src/ui/Icons.cpp
    src/ui/ItemCheckStyle.h src/ui/ItemCheckStyle.cpp
    src/ui/ItemsDialog.h src/ui/ItemsDialog.cpp
    src/ui/LoginItem.h
    src/ui/MainWindow.h src/ui/MainWindow.cpp
    src/ui/SettingsDialog.h src/ui/SettingsDialog.cpp
    src/ui/SizeSpinBox.h
    src/ui/Spinner.h src/ui/Spinner.cpp
    src/ui/SubfolderDialog.h src/ui/SubfolderDialog.cpp
    src/ui/SubfolderModel.h src/ui/SubfolderModel.cpp
    src/ui/SystemFoldersDialog.h src/ui/SystemFoldersDialog.cpp
    src/ui/TrayIcon.h
    src/ui/TypesDialog.h src/ui/TypesDialog.cpp
    src/ui/UsagePanel.h src/ui/UsagePanel.cpp
)
target_link_libraries(dac_ui PUBLIC dac_core Qt6::Widgets)
if(APPLE)
    enable_language(OBJCXX)
    target_sources(dac_ui PRIVATE
        src/ui/TrayIcon_mac.mm src/ui/LoginItem_mac.mm src/ui/MacApp.h src/ui/MacApp.mm)
    set_source_files_properties(src/ui/TrayIcon_mac.mm src/ui/LoginItem_mac.mm src/ui/MacApp.mm
        PROPERTIES COMPILE_OPTIONS -fobjc-arc)
    target_link_libraries(dac_ui PUBLIC "-framework AppKit" "-framework ServiceManagement")
else()
    target_sources(dac_ui PRIVATE src/ui/TrayIcon.cpp)
endif()
if(WIN32)
    target_sources(dac_ui PRIVATE src/ui/LoginItem_win.cpp)
elseif(NOT APPLE)
    target_sources(dac_ui PRIVATE src/ui/LoginItem_none.cpp)
endif()

qt_add_executable(dev-asset-cleaner WIN32 MACOSX_BUNDLE
    src/main.cpp
    src/platform/SingleInstance.h src/platform/SingleInstance.cpp
)

# The executable icon. Regenerate the files with the make_icons target after changing the
# drawing in src/ui/Icons.cpp.
if(WIN32)
    target_sources(dev-asset-cleaner PRIVATE resources/dev-asset-cleaner.rc)
elseif(APPLE)
    target_sources(dev-asset-cleaner PRIVATE resources/dev-asset-cleaner.icns)
    set_source_files_properties(resources/dev-asset-cleaner.icns PROPERTIES
        MACOSX_PACKAGE_LOCATION Resources)
    set_target_properties(dev-asset-cleaner PROPERTIES
        MACOSX_BUNDLE_ICON_FILE dev-asset-cleaner.icns)
endif()

target_compile_definitions(dev-asset-cleaner PRIVATE DAC_VERSION="${PROJECT_VERSION}")
target_link_libraries(dev-asset-cleaner PRIVATE dac_ui Qt6::Network)

# On macOS the bundle and its executable carry the app's name, as Finder shows it:
# "Dev Asset Cleaner.app". Elsewhere the executable keeps the target's name.
set(DAC_BUNDLE_NAME "Dev Asset Cleaner")
if(APPLE)
    set_target_properties(dev-asset-cleaner PROPERTIES OUTPUT_NAME "${DAC_BUNDLE_NAME}")
endif()

set_target_properties(dev-asset-cleaner PROPERTIES
    MACOSX_BUNDLE_BUNDLE_NAME "${DAC_BUNDLE_NAME}"
    MACOSX_BUNDLE_GUI_IDENTIFIER com.rangelreale.dev-asset-cleaner
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
)

qt_add_executable(make_icons EXCLUDE_FROM_ALL tools/make_icons.cpp)
target_link_libraries(make_icons PRIVATE dac_ui)

qt_add_executable(make_screenshot EXCLUDE_FROM_ALL tools/make_screenshot.cpp)
target_link_libraries(make_screenshot PRIVATE dac_ui)
if(APPLE)
    target_link_libraries(make_screenshot PRIVATE "-framework CoreGraphics")
elseif(WIN32)
    target_link_libraries(make_screenshot PRIVATE dwmapi)
endif()

# Copy the Qt runtime next to the executable so it runs from the build tree.
if(WIN32)
    add_custom_command(TARGET dev-asset-cleaner POST_BUILD
        COMMAND $<TARGET_FILE:Qt6::windeployqt> --no-translations --no-compiler-runtime
                --no-system-d3d-compiler --no-opengl-sw $<TARGET_FILE:dev-asset-cleaner>
        VERBATIM)
endif()

enable_testing()

foreach(test_name tst_store tst_scanner tst_mainwindow)
    qt_add_executable(${test_name} tests/${test_name}.cpp)
    target_link_libraries(${test_name} PRIVATE dac_ui Qt6::Test)
    # TAP: the plain-text logger prints nothing when stdout is a pipe on Windows.
    add_test(NAME ${test_name} COMMAND ${test_name} -o -,tap)
    if(WIN32)
        set_tests_properties(${test_name} PROPERTIES
            ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${QT6_INSTALL_PREFIX}/${QT6_INSTALL_BINS}")
    endif()
    set_property(TEST ${test_name} APPEND PROPERTY ENVIRONMENT
        "QT_QPA_PLATFORM=offscreen"
        "QT_PLUGIN_PATH=${QT6_INSTALL_PREFIX}/${QT6_INSTALL_PLUGINS}")
endforeach()

# `cmake --install` produces a self-contained app: windeployqt or macdeployqt copy the Qt
# libraries and plugins next to it.
include(GNUInstallDirs)
install(TARGETS dev-asset-cleaner
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES LICENSE DESTINATION .)
# windeployqt only: the runtime DLLs come from InstallRequiredSystemLibraries below, and Qt
# Widgets on Direct3D 11 needs neither the redistributable installer nor the shader compilers.
set(_deploy_options)
if(WIN32)
    set(_deploy_options --no-compiler-runtime --no-system-d3d-compiler
                        --no-system-dxc-compiler --no-opengl-sw)
endif()
qt_generate_deploy_app_script(
    TARGET dev-asset-cleaner
    OUTPUT_SCRIPT deploy_script
    NO_TRANSLATIONS
    NO_UNSUPPORTED_PLATFORM_ERROR
    DEPLOY_TOOL_OPTIONS ${_deploy_options})
install(SCRIPT ${deploy_script})

# macdeployqt edits the bundle's binaries, which leaves it unsigned, and macOS on Apple silicon
# does not start unsigned code. "-" signs ad hoc; a downloaded copy then needs Open Anyway in
# Privacy & Security on its first start. scripts/release-macos.sh passes a Developer ID instead,
# with the hardened runtime and secure timestamp that notarisation requires.
if(APPLE)
    set(DAC_CODESIGN_IDENTITY "-" CACHE STRING "codesign identity for the installed app")
    set(_codesign_options)
    if(NOT DAC_CODESIGN_IDENTITY STREQUAL "-")
        set(_codesign_options --options runtime --timestamp)
    endif()
    install(CODE "
        execute_process(
            COMMAND codesign --force --deep ${_codesign_options}
                    --sign \"${DAC_CODESIGN_IDENTITY}\"
                    \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${DAC_BUNDLE_NAME}.app\"
            COMMAND_ERROR_IS_FATAL ANY)
    ")
endif()

# The MSVC runtime DLLs, so the app runs on PCs without the Visual C++ Redistributable.
if(WIN32)
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION ${CMAKE_INSTALL_BINDIR})
    include(InstallRequiredSystemLibraries)
endif()

# Setup: `cpack -C Release` in build/<preset> builds dev-asset-cleaner-<version>-win64.exe with NSIS on
# Windows and dev-asset-cleaner-<version>-macos.dmg on macOS.
set(CPACK_PACKAGE_NAME dev-asset-cleaner)
set(CPACK_PACKAGE_VENDOR dev-asset-cleaner)
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Watches folder sizes and cleans build outputs and caches")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/rrgmc/dev-asset-cleaner")
set(CPACK_PACKAGE_INSTALL_DIRECTORY dev-asset-cleaner)
set(CPACK_PACKAGE_EXECUTABLES dev-asset-cleaner "Dev Asset Cleaner")
if(APPLE)
    set(CPACK_GENERATOR DragNDrop)
    set(CPACK_PACKAGE_FILE_NAME dev-asset-cleaner-${PROJECT_VERSION}-macos)
    set(CPACK_DMG_VOLUME_NAME "Dev Asset Cleaner")
else()
    set(CPACK_GENERATOR NSIS)
endif()
set(CPACK_NSIS_PACKAGE_NAME "Dev Asset Cleaner")
set(CPACK_NSIS_DISPLAY_NAME "Dev Asset Cleaner")
set(CPACK_NSIS_INSTALLED_ICON_NAME [=[bin\dev-asset-cleaner.exe]=])
set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
set(CPACK_NSIS_URL_INFO_ABOUT "${CPACK_PACKAGE_HOMEPAGE_URL}")
# Shown on the setup's license page.
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
file(TO_NATIVE_PATH "${CMAKE_SOURCE_DIR}/resources/dev-asset-cleaner.ico" _dac_ico)

# Icons, and two checkboxes on the last page: run dev-asset-cleaner now, and start it at sign-in. CPack
# has no hook for finish page options, so they go in with the icon defines, which come right
# before the pages. The sign-in value goes in the per-user Run key; --hidden starts dev-asset-cleaner in
# the tray.
#
# The sign-in checkbox starts ticked on a first install and keeps the installed version's
# choice on an upgrade. The old version's uninstaller removes the Run value before the new
# files go in, so DacReadStartAtSignIn reads it first, at the start of .onInit.
set(_nsis_code [=[
!define MUI_ICON "@ICO@"
!define MUI_UNICON "@ICO@"
!define MUI_FINISHPAGE_RUN ""
!define MUI_FINISHPAGE_RUN_TEXT "Run Dev Asset Cleaner"
!define MUI_FINISHPAGE_RUN_FUNCTION DacRun
!define MUI_FINISHPAGE_SHOWREADME ""
!define MUI_FINISHPAGE_SHOWREADME_TEXT "Start Dev Asset Cleaner when I sign in"
!define MUI_FINISHPAGE_SHOWREADME_FUNCTION DacAutostart
Var DacStartAtSignIn
Function DacReadStartAtSignIn
  Push $0
  StrCpy $DacStartAtSignIn 1
  ReadRegStr $0 HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\dev-asset-cleaner" "UninstallString"
  StrCmp $0 "" done
  ClearErrors
  ReadRegStr $0 HKCU "Software\Microsoft\Windows\CurrentVersion\Run" "dev-asset-cleaner"
  IfErrors 0 done
  StrCpy $DacStartAtSignIn 0
done:
  Pop $0
FunctionEnd
Function DacAutostart
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Run" "dev-asset-cleaner" '"$INSTDIR\bin\dev-asset-cleaner.exe" --hidden'
FunctionEnd
; The setup runs elevated; Explorer starts dev-asset-cleaner as the signed-in user instead.
Function DacRun
  Exec '"$WINDIR\explorer.exe" "$INSTDIR\bin\dev-asset-cleaner.exe"'
FunctionEnd
]=])
string(REPLACE "@ICO@" "${_dac_ico}" CPACK_NSIS_INSTALLER_MUI_ICON_CODE "${_nsis_code}")
# Close a running dev-asset-cleaner so its files can be replaced, and remove the Run value, which may
# point at an old folder. The finish page writes it again; a silent install has no finish
# page, so it writes the value here.
set(CPACK_NSIS_EXTRA_PREINSTALL_COMMANDS [=[
  nsExec::Exec 'taskkill /F /IM dev-asset-cleaner.exe'
  DeleteRegValue HKCU "Software\Microsoft\Windows\CurrentVersion\Run" "dev-asset-cleaner"
  ${If} ${Silent}
  ${AndIf} $DacStartAtSignIn == 1
    Call DacAutostart
  ${EndIf}
]=])
# CPack's NSIS template has no hook in .onInit or on the finish page, so CPack gets a copy
# that calls DacReadStartAtSignIn and unticks the sign-in checkbox when needed.
if(WIN32)
    file(READ "${CMAKE_ROOT}/Modules/Internal/CPack/NSIS.template.in" _nsis_template)
    set(_nsis_oninit "Function .onInit\n")
    set(_nsis_finish "!insertmacro MUI_PAGE_FINISH\n")
    foreach(_anchor IN ITEMS _nsis_oninit _nsis_finish)
        string(FIND "${_nsis_template}" "${${_anchor}}" _pos)
        if(_pos EQUAL -1)
            message(FATAL_ERROR "CPack's NSIS template has no \"${${_anchor}}\"; update CMakeLists.txt.")
        endif()
    endforeach()
    string(REPLACE "${_nsis_oninit}" "${_nsis_oninit}  Call DacReadStartAtSignIn\n"
        _nsis_template "${_nsis_template}")
    string(REPLACE "${_nsis_finish}" [=[!define MUI_PAGE_CUSTOMFUNCTION_SHOW DacFinishShow
  !insertmacro MUI_PAGE_FINISH
  Function DacFinishShow
    StrCmp $DacStartAtSignIn 1 +2
      SendMessage $mui.FinishPage.ShowReadme ${BM_SETCHECK} ${BST_UNCHECKED} 0
  FunctionEnd
]=] _nsis_template "${_nsis_template}")
    file(WRITE "${CMAKE_BINARY_DIR}/cpack/NSIS.template.in" "${_nsis_template}")
    list(PREPEND CMAKE_MODULE_PATH "${CMAKE_BINARY_DIR}/cpack")
endif()
set(CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS [=[
  nsExec::Exec 'taskkill /F /IM dev-asset-cleaner.exe'
  DeleteRegValue HKCU "Software\Microsoft\Windows\CurrentVersion\Run" "dev-asset-cleaner"
]=])
# Pass the NSIS code above through unchanged; without this CPack strips its quotes.
set(CPACK_VERBATIM_VARIABLES ON)
include(CPack)
