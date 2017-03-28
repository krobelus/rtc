# automake-1.11
# autoconf-2.64
# gcc5
# git checkout cilkplus
# bash -c 'env PATH=/opt/autoconf/2.64/bin:$PATH LD_PRELOAD=/usr/lib/libstdc++.so.6 CXX=g++-5 ~/git/gcc/configure --prefix=$HOME/git/ratelim/rtc/cilkplus --enable-languages="c" && make && make install'
export PATH=./cilkplus/bin:$PATH
export CPATH=./cilkplus/include:$CPATH
export LIBRARY_PATH=./cilkplus/lib64:$LIBRARY_PATH
export LD_LIBRARY_PATH=./cilkplus/lib64:$LD_LIBRARY_PATH
