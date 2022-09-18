find_library(MHTD microhttpd REQUIRED)
find_package(CURL REQUIRED)
find_package(LibXml2 REQUIRED)
find_package(SQLite3 REQUIRED)

include_directories("${LIBXML2_INCLUDE_DIRS}")

add_executable("${PROJECT_NAME}" main.c server.c)

target_link_libraries(${PROJECT_NAME}
  ${MHTD}
  ${CURL_LIBRARIES}
  ${LIBXML2_LIBRARIES}
  ${SQLite3_LIBRARIES}
)
