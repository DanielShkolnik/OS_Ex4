//
// Created by danie on 13/06/2020.
//

#include <stdio.h>
#include <unistd.h>

class MallocMetadataNode {
public:
    void* allocatedBlockPtr;
    size_t size;
    bool is_free;
    MallocMetadataNode* next;
    MallocMetadataNode* prev;
};

class MallocMetadataNodeList {
public:
    size_t numOfFreeBlocks;
    size_t sizeOfFreeBlocks;
    size_t numOfUsedBlocks;
    size_t sizeOfUsedBlocks;
    size_t sizeOfSingleMetadataBlock;
    MallocMetadataNode* head;
    MallocMetadataNode* tail;

};


void* smalloc(size_t size){

}

void* scalloc(size_t num, size_t size){

}

void sfree(void* p){

}

void* srealloc(void* oldp, size_t size){

}

size_t _num_free_blocks(){

}

size_t _num_free_bytes(){

}

size_t _num_allocated_blocks(){

}

size_t _num_allocated_bytes(){

}

size_t _num_meta_data_bytes(){

}

size_t _size_meta_data(){

}
