# Installs the XenUI DLL, import library, headers and a CMake package, so other projects can use
# find_package(XenUI CONFIG REQUIRED) and link XenUI::XenUI.
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(XENUI_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/XenUI")

install(TARGETS XenUI
        EXPORT XenUITargets
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
install(FILES "$<TARGET_PDB_FILE:XenUI>" DESTINATION "${CMAKE_INSTALL_BINDIR}" OPTIONAL)

install(DIRECTORY "${PROJECT_SOURCE_DIR}/Source/XenUI/"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/XenUI"
        FILES_MATCHING PATTERN "*.hpp"
        PATTERN "Pch.hpp" EXCLUDE
)
install(FILES "${XENUI_GENERATED_DIR}/XenUI/Version.hpp" DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/XenUI")

install(EXPORT XenUITargets
        NAMESPACE XenUI::
        DESTINATION "${XENUI_INSTALL_CMAKEDIR}"
)

configure_package_config_file(
        "${PROJECT_SOURCE_DIR}/cmake/XenUIConfig.cmake.in"
        "${PROJECT_BINARY_DIR}/XenUIConfig.cmake"
        INSTALL_DESTINATION "${XENUI_INSTALL_CMAKEDIR}"
)
write_basic_package_version_file(
        "${PROJECT_BINARY_DIR}/XenUIConfigVersion.cmake"
        VERSION "${PROJECT_VERSION}"
        COMPATIBILITY SameMinorVersion
)
install(FILES
        "${PROJECT_BINARY_DIR}/XenUIConfig.cmake"
        "${PROJECT_BINARY_DIR}/XenUIConfigVersion.cmake"
        DESTINATION "${XENUI_INSTALL_CMAKEDIR}"
)
