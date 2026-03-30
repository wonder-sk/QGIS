include(FetchContent)
FetchContent_Declare(SARibbon
  GIT_REPOSITORY https://github.com/czyt1988/SARibbon.git
  GIT_TAG v2.1.0
  GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(SARibbon)

set(QGIS_RIBBON_SRCS
  main.cpp
  qgsribbonmainwindow.cpp
  qgsidentifypanel.cpp
)

if (WIN32)
  win32_icon(QGIS_RIBBON_SRCS)
  win32_version_info("QGIS Ribbon App" "qgis_ribbon" QGIS_RIBBON_SRCS)
endif()

add_executable(qgis_ribbon WIN32 ${QGIS_RIBBON_SRCS})
target_include_directories(qgis_ribbon PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
)
target_link_libraries(qgis_ribbon
  qgis_core
  qgis_gui
  ${QT_VERSION_BASE}::Core
  ${QT_VERSION_BASE}::Gui
  ${QT_VERSION_BASE}::Widgets
  SARibbonBar
)

install(TARGETS qgis_ribbon
  BUNDLE DESTINATION ${QGIS_BIN_DIR}
  RUNTIME DESTINATION ${QGIS_BIN_DIR}
)
