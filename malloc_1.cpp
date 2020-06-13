//
// Created by danie on 13/06/2020.
//

#include <stdio.h>
#include <unistd.h>

void* smalloc(size_t size){
    if(size<=0 || size>1e8) return NULL;
    void* previousBRK =  sbrk(size);
    if(*((int*)previousBRK) == -1) return NULL;
    return previousBRK;
}
