project(RAT_Test)

if(WIN32)
	
elseif(APPLE)

else()

endif()

if(MSVC)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4 /wd4100")
else()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -pedantic")
endif()

set(${PROJECT_NAME}_EXAMPLE_SRC 
	main.cpp
)

add_definitions(
	-DTEST_CERTIFICATE_PRIVATE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/../Test/private.key"
	-DTEST_CERTIFICATE_PUBLIC_PATH="${CMAKE_CURRENT_SOURCE_DIR}/../Test/public.crt"
	-DTEST_CERTIFICATE_PRIVATE_PASSWORD="Test pass"
)

include_directories(${REMOTE_ACCESS_INCLUDE_DIRS} uWebSockets/src)

add_executable(${PROJECT_NAME} ${${PROJECT_NAME}_EXAMPLE_SRC} ${${PROJECT_NAME}_PLATFORM_SRC})
target_link_libraries(${PROJECT_NAME} ${REMOTE_ACCESS_LIBS} ${CMAKE_THREAD_LIBS_INIT})

add_test (NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})