#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>


//__thread int key;
//__thread int obj = 32;
//__thread int arr[64];

int main(int argc, char** argv, char** envp) {
    write(1, "Hello World", 11);
    
#if 0   
    int i;
    for(i = 0; i < argc; i++)
        printf("%d: %s\n", i, argv[i]);
        
    for(i = 0; envp[i]; i++)
        printf("%d: %s\n", i, envp[i]);
    
    
    //key = 10;
    //printf("Hello World: %d %d\n", key, obj);
    
    
    //obj = 0xFF;
    //key = 10;
    
    //memset(arr, 0, 64 * sizeof(int));
    
    //printf("key: %d, obj: %d, arr[32] = %d\n", key, obj, arr[32]);
#endif

    for(;;);
}
