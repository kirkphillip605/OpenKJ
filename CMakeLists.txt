cmake_minimum_required(VERSION 3.16)

file(READ "src/okjversion.h" vercontents)
string(REGEX MATCH "OKJ_VERSION_MAJOR ([0-9]*)" _ ${vercontents})
set(ver_major ${CMAKE_MATCH_1})
string(REGEX MATCH "OKJ_VERSION_MINOR ([0-9]*)" _ ${vercontents})
set(ver_minor ${CMAKE_MATCH_1})
string(REGEX MATCH "OKJ_VERSION_BUILD ([0-9]*)" _ ${vercontents})
set(ver_build ${CMAKE_MATCH_1})
set(sem_ver "${ver_major}.${ver_minor}.${ver_build}")

message(STATUS "Building OpenKJ ${sem_ver}")

project(openkj
        VERSION ${sem_ver}
        LANGUAGES CXX C
        )

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Disable APIs deprecated before Qt 5.15 to ease the Qt 6 bridge
add_definitions(-DQT_DISABLE_DEPRECATED_BEFORE=0x050F00)

# --- Qt Dependency ---
# Search Qt6 first, then fall back to Qt5 when both are installed.
find_package(QT NAMES Qt6 Qt5 COMPONENTS Widgets REQUIRED)
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Core Gui Sql Network Widgets Concurrent Svg PrintSupport REQUIRED)
if(QT_VERSION_MAJOR EQUAL 6)
    find_package(Qt6 COMPONENTS OpenGLWidgets QUIET)
    message(STATUS "Building with Qt 6 (${Qt6_VERSION})")
else()
    message(STATUS "Building with Qt 5 (${Qt5_VERSION})")
endif()

include_directories(src/)

# --- TagLib Dependency (Strictly External) ---
# We use pkg-config to locate the system TagLib. If not found, we fail immediately.
find_package(PkgConfig REQUIRED)
pkg_check_modules(TAGLIB IMPORTED_TARGET REQUIRED taglib)

message(STATUS "Using strictly external TagLib found at: ${TAGLIB_LIBRARY_DIRS}")
include_directories(SYSTEM ${TAGLIB_INCLUDE_DIRS})
link_directories(${TAGLIB_LIBRARY_DIRS})

# --- OS Specific Dependencies (GStreamer & Spdlog) ---

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux" OR ${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")
    pkg_check_modules(GSTREAMER REQUIRED gstreamer-1.0 gstreamer-app-1.0 gstreamer-audio-1.0 gstreamer-pbutils-1.0 gstreamer-controller-1.0 gstreamer-video-1.0)
    include_directories(${GSTREAMER_INCLUDE_DIRS} ${GLIB_LIBRARY_DIRS})
    link_directories(${GSTREAMER_LIBRARY_DIRS} ${GLIB_INCLUDE_DIRS})

    pkg_check_modules(SPDLOG IMPORTED_TARGET spdlog)
    if (SPDLOG_FOUND)
        message(STATUS "Using OS installed spdlog")
        add_definitions(-DSPDLOG_FMT_EXTERNAL)
        link_directories(${SPDLOG_LIBRARY_DIRS})
        include_directories(${SPDLOG_INCLUDE_DIRS})
        set(EXTERNAL_SPDLOG True)
    endif ()
endif ()

if (NOT EXTERNAL_SPDLOG)
    message(STATUS "No OS spdlog install found, pulling it in via CPM")
    include(cmake/CPM.cmake)
    CPMAddPackage("gh:gabime/spdlog@1.8.2")
endif ()

if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    message("macOS detected")
    include_directories(/Library/Frameworks/GStreamer.framework/Headers)
    link_directories(/Library/Frameworks)
    
    if (DEFINED BUNDLE_GSTREAMER)
        message("Will bundle GStreamer into the app bundle")
        add_compile_definitions(MAC_OVERRIDE_GST)
    endif ()
endif ()

if (${CMAKE_SYSTEM_NAME} MATCHES "Windows")
    set(WINDOWS_ICON_RC "src/appicon.rc")
    
    # Auto-detect architecture for GStreamer paths
    if(CMAKE_GENERATOR_PLATFORM MATCHES "ARM64" OR CMAKE_SYSTEM_PROCESSOR MATCHES "ARM64|arm64")
        set(GST_BASE_PATH "C:/Program Files/gstreamer/1.0/msvc_arm64")
        set(GST_BASE_PATH_NATIVE "C:\\Program Files\\gstreamer\\1.0\\msvc_arm64")
    elseif (CMAKE_SIZEOF_VOID_P EQUAL 4)
        set(GST_BASE_PATH "C:/gstreamer/1.0/x86")
        set(GST_BASE_PATH_NATIVE "C:\\gstreamer\\1.0\\x86")
    else ()
        set(GST_BASE_PATH "C:/gstreamer/1.0/x86_64")
        set(GST_BASE_PATH_NATIVE "C:\\gstreamer\\1.0\\x86_64")
    endif ()

    message("Using GST_BASE_PATH: ${GST_BASE_PATH}")
    include_directories(
            ${GST_BASE_PATH}/include/gstreamer-1.0
            ${GST_BASE_PATH}/include/glib-2.0
            ${GST_BASE_PATH}/lib/glib-2.0/include
            ${GST_BASE_PATH}/include/glib-2.0/gobject
    )
    link_directories("${GST_BASE_PATH}/lib")
    add_compile_definitions(MINIZ_NO_TIME)

    set(GSTREAMER_LIBRARIES
            gstreamer-1.0 glib-2.0 gobject-2.0 gstbase-1.0
            gstapp-1.0 gstaudio-1.0 gstpbutils-1.0
            gstcontroller-1.0 gstvideo-1.0 winmm
    )
endif ()

# --- Qlementine Style Engine & Icons (Qt 6 only) ---
if(QT_VERSION_MAJOR EQUAL 6)
    add_subdirectory(src/qlementine)
    add_subdirectory(src/qlementine-icons)
    add_definitions(-DUSE_QLEMENTINE)
endif()

# --- Project Source Files (Internal TagLib directory entirely purged) ---
set(SOURCE_FILES
        src/miniz/miniz.h
        src/miniz/miniz.c
        src/main.cpp
        src/dlgaddsong.cpp
        src/mediabackend.cpp
        src/mzarchive.cpp
        src/okjutil.h
        src/dlgvideopreview.cpp
        src/mainwindow.cpp
        src/dbupdatethread.cpp
        src/databasemanager.cpp
        src/directorymonitor.cpp
        src/shortcutmanager.cpp
        src/dlgkeychange.cpp
        src/dlgdatabase.cpp
        src/dlgrequests.cpp
        src/dlgregularexport.cpp
        src/dlgregularimport.cpp
        src/dlgregularsingers.cpp
        src/dlgsettings.cpp
        src/dlgcdg.cpp
        src/okarchive.cpp
        src/models/tablemodelbreaksongs.cpp
        src/models/tablemodelcustomnamingpatterns.cpp
        src/models/tablemodelhistorysingers.cpp
        src/models/tablemodelhistorysongs.cpp
        src/models/tablemodelkaraokesongs.cpp
        src/models/tablemodelkaraokesourcedirs.cpp
        src/models/tablemodelplaylistsongs.cpp
        src/models/tablemodelqueuesongs.cpp
        src/models/tablemodelrequests.cpp
        src/models/tablemodelrotation.cpp
        src/models/tablemodelsongshopsongs.cpp
        src/tagreader.cpp
        src/bmdbupdatethread.cpp
        src/settings.cpp
        src/bmdbdialog.cpp
        src/dlgcustompatterns.cpp
        src/audiorecorder.cpp
        src/okjsongbookapi.cpp
        src/dlgdbupdate.cpp
        src/dlgbookcreator.cpp
        src/dlgeq.cpp
        src/audiofader.cpp
        src/customlineedit.cpp
        src/tickernew.cpp
        src/updatechecker.cpp
        src/videodisplay.cpp
        src/volslider.cpp
        src/dlgaddsinger.cpp
        src/songshop.cpp
        src/dlgsongshop.cpp
        src/simplecrypt.cpp
        src/dlgsongshoppurchase.cpp
        src/dlgsetpassword.cpp
        src/dlgpassword.cpp
        src/dlgpurchaseprogress.cpp
        src/karaokefileinfo.cpp
        src/dlgeditsong.cpp
        src/soundfxbutton.cpp
        src/runguard/runguard.cpp
        src/durationlazyupdater.cpp
        src/idledetect.cpp
        src/mainwindow.h
        src/dlgaddsong.h
        src/dlgvideopreview.h
        src/mediabackend.h
        src/mzarchive.h
        src/okjutil.h
        src/dbupdatethread.h
        src/databasemanager.h
        src/directorymonitor.h
        src/shortcutmanager.h
        src/dlgkeychange.h
        src/dlgdatabase.h
        src/dlgrequests.h
        src/dlgregularexport.h
        src/dlgregularimport.h
        src/dlgregularsingers.h
        src/dlgsettings.h
        src/dlgcdg.h
        src/okarchive.h
        src/models/tablemodelbreaksongs.h
        src/models/tablemodelcustomnamingpatterns.h
        src/models/tablemodelhistorysingers.h
        src/models/tablemodelhistorysongs.h
        src/models/tablemodelkaraokesongs.h
        src/models/tablemodelkaraokesourcedirs.h
        src/models/tablemodelplaylistsongs.h
        src/models/tablemodelqueuesongs.h
        src/models/tablemodelrequests.h
        src/models/tablemodelrotation.h
        src/models/tablemodelsongshopsongs.h
        src/tagreader.h
        src/bmdbupdatethread.h
        src/settings.h
        src/bmdbdialog.h
        src/dlgcustompatterns.h
        src/audiorecorder.h
        src/okjsongbookapi.h
        src/dlgdbupdate.h
        src/dlgbookcreator.h
        src/dlgeq.h
        src/audiofader.h
        src/customlineedit.h
        src/tickernew.h
        src/updatechecker.h
        src/videodisplay.h
        src/volslider.h
        src/okjversion.h
        src/dlgaddsinger.h
        src/songshop.h
        src/dlgsongshop.h
        src/simplecrypt.h
        src/dlgsongshoppurchase.h
        src/dlgsetpassword.h
        src/dlgpassword.h
        src/dlgpurchaseprogress.h
        src/karaokefileinfo.h
        src/dlgeditsong.h
        src/soundfxbutton.h
        src/runguard/runguard.h
        src/models/tableviewtooltipfilter.h
        src/durationlazyupdater.h
        src/idledetect.h
        src/mainwindow.ui
        src/dlgaddsong.ui
        src/dlgkeychange.ui
        src/dlgdatabase.ui
        src/dlgrequests.ui
        src/dlgregularexport.ui
        src/dlgregularimport.ui
        src/dlgregularsingers.ui
        src/dlgsettings.ui
        src/dlgcdg.ui
        src/bmdbdialog.ui
        src/dlgcustompatterns.ui
        src/dlgdbupdate.ui
        src/dlgbookcreator.ui
        src/dlgeq.ui
        src/dlgaddsinger.ui
        src/dlgsongshop.ui
        src/dlgsongshoppurchase.ui
        src/dlgsetpassword.ui
        src/dlgpassword.ui
        src/dlgpurchaseprogress.ui
        src/dlgeditsong.ui
        src/dlgvideopreview.ui
        src/resources.qrc
        src/softwarerendervideosink.cpp
        src/softwarerendervideosink.h
        src/cdg/cdgappsrc.cpp
        src/cdg/cdgappsrc.h
        src/cdg/cdgfilereader.cpp
        src/cdg/cdgfilereader.h
        src/cdg/cdgimageframe.cpp
        src/cdg/cdgimageframe.h
        src/cdg/libCDG.h
        src/cdg/xbrz.cpp
        src/cdg/xbrz.h
        src/cdg/xbrz_config.h
        src/cdg/xbrz_tools.h
        src/gstreamer/gstreamerhelper.cpp
        src/gstreamer/gstreamerhelper.h
        src/dlgdebugoutput.cpp
        src/dlgdebugoutput.h
        src/dlgdebugoutput.ui
        src/theme.cpp
        src/theme.h
        )

# --- Libraries ---
set(LIBRARIES
        spdlog
        PkgConfig::TAGLIB
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::Sql
        Qt${QT_VERSION_MAJOR}::Network
        Qt${QT_VERSION_MAJOR}::Svg
        Qt${QT_VERSION_MAJOR}::PrintSupport
        Qt${QT_VERSION_MAJOR}::Concurrent
        )

if(QT_VERSION_MAJOR EQUAL 6)
    list(APPEND LIBRARIES qlementine qlementine-icons)
endif()

# --- Target Execution & Deployment ---

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux" OR ${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")
    if (EXTERNAL_SPDLOG)
        list(APPEND LIBRARIES "PkgConfig::SPDLOG")
    endif ()

    add_executable(openkj ${SOURCE_FILES})
    target_link_libraries(openkj ${LIBRARIES} ${GSTREAMER_LIBRARIES})

    install(TARGETS openkj DESTINATION bin)
    install(FILES src/Icons/org.openkj.OpenKJ.svg DESTINATION share/icons/hicolor/scalable/apps)
    install(FILES src/org.openkj.OpenKJ.desktop DESTINATION share/applications)
    install(FILES src/org.openkj.OpenKJ.metainfo.xml DESTINATION share/metainfo)
endif ()

if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set(openkj_ICON ${CMAKE_CURRENT_SOURCE_DIR}/src/Icons/OpenKJ.icns)
    set_source_files_properties(${openkj_ICON} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
    
    add_executable(openkj MACOSX_BUNDLE ${openkj_ICON} ${SOURCE_FILES})
    
    set_target_properties(openkj PROPERTIES
            BUNDLE True
            MACOSX_BUNDLE_GUI_IDENTIFIER org.openkj.openkj
            MACOSX_BUNDLE_BUNDLE_NAME OpenKJ
            MACOSX_BUNDLE_BUNDLE_VERSION ${sem_ver}
            MACOSX_BUNDLE_ICON_FILE OpenKJ.icns
            )
            
    target_link_libraries(openkj PRIVATE
            ${LIBRARIES}
            "-F /Library/Frameworks -framework GStreamer"
            )
            
    if (NOT DEFINED BUILDONLY)
        get_target_property(_qt_qmake_location Qt${QT_VERSION_MAJOR}::qmake IMPORTED_LOCATION)
        execute_process(
                COMMAND "${_qt_qmake_location}" -query QT_INSTALL_PREFIX
                RESULT_VARIABLE return_code
                OUTPUT_VARIABLE qt5_install_prefix
                OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        if (qt5_install_prefix)
            find_program(MACDEPLOYQT macdeployqt HINTS ${qt5_install_prefix} PATH_SUFFIXES bin)
            if (MACDEPLOYQT)
                if (DEFINED MAC_SIGNING_IDENTITY)
                    add_custom_command(
                            TARGET openkj
                            COMMAND "${MACDEPLOYQT}"
                            VERBATIM
                            ARGS
                            "${CMAKE_BINARY_DIR}/openkj.app"
                            -codesign=${MAC_SIGNING_IDENTITY}
                            "-appstore-compliant"
                    )
                else ()
                    add_custom_command(
                            TARGET openkj
                            COMMAND "${MACDEPLOYQT}"
                            ARGS "${CMAKE_BINARY_DIR}/openkj.app"
                    )
                endif ()
            else ()
                message("Can't find macdeployqt")
            endif ()
        else ()
            message("QT_INSTALL_DIR not defined")
        endif ()
        
        if (DEFINED BUNDLE_GSTREAMER)
            message("Installing GStreamer framework into app bundle")
            install(
                    DIRECTORY "/Library/Frameworks/GStreamer.framework.dist"
                    DESTINATION "openkj.app/Contents/Frameworks/GStreamer.framework"
            )
            add_custom_command(
                    TARGET openkj
                    POST_BUILD
                    COMMAND "cp" ARGS "-pR" "/Library/Frameworks/GStreamer.framework.dist" "openkj.app/Contents/Frameworks/GStreamer.framework"
                    COMMAND "osxrelocator" ARGS "openkj.app/Contents/Frameworks/GStreamer.framework/Versions/Current/lib" "/Library/Frameworks/GStreamer.framework/" "@executable_path/../Frameworks/GStreamer.framework/" "-r" USES_TERMINAL
                    COMMAND "osxrelocator" ARGS "openkj.app/Contents/Frameworks/GStreamer.framework/Versions/Current/libexec" "/Library/Frameworks/GStreamer.framework/" "@executable_path/../../../../../../Frameworks/GStreamer.framework/" "-r" USES_TERMINAL
                    COMMAND "osxrelocator" ARGS "openkj.app/Contents/Frameworks/GStreamer.framework/Versions/Current/bin" "/Library/Frameworks/GStreamer.framework/" "@executable_path/../../../../Frameworks/GStreamer.framework/" "-r" USES_TERMINAL
                    COMMAND "osxrelocator" ARGS "openkj.app/Contents/MacOS" "/Library/Frameworks/GStreamer.framework/" "@executable_path/../Frameworks/GStreamer.framework/" "-r" USES_TERMINAL
            )
        endif ()
    endif ()
endif ()

if (${CMAKE_SYSTEM_NAME} MATCHES "Windows")
    add_executable(openkj ${SOURCE_FILES} ${WINDOWS_ICON_RC})
    
    target_link_libraries(openkj PRIVATE ${LIBRARIES} ${GSTREAMER_LIBRARIES})
    
    set_property(TARGET openkj PROPERTY WIN32_EXECUTABLE true)

    if (DEFINED DEPLOY_DEPS)
        get_target_property(_qt_qmake_location Qt${QT_VERSION_MAJOR}::qmake IMPORTED_LOCATION)
        execute_process(
                COMMAND "${_qt_qmake_location}" -query QT_INSTALL_PREFIX
                RESULT_VARIABLE return_code
                OUTPUT_VARIABLE qt5_install_prefix
                OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        install(DIRECTORY "${GST_BASE_PATH}/bin/" DESTINATION "bin")
        if (qt5_install_prefix)
            find_program(WINDEPLOYQT windeployqt HINTS ${qt5_install_prefix} PATH_SUFFIXES bin)
            if (WINDEPLOYQT)
                add_custom_command(
                        TARGET openkj
                        COMMAND "${WINDEPLOYQT}"
                        ARGS "${CMAKE_BINARY_DIR}\\openkj.exe"
                )
            else ()
                message("Can't find windeployqt")
            endif ()
        else ()
            message("QT_INSTALL_DIR not defined")
        endif ()
        add_custom_command(
                TARGET openkj
                COMMAND "xcopy" ARGS "${GST_BASE_PATH_NATIVE}\\*" "/s" "/y"
                COMMAND "xcopy" ARGS "${GST_BASE_PATH_NATIVE}\\bin\\*.dll" "/y"
        )
    endif ()
endif ()
