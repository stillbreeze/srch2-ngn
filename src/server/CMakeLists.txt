EXEC_PROGRAM( ${CMAKE_CXX_COMPILER}
                      ARGS  -dumpversion
                  OUTPUT_VARIABLE gcc_compiler_version)

IF (ANDROID) 
   SET( jsoncpp_LIBRARY ${CMAKE_SOURCE_DIR}/thirdparty/json/jsoncpp-src/android/libs/armeabi-v7a/libjsoncpp.so)
ELSE()
   SET( jsoncpp_LIBRARY ${CMAKE_SOURCE_DIR}/thirdparty/json/jsoncpp-src/android/libs/armeabi-v7a/libjsoncpp.so)
ENDIF()

FILE(GLOB SRCH2_SERVER_SRC RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.cpp" "./util/*.cpp" "./util/xmlParser/*.cpp"
                    "../../thirdparty/snappy-1.0.4/*.cc")
LIST(REMOVE_ITEM SRCH2_SERVER_SRC "srch2SearchServer.cpp")

SET(SRCH2_SERVER_NAME srch2-search-server)

INCLUDE_DIRECTORIES(
    ${CMAKE_BINARY_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/wrapper/
    ${CMAKE_SOURCE_DIR}/src/core/
    ${CMAKE_SOURCE_DIR}/src/server/
    ${CMAKE_SOURCE_DIR}/src/adapter/
    ${Boost_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/thirdparty/event/include
    ${CMAKE_SOURCE_DIR}/thirdparty/event/android/include
    ${CMAKE_SOURCE_DIR}/thirdparty/json/jsoncpp-src/include
    ${CMAKE_SOURCE_DIR}/thirdparty/gperftools/include
    ${CMAKE_SOURCE_DIR}/libs/armeabi-v7a
)

add_library(SERVER_OBJECTS OBJECT ${SRCH2_SERVER_SRC})

set(GPERFTOOL_LIBS "")
IF(ENABLE_PROFILER)
    set(GPERFTOOL_LIBS ${CMAKE_SOURCE_DIR}/thirdparty/gperftools/lib/libprofiler.so ${CMAKE_SOURCE_DIR}/thirdparty/libunwind/lib/libunwind.so)
ENDIF()

# IF(ANDROID)
# 	ADD_EXECUTABLE(${SRCH2_SERVER_NAME} srch2SearchServer.cpp $<TARGET_OBJECTS:WRAPPER_OBJECTS> $<TARGET_OBJECTS:SERVER_OBJECTS> $<TARGET_OBJECTS:ADAPTER_OBJECTS>)
# 	TARGET_LINK_LIBRARIES(${SRCH2_SERVER_NAME} 
#                         ${Srch2InstantSearch_LIBRARIES} 
#                         ${jsoncpp_LIBRARY}      ${CMAKE_SOURCE_DIR}/thirdparty/event/android/lib/libevent.so ${CMAKE_SOURCE_DIR}/thirdparty/event/android/lib/libevent_pthreads.so
#                         ${CMAKE_REQUIRED_LIBRARIES} ${Boost_LIBRARIES} 
#                 )
# ELSE()

# #ADD_EXECUTABLE(${SRCH2_SERVER_NAME} srch2SearchServer.cpp $<TARGET_OBJECTS:WRAPPER_OBJECTS> $<TARGET_OBJECTS:SERVER_OBJECTS> $<TARGET_OBJECTS:ADAPTER_OBJECTS>)              
# SET (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-conversion -Wno-sign-compare -fPIC")
# ADD_LIBRARY(${SRCH2_SERVER_NAME} SHARED srch2SearchServer.cpp $<TARGET_OBJECTS:WRAPPER_OBJECTS> $<TARGET_OBJECTS:SERVER_OBJECTS> $<TARGET_OBJECTS:ADAPTER_OBJECTS>)
# TARGET_LINK_LIBRARIES(${SRCH2_SERVER_NAME}  
#                         ${Srch2InstantSearch_LIBRARIES} 
#                         ${jsoncpp_LIBRARY} ${crypto_LIBRARY}  ${CMAKE_SOURCE_DIR}/thirdparty/event/lib/libevent.so ${CMAKE_SOURCE_DIR}/thirdparty/event/lib/libevent_pthreads.so
#                         ${Boost_LIBRARIES} ${GPERFTOOL_LIBS} ${CMAKE_REQUIRED_LIBRARIES}                      
#                 )
                
# ENDIF()

SET (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-conversion -Wno-sign-compare -fPIC")
ADD_LIBRARY(${SRCH2_SERVER_NAME} SHARED srch2SearchServer.cpp $<TARGET_OBJECTS:WRAPPER_OBJECTS> $<TARGET_OBJECTS:SERVER_OBJECTS> $<TARGET_OBJECTS:ADAPTER_OBJECTS>)
TARGET_LINK_LIBRARIES(${SRCH2_SERVER_NAME}  
                        ${Srch2InstantSearch_LIBRARIES} 
                        ${jsoncpp_LIBRARY} ${crypto_LIBRARY}  ${CMAKE_SOURCE_DIR}/thirdparty/event/android/lib/libevent.so ${CMAKE_SOURCE_DIR}/thirdparty/event/android/lib/libevent_pthreads.so
                        ${Boost_LIBRARIES} ${GPERFTOOL_LIBS} ${CMAKE_REQUIRED_LIBRARIES}                      
                )

#SET_TARGET_PROPERTIES(${SRCH2_SERVER_NAME} PROPERTIES COMPILE_FLAGS -fPIC)

ADD_DEPENDENCIES(${SRCH2_SERVER_NAME} srch2_core)
if (ANDROID)
INSTALL(FILES ${CMAKE_BINARY_DIR}/bin/${SRCH2_SERVER_NAME} DESTINATION bin RENAME "srch2-engine"  PERMISSIONS OWNER_READ OWNER_EXECUTE)
else()
INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/${SRCH2_SERVER_NAME} DESTINATION bin RENAME "srch2-engine"  PERMISSIONS OWNER_READ OWNER_EXECUTE)
endif()
#SET(EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_BINARY_DIR}/../search-server)
