cmake_minimum_required(VERSION 3.7)

project(spark_dpu)
set(CMAKE_BUILD_TYPE "Debug")
set(CMAKE_C_FLAGS_DEBUG "$ENV{CXXFLAGS} -g2 -ggdb -Wno-deprecated-declarations")
set(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -g2 -ggdb -Wno-deprecated-declarations")
set(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -Wall -O3")

# export JAVA_HOME=/usr/lib/jvm/java-8-openjdk-amd64/
set(ENV{JAVA_HOME} "/usr/lib/jvm/java-8-openjdk-amd64/")
set(JAVA_AWT_LIBRARY "$ENV{JAVA_HOME}/lib/libjawt.so")
set(JAVA_JVM_LIBRARY "$ENV{JAVA_HOME}/lib/server/libjvm.so")
set(JAVA_INCLUDE_PATH "$ENV{JAVA_HOME}/include")
set(JAVA_INCLUDE_PATH2 "$ENV{JAVA_HOME}/include/linux")
set(JAVA_AWT_INCLUDE_PATH "$ENV{JAVA_HOME}/include")
find_package(JNI 1.8.0 EXACT REQUIRED)

# message(${JNI_INCLUDE_DIRS})
include_directories(${JNI_INCLUDE_DIRS})

### load module PktConfig
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBDOCA doca)

include_directories(.)
aux_source_directory(. SRC)
add_library(spark_dpu SHARED ${SRC})

message("LIBDOCA_CFLAGS" ${LIBDOCA_CFLAGS})
message("LIBDOCA_LDLAGS" ${LIBDOCA_LDFLAGS})
target_compile_options(spark_dpu PRIVATE ${LIBDOCA_CFLAGS})
target_link_libraries(spark_dpu ${LIBDOCA_LDFLAGS})

