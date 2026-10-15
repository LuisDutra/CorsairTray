cmake_minimum_required(VERSION 3.20.0)

project(Tray VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# iCUE SDK: headers ficam em third_party/; a DLL e a .lib devem ser baixadas
# do SDK da Corsair e colocadas nessa mesma pasta (nao sao versionadas).
add_library(ICUESDK SHARED IMPORTED)
set_target_properties(ICUESDK PROPERTIES
    IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/third_party/iCUESDK.x64_2019.dll"
    IMPORTED_IMPLIB "${CMAKE_CURRENT_SOURCE_DIR}/third_party/iCUESDK.x64_2019.lib"
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/third_party")

add_subdirectory(src)

# ---------------------------------------------------------------------------
# Empacotamento (cpack): instalador NSIS + ZIP portatil
# ---------------------------------------------------------------------------

# A DLL do iCUE depende do runtime do VC++ (MSVCP140, VCRUNTIME140...):
# copia essas DLLs para junto do app, assim nao precisa instalar o vc_redist.
set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION bin)
include(InstallRequiredSystemLibraries)

set(CPACK_VERBATIM_VARIABLES ON) # "\\" no CMake vira "\" no script NSIS
set(CPACK_GENERATOR "NSIS;ZIP")
set(CPACK_PACKAGE_NAME "CorsairTray")
set(CPACK_PACKAGE_VENDOR "Luis Dutra")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Nivel de bateria do headset Corsair na bandeja do Windows")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "CorsairTray")
set(CPACK_PACKAGE_EXECUTABLES "CorsairTray" "CorsairTray") # atalho no menu Iniciar

set(CPACK_NSIS_DISPLAY_NAME "CorsairTray")
set(CPACK_NSIS_PACKAGE_NAME "CorsairTray")
set(CPACK_NSIS_INSTALLED_ICON_NAME "CorsairTray.exe")
set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
set(CPACK_NSIS_MUI_FINISHPAGE_RUN "CorsairTray.exe")

# Fecha o app se estiver aberto (senao os arquivos ficam travados) e
# registra/remove a inicializacao junto com o Windows para todos os usuarios.
set(CPACK_NSIS_EXTRA_PREINSTALL_COMMANDS
    "nsExec::Exec 'taskkill /F /IM CorsairTray.exe'")
set(CPACK_NSIS_EXTRA_INSTALL_COMMANDS
    "WriteRegStr HKLM 'Software\\Microsoft\\Windows\\CurrentVersion\\Run' 'CorsairTray' '\"$INSTDIR\\bin\\CorsairTray.exe\"'")
set(CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS
    "nsExec::Exec 'taskkill /F /IM CorsairTray.exe'
DeleteRegValue HKLM 'Software\\Microsoft\\Windows\\CurrentVersion\\Run' 'CorsairTray'")

include(CPack)
