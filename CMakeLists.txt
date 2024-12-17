cmake_minimum_required(VERSION 3.5)

# Set extension details
set(TARGET_NAME httpserver)
set(EXTENSION_NAME ${TARGET_NAME}_extension)
set(LOADABLE_EXTENSION_NAME ${TARGET_NAME}_loadable_extension)

project(${TARGET_NAME})
include_directories(
  src/include
  ${CMAKE_CURRENT_BINARY_DIR}
  duckdb/third_party/httplib
  duckdb/parquet/include
)

# Embed ./src/assets/index.html as a C++ header
add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/playground.hpp
  COMMAND ${CMAKE_COMMAND} -P ${PROJECT_SOURCE_DIR}/embed.cmake ${PROJECT_SOURCE_DIR}/src/assets/index.html ${CMAKE_CURRENT_BINARY_DIR}/httpserver_extension/http_handler/playground.hpp playgroundContent
  DEPENDS ${PROJECT_SOURCE_DIR}/src/assets/index.html
)

set(EXTENSION_SOURCES
    src/httpserver_extension.cpp
    src/http_handler/authentication.cpp
    src/http_handler/bindings.cpp
    src/http_handler/handler.cpp
    src/http_handler/response_serializer.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/playground.hpp
)

if(MINGW)
  set(OPENSSL_USE_STATIC_LIBS TRUE)
endif()

# Find OpenSSL before building extensions
find_package(OpenSSL REQUIRED)

build_static_extension(${TARGET_NAME} ${EXTENSION_SOURCES})
build_loadable_extension(${TARGET_NAME} " " ${EXTENSION_SOURCES})

include_directories(${OPENSSL_INCLUDE_DIR})
target_link_libraries(${LOADABLE_EXTENSION_NAME} duckdb_mbedtls ${OPENSSL_LIBRARIES})
set_property(TARGET ${LOADABLE_EXTENSION_NAME} PROPERTY CXX_STANDARD 17)
target_link_libraries(${EXTENSION_NAME} duckdb_mbedtls ${OPENSSL_LIBRARIES})
set_property(TARGET ${EXTENSION_NAME} PROPERTY CXX_STANDARD 17)

if(MINGW)
  set(WIN_LIBS crypt32 ws2_32 wsock32)
  find_package(ZLIB)
  target_link_libraries(${LOADABLE_EXTENSION_NAME} ZLIB::ZLIB ${WIN_LIBS})
  target_link_libraries(${EXTENSION_NAME} ZLIB::ZLIB ${WIN_LIBS})
endif()

install(
  TARGETS ${EXTENSION_NAME}
  EXPORT "${DUCKDB_EXPORT_SET}"
  LIBRARY DESTINATION "${INSTALL_LIB_DIR}"
  ARCHIVE DESTINATION "${INSTALL_LIB_DIR}")
