project(vertexarrayfunc)
cmake_minimum_required(VERSION 3.5)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(OpenSceneGraph REQUIRED osgViewer)

include_directories(SYSTEM ${OPENSCENEGRAPH_INCLUDE_DIRS})
aux_source_directory(. SRC_LIST)
add_executable(${PROJECT_NAME} ${SRC_LIST})
target_link_libraries(${PROJECT_NAME} ${OPENSCENEGRAPH_LIBRARIES})
