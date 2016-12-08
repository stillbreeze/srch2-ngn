
INCLUDE_DIRECTORIES(
	${CMAKE_BINARY_DIR}/include
	${CMAKE_SOURCE_DIR}/src/
	${CMAKE_SOURCE_DIR}/src/core/
	${CMAKE_SOURCE_DIR}/src/wrapper/
	${CMAKE_SOURCE_DIR}/src/adapter/
    ${Boost_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/thirdparty/event/include
    ${CMAKE_SOURCE_DIR}/thirdparty/json/jsoncpp-src/include
)

set(GPERFTOOL_LIBS "")
IF(ENABLE_PROFILER)
    set(GPERFTOOL_LIBS ${CMAKE_SOURCE_DIR}/thirdparty/gperftools/lib/libprofiler.a ${CMAKE_SOURCE_DIR}/thirdparty/libunwind/lib/libunwind.a)
ENDIF()

# EXEC_PROGRAM( ${CMAKE_CXX_COMPILER}
#                       ARGS  -dumpversion
#                   OUTPUT_VARIABLE gcc_compiler_version)

SET( jsoncpp_LIBRARY ${CMAKE_SOURCE_DIR}/thirdparty/json/jsoncpp-src/android/libs/armeabi-v7a/libjsoncpp.so)

# ADD_EXECUTABLE(PostProcessingFilter_Test PostProcessingFilters_Test.cpp)

SET (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-conversion -Wno-sign-compare -fPIC")
ADD_LIBRARY(PostProcessingFilter_Test SHARED PostProcessingFilters_Test.cpp)

TARGET_LINK_LIBRARIES(PostProcessingFilter_Test 
						${Srch2InstantSearch_LIBRARIES} 
						${jsoncpp_LIBRARY}	${CMAKE_SOURCE_DIR}/thirdparty/event/lib/libevent.so 
						${Boost_LIBRARIES} ${CMAKE_REQUIRED_LIBRARIES}  ${GPERFTOOL_LIBS}
						)
ADD_DEPENDENCIES(PostProcessingFilter_Test srch2_core)




# ADD_EXECUTABLE(XMLParser_Test XMLParser_Test.cpp $<TARGET_OBJECTS:SERVER_OBJECTS> $<TARGET_OBJECTS:WRAPPER_OBJECTS> $<TARGET_OBJECTS:ADAPTER_OBJECTS>)

SET (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-conversion -Wno-sign-compare -fPIC")
ADD_LIBRARY(XMLParser_Test SHARED XMLParser_Test.cpp $<TARGET_OBJECTS:SERVER_OBJECTS> $<TARGET_OBJECTS:WRAPPER_OBJECTS> $<TARGET_OBJECTS:ADAPTER_OBJECTS>)
TARGET_LINK_LIBRARIES(XMLParser_Test
                        ${Srch2InstantSearch_LIBRARIES} 
                        ${jsoncpp_LIBRARY}  ${CMAKE_SOURCE_DIR}/thirdparty/event/lib/libevent.so 
                        ${Boost_LIBRARIES} ${CMAKE_REQUIRED_LIBRARIES}  ${GPERFTOOL_LIBS} ${CMAKE_SOURCE_DIR}/thirdparty/json/jsoncpp-src/android/libs/armeabi-v7a/libjsoncpp.so
                    )
ADD_DEPENDENCIES(XMLParser_Test srch2_core)
