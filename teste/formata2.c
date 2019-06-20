#include "../include/t2fs.h"
#include <stdio.h>

int main() {
    
    int file1,file2;
    char amor[255] = "alencarS2matheusforever";
    char lido[255] = {'\0'};
    format2(4);
    file1 = create2("/alencarEMatheus");
    create2("/alencarEmatheus");

    write2(file1,amor,40);
    seek2(file1, 0);
    read2(file1,lido,40);
    printf("%s", lido);
    return 0;
}
