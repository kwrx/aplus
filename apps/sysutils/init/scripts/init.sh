#/bin/sh

#aplus-terminal -c "cat /etc/motd && bash"
#aplus-ui

cd /tmp

echo "#include <iostream>" > main.cpp
echo "int main() {" >> main.cpp
echo "std::cout << \"Hello, World!\" << std::endl;" >> main.cpp
echo "}" >> main.cpp

/usr/bin/g++ main.cpp -o main -O3

./main

# iobench /usr/libexec/gcc/x86_64-aplus/12.2.0/cc1plus

# iobench /dev/sda
# iobench /dev/sda1
# iobench /dev/sda2

