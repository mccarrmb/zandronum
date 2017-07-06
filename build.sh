cd buildclient
cmake -DCMAKE_BUILD_TYPE=Release -DFMOD_LIBRARY=`pwd`/../fmodapi42416linux64/api/lib/libfmodex64-4.24.16.so -DFMOD_INCLUDE_DIR=`pwd`/../fmodapi42416linux64/api/inc .. 
make
