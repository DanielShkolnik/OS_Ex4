//
// Created by danie on 13/06/2020.
//

#include <stdio.h>
#include <unistd.h>
#include <string.h>


typedef struct MallocMetadataNode_t{
public:
    size_t size; // actual allocated block size
    bool is_free;
    MallocMetadataNode_t* next;
    MallocMetadataNode_t* prev;
}MallocMetadataNode;


void initMallocMetadataNode(MallocMetadataNode* node, size_t size){
    node->size = size;
    node->is_free = false;
    node->next = nullptr;
    node->prev = nullptr;
}

class MallocMetadataList {
public:
    size_t numOfFreeBlocks;
    size_t sizeOfFreeBlocks;
    size_t numOfUsedBlocks;
    size_t sizeOfUsedBlocks;
    MallocMetadataNode* head;
    MallocMetadataNode* tail;

    MallocMetadataList():numOfFreeBlocks(0),sizeOfFreeBlocks(0),numOfUsedBlocks(0),sizeOfUsedBlocks(0),
                            head(nullptr),tail(nullptr){};

    MallocMetadataList(const MallocMetadataList &mallocMetadataList) = default;
    MallocMetadataList &operator=(const MallocMetadataList &mallocMetadataList) = default;

    ~MallocMetadataList() = default;

};

MallocMetadataList mallocMetadataList = MallocMetadataList();


void* smalloc(size_t size){
    if(size<=0 || size>1e8) return NULL;
    MallocMetadataNode* current = mallocMetadataList.head;

    while (current != nullptr){
        if(current->is_free && current->size>=(size+sizeof(MallocMetadataNode))){
            current->is_free = false;
            mallocMetadataList.numOfFreeBlocks--;
            mallocMetadataList.numOfUsedBlocks++;
            mallocMetadataList.sizeOfFreeBlocks-=current->size;
            mallocMetadataList.sizeOfUsedBlocks+=current->size;
            return (void*)((MallocMetadataNode*)current+1);
        }
        current = current->next;
    }

    MallocMetadataNode* newBlock = (MallocMetadataNode*)sbrk((intptr_t)(size+sizeof(MallocMetadataNode)));
    if(newBlock == (void*)(-1)) return NULL;


    initMallocMetadataNode(newBlock,size+sizeof(MallocMetadataNode));

    if(mallocMetadataList.head == nullptr && mallocMetadataList.tail== nullptr){
        mallocMetadataList.head = newBlock;
        mallocMetadataList.tail = newBlock;
    }
    else{
        mallocMetadataList.tail->next = newBlock;
        newBlock->prev=mallocMetadataList.tail;
        mallocMetadataList.tail = newBlock;
    }

    mallocMetadataList.numOfUsedBlocks++;
    mallocMetadataList.sizeOfUsedBlocks+=size+sizeof(MallocMetadataNode);

    return (void*)((MallocMetadataNode*)newBlock+1);
}

void* scalloc(size_t num, size_t size){
    if(size<=0 || num*size>1e8 || num<=0) return NULL;

    void* newBlockPtr = smalloc(size*num);
    if(newBlockPtr == NULL) return NULL;

    memset((void*)newBlockPtr,0,size*num);
    return (void*)newBlockPtr;
}

void sfree(void* p){
    if(p == NULL) return;
    MallocMetadataNode* current = (MallocMetadataNode*)p-1;
    if(current->is_free) return;
    current->is_free = true;
    mallocMetadataList.numOfFreeBlocks++;
    mallocMetadataList.numOfUsedBlocks--;
    mallocMetadataList.sizeOfFreeBlocks+=current->size;
    mallocMetadataList.sizeOfUsedBlocks-=current->size;
}

void* srealloc(void* oldp, size_t size){
    if(size<=0 || size>1e8) return NULL;
    if(oldp == NULL) return smalloc(size);
    MallocMetadataNode* current = (MallocMetadataNode*)oldp-1;
    if(size+sizeof(MallocMetadataNode)<=current->size) return (void*)((MallocMetadataNode*)current+1);
    void* newBlockPtr = smalloc(size);
    if(newBlockPtr == NULL) return NULL;
    memcpy(newBlockPtr,(void*)((MallocMetadataNode*)oldp+1),size);
    sfree(oldp);
    return (void*)newBlockPtr;
}

size_t _num_free_blocks(){
    return mallocMetadataList.numOfFreeBlocks;
}

size_t _num_free_bytes(){
    return mallocMetadataList.sizeOfFreeBlocks-mallocMetadataList.numOfFreeBlocks*sizeof(MallocMetadataNode);
}

size_t _num_allocated_blocks(){
    return  mallocMetadataList.numOfUsedBlocks + mallocMetadataList.numOfFreeBlocks;

}

size_t _num_allocated_bytes(){
    return mallocMetadataList.sizeOfUsedBlocks + mallocMetadataList.sizeOfFreeBlocks - (mallocMetadataList.numOfFreeBlocks+mallocMetadataList.numOfUsedBlocks)*sizeof(MallocMetadataNode);
}

size_t _num_meta_data_bytes(){
    return (mallocMetadataList.numOfFreeBlocks+mallocMetadataList.numOfUsedBlocks)*sizeof(MallocMetadataNode);
}

size_t _size_meta_data(){
    return sizeof(MallocMetadataNode);
}
