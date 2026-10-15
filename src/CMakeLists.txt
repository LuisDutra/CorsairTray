add_executable(${PROJECT_NAME} WIN32 main.cpp)
set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME CorsairTray)

target_link_libraries(${PROJECT_NAME} PRIVATE ICUESDK shell32 user32 gdi32)

# Copia a DLL do SDK para junto do executavel
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:ICUESDK> $<TARGET_FILE_DIR:${PROJECT_NAME}>)

# Arquivos que vao para o instalador. Precisa ser bin/: e onde o NSIS do CPack
# espera o executavel para o atalho do menu Iniciar e o "executar ao concluir".
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)
install(FILES $<TARGET_FILE:ICUESDK> DESTINATION bin)
