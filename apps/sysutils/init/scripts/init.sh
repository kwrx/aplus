#/bin/sh

#aplus-terminal -c "cat /etc/motd && bash"
#aplus-ui

cd /tmp

echo "#include <stdio.h>" > main.c
echo "int main() {" >> main.c
echo "printf(\"Hello World!\");" >> main.c
echo "}" >> main.c

/usr/bin/gcc main.c -o main -O3

ls -lah

