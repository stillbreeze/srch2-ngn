# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 2.8

#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:

# Remove some rules from gmake that .SUFFIXES does not remove.
SUFFIXES =

.SUFFIXES: .hpux_make_needs_suffix_list

# Produce verbose output by default.
VERBOSE = 1

# Suppress display of executed commands.
$(VERBOSE).SILENT:

# A target that is always out of date.
cmake_force:
.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /usr/local/bin/cmake

# The command to remove a file.
RM = /usr/local/bin/cmake -E remove -f

# Escaping for special characters.
EQUALS = =

# The program to use to edit the cache.
CMAKE_EDIT_COMMAND = /usr/bin/cmake-gui

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/jianfeng/workspace/srch2-ngn-jianfeng

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/jianfeng/workspace/srch2-ngn-jianfeng/debug

# Include any dependencies generated for this target.
include test/core/lib4Android/CMakeFiles/srch2scaleTest.dir/depend.make

# Include the progress variables for this target.
include test/core/lib4Android/CMakeFiles/srch2scaleTest.dir/progress.make

# Include the compile flags for this target's objects.
include test/core/lib4Android/CMakeFiles/srch2scaleTest.dir/flags.make

test/core/lib4Android/CMakeFiles/srch2scaleTest.dir/Scalability_Test.cpp.o: test/core/lib4Android/CMakeFiles/srch2scaleTest.dir/flags.make
test/core/lib4Android/CMakeFiles/srch2scaleTest.dir/Scalability_Test.cpp.o: ../test/core/lib4Android/Scalability_Test.cpp
	$(CMAKE_COMMAND) -E cmake_progress_report /home/jianfeng/workspace/srch2-ngn-jianfeng/debug/CMakeFiles $(CMAKE_PROGRESS_1)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building CXX object test/core/lib4Android/CMakeFiles/srch2scaleTest.dir/Scalability_Test.cpp.o"
	cd /home/jianfeng/workspace/srch2-ngn-jianfeng/debug/test/core/lib4Android && /home/jianfeng/software/android-toolchain/bin/arm-linux-androideabi-g++   $(CXX_DEFINES) $(CXX_FLAGS) -fPIC -o CMakeFiles/srch2scaleTest.dir/Scalability_Test.cpp.o -c /home/jianfeng/workspace/srch2-ngn-jianfeng/test/core/lib4Android/Scalability_Test.cpp

test/core/lib4Android/CMakeFiles/srch2scaleTest.dir/Scalability_Test.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/srch2scaleTest.dir/Scalability_Test.cpp.i"
	cd /home/jianfeng/workspace/srch2-ngn-jianfeng/debug/test/core/lib4Android && /home/jianfeng/software/android-toolchain/bin/arm-linux-androideabi-g++  $(CXX_DEFINES) $(CXX_FLAGS) -fPIC -E /home/jianfeng/workspace/srch2-ngn-jianfeng/test/core/lib4Android/Scalability_Test.cpp > CMakeFiles/srch2scaleTest.dir/Scalability_Test.cpp.i

test/core/lib4Android/CMakeFiles/srch2scaleTest.dir/Scalability_Test.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/srch2scaleTest.dir/Scalability_Test.cpp.s"
	cd /home/jianfeng/workspace/srch2-ngn-jianfeng/debug/test/core/lib4Android && /home/jianfeng/software/android-toolchain/bin/arm-linux-androideabi-g++  $(CXX_DEFINES) $(CXX_FLAGS) -fPIC -S /home/jianfeng/workspace/srch2-ngn-jianfeng/test/core/lib4Android/Scalability_Test.cpp -o CMakeFiles/srch2scaleTest.dir/Scalability_Test.cpp.s

test/core/lib4Android/CMakeFiles/srch2scaleTest.dir/Scalability_Test.cpp.o.requires:
.PHONY : test/core/lib4Android/CMakeFiles/srch2scaleTest.dir/Scalability_Test.cpp.o.requires

test/core/lib4Android/CMakeFiles/srch2scaleTest.dir/Scalability_Test.cpp.o.provides: test/core/lib4Android/CMakeFiles/srch2scaleTest.dir/Scalability_Test.cpp.o.requires
	$(MAKE) -f test/core/lib4Android/CMakeFiles/srch2scaleTest.dir/build.make test/core/lib4Android/CMakeFiles/srch2scaleTest.dir/Scalability_Test.cpp.o.provides.build
.PHONY : test/core/lib4Android/CMakeFiles/srch2scaleTest.dir/Scalability_Test.cpp.o.provides

test/core/lib4Android/CMakeFiles/srch2scaleTest.dir/Scalability_Test.cpp.o.provides.build: test/core/lib4Android/CMakeFiles/srch2scaleTest.dir/Scalability_Test.cpp.o

# Object files for target srch2scaleTest
srch2scaleTest_OBJECTS = \
"CMakeFiles/srch2scaleTest.dir/Scalability_Test.cpp.o"

# External object files for target srch2scaleTest
srch2scaleTest_EXTERNAL_OBJECTS =

../libs/armeabi-v7a/libsrch2scaleTest.so: test/core/lib4Android/CMakeFiles/srch2scaleTest.dir/Scalability_Test.cpp.o
../libs/armeabi-v7a/libsrch2scaleTest.so: test/core/lib4Android/CMakeFiles/srch2scaleTest.dir/build.make
../libs/armeabi-v7a/libsrch2scaleTest.so: ../libs/armeabi-v7a/libsrch2_core.so
../libs/armeabi-v7a/libsrch2scaleTest.so: /home/jianfeng/software/android-toolchain/user/lib/libboost_serialization.a
../libs/armeabi-v7a/libsrch2scaleTest.so: /home/jianfeng/software/android-toolchain/user/lib/libboost_program_options.a
../libs/armeabi-v7a/libsrch2scaleTest.so: /home/jianfeng/software/android-toolchain/user/lib/libboost_regex.a
../libs/armeabi-v7a/libsrch2scaleTest.so: /home/jianfeng/software/android-toolchain/user/lib/libboost_system.a
../libs/armeabi-v7a/libsrch2scaleTest.so: /home/jianfeng/software/android-toolchain/user/lib/libboost_thread.a
../libs/armeabi-v7a/libsrch2scaleTest.so: /home/jianfeng/software/android-toolchain/sysroot/usr/lib/libdl.so
../libs/armeabi-v7a/libsrch2scaleTest.so: /home/jianfeng/software/android-toolchain/sysroot/usr/lib/libz.so
../libs/armeabi-v7a/libsrch2scaleTest.so: /home/jianfeng/software/android-toolchain/sysroot/usr/lib/liblog.so
../libs/armeabi-v7a/libsrch2scaleTest.so: test/core/lib4Android/CMakeFiles/srch2scaleTest.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --red --bold "Linking CXX shared library ../../../../libs/armeabi-v7a/libsrch2scaleTest.so"
	cd /home/jianfeng/workspace/srch2-ngn-jianfeng/debug/test/core/lib4Android && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/srch2scaleTest.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
test/core/lib4Android/CMakeFiles/srch2scaleTest.dir/build: ../libs/armeabi-v7a/libsrch2scaleTest.so
.PHONY : test/core/lib4Android/CMakeFiles/srch2scaleTest.dir/build

test/core/lib4Android/CMakeFiles/srch2scaleTest.dir/requires: test/core/lib4Android/CMakeFiles/srch2scaleTest.dir/Scalability_Test.cpp.o.requires
.PHONY : test/core/lib4Android/CMakeFiles/srch2scaleTest.dir/requires

test/core/lib4Android/CMakeFiles/srch2scaleTest.dir/clean:
	cd /home/jianfeng/workspace/srch2-ngn-jianfeng/debug/test/core/lib4Android && $(CMAKE_COMMAND) -P CMakeFiles/srch2scaleTest.dir/cmake_clean.cmake
.PHONY : test/core/lib4Android/CMakeFiles/srch2scaleTest.dir/clean

test/core/lib4Android/CMakeFiles/srch2scaleTest.dir/depend:
	cd /home/jianfeng/workspace/srch2-ngn-jianfeng/debug && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/jianfeng/workspace/srch2-ngn-jianfeng /home/jianfeng/workspace/srch2-ngn-jianfeng/test/core/lib4Android /home/jianfeng/workspace/srch2-ngn-jianfeng/debug /home/jianfeng/workspace/srch2-ngn-jianfeng/debug/test/core/lib4Android /home/jianfeng/workspace/srch2-ngn-jianfeng/debug/test/core/lib4Android/CMakeFiles/srch2scaleTest.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : test/core/lib4Android/CMakeFiles/srch2scaleTest.dir/depend

