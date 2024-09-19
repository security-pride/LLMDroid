cd "native"

rm -r  CMakeFiles/
rm -f CMakeCache.txt
# cmake -DCMAKE_TOOLCHAIN_FILE=$NDK_ROOT/build/cmake/android.toolchain.cmake -DANDROID_ABI=armeabi-v7a -DCMAKE_BUILD_TYPE=Release
# make
# rm -r  CMakeFiles/

cmake -DCMAKE_TOOLCHAIN_FILE=$NDK_ROOT/build/cmake/android.toolchain.cmake -DANDROID_ABI=arm64-v8a -DCMAKE_BUILD_TYPE=Release
make clean
make -j8
#rm -r  CMakeFiles/
#$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
#-DVCPKG_CHAINLOAD_TOOLCHAIN_FILE=$NDK_ROOT/build/cmake/android.toolchain.cmake

# cmake -DCMAKE_TOOLCHAIN_FILE=$NDK_ROOT/build/cmake/android.toolchain.cmake -DANDROID_ABI=x86  -DCMAKE_BUILD_TYPE=Release
# make
# rm -r  CMakeFiles/

# cmake -DCMAKE_TOOLCHAIN_FILE=$NDK_ROOT/build/cmake/android.toolchain.cmake -DANDROID_ABI=x86_64  -DCMAKE_BUILD_TYPE=Release
# make
# rm -r  CMakeFiles/
