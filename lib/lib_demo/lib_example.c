#include <stdio.h>
#include <stdlib.h>

void lib_example1(void)
{
    printf("This is a lib_example\n");
    return ;

}

int lib_example2(int num)
{

    num=num+1;
    num++;
    return num;

}
