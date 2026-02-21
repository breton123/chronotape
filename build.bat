cmake -S . -B build ^
  -G "MinGW Makefiles" ^
  -DCMAKE_C_COMPILER=gcc ^
  -DCMAKE_CXX_COMPILER=g++ ^
  -DCMAKE_BUILD_TYPE=Release
cmake --build build 

