#!/bin/sh

# cd /tmp

# echo "#include <iostream>" > main.cpp
# echo "int main() {" >> main.cpp
# echo "std::cout << \"Hello, World!\" << std::endl;" >> main.cpp
# echo "}" >> main.cpp

# /usr/bin/g++ main.cpp -o main -O3

# ./main

# The display server takes over /dev/fb0 and the input devices (/dev/kbd, /dev/mouse and
# /dev/tablet) and hands out windows over /tmp/aplus-wm.sock. It has to be running before
# any client, but the clients retry the connect for a few seconds, so there is no race to
# sleep around here.
aplus-wm &

# Now a client rather than the owner of the screen.
aplus-terminal -c "cat /etc/motd && while true; do /bin/dash; done"



# iobench -b64 /usr/libexec/gcc/x86_64-aplus/12.2.0/cc1plus
# iobench -b64 /usr/libexec/gcc/x86_64-aplus/12.2.0/cc1plus

# iobench /dev/sda
# # iobench /dev/sda1
# # iobench /dev/sda2

# /usr/lib/ld-musl-x86_64.so.1 --library-path /usr/lib /test

# gl-gears
# gl-shaders-triangle
# gl-shaders-scene

