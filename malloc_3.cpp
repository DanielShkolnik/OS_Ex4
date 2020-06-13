//
// Created by danie on 13/06/2020.
//

#include <stdio.h>
#include <unistd.h>
#include <string.h>


class MallocMetadataNode {
public:
    size_t size;
    bool is_free;
    MallocMetadataNode* next;
    MallocMetadataNode* prev;

    MallocMetadataNode(size_t size):size(size),is_free(false),next(nullptr),prev(nullptr){};

    MallocMetadataNode(const MallocMetadataNode &mallocMetadataNode) = default;
    MallocMetadataNode &operator=(const MallocMetadataNode &mallocMetadataNode) = default;

    ~MallocMetadataNode() = default;
};

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
        if(current->is_free && current->size>(size+sizeof(MallocMetadataNode))){
            current->is_free = false;
            mallocMetadataList.numOfFreeBlocks--;
            mallocMetadataList.numOfUsedBlocks++;
            mallocMetadataList.sizeOfFreeBlocks-=current->size;
            mallocMetadataList.sizeOfUsedBlocks+=current->size;
            return current+sizeof(MallocMetadataNode);
        }
        current = current->next;
    }

    MallocMetadataNode* newBlock =  sbrk(size+sizeof(MallocMetadataNode));
    if(*((int*)newBlock) == -1) return NULL;



    *newBlock = MallocMetadataNode(size+sizeof(MallocMetadataNode));

    mallocMetadataList.tail->next = newBlock;
    newBlock->prev=mallocMetadataList.tail;
    mallocMetadataList.tail = newBlock;

    mallocMetadataList.numOfUsedBlocks++;
    mallocMetadataList.sizeOfUsedBlocks+=size+sizeof(MallocMetadataNode);

    return newBlock+sizeof(MallocMetadataNode);
}

void* scalloc(size_t num, size_t size){
    if(size<=0 || num*size>1e8 || num<=0) return NULL;
    MallocMetadataNode* current = mallocMetadataList.head;

    while (current != nullptr){
        if(current->is_free && current->size>size*num){
            current->is_free = false;
            mallocMetadataList.numOfFreeBlocks--;
            mallocMetadataList.numOfUsedBlocks++;
            mallocMetadataList.sizeOfFreeBlocks-=current->size;
            mallocMetadataList.sizeOfUsedBlocks+=current->size;
            return memset(current+sizeof(MallocMetadataNode),0,num*size);
        }
        current = current->next;
    }

    MallocMetadataNode* newBlock =  sbrk(size+sizeof(MallocMetadataNode));
    if(*((int*)newBlock) == -1) return NULL;

    *newBlock = MallocMetadataNode(size+sizeof(MallocMetadataNode));

    mallocMetadataList.tail->next = newBlock;
    newBlock->prev=mallocMetadataList.tail;
    mallocMetadataList.tail = newBlock;

    mallocMetadataList.numOfUsedBlocks++;
    mallocMetadataList.sizeOfUsedBlocks+=size+sizeof(MallocMetadataNode);

    return memset(newBlock+sizeof(MallocMetadataNode),0,num*size);
}

void sfree(void* p){
    if(p == NULL) return;
    MallocMetadataNode* current = mallocMetadataList.head;

    while (current != nullptr && current+sizeof(MallocMetadataNode)<=p){
        if(current+sizeof(MallocMetadataNode)==p){
            if(current->is_free) return;
            current->is_free = true;
            mallocMetadataList.numOfFreeBlocks++;
            mallocMetadataList.numOfUsedBlocks--;
            mallocMetadataList.sizeOfFreeBlocks+=current->size;
            mallocMetadataList.sizeOfUsedBlocks-=current->size;
            return;
        }
        current = current->next;
    }
}

void* srealloc(void* oldp, size_t size){
    if(size<=0 || size>1e8) return NULL;
    if(oldp == NULL) return smalloc(size);

    MallocMetadataNode* current = mallocMetadataList.head;

    while (current != nullptr  && current+sizeof(MallocMetadataNode)<=oldp){
        if(current+sizeof(MallocMetadataNode)==oldp){
            if(size<current->size) return current+sizeof(MallocMetadataNode);

            void* newBlockPtr = smalloc(size);
            if(newBlockPtr == NULL) return NULL;
            memcpy(newBlockPtr,oldp,size);

            current->is_free = true;
            mallocMetadataList.numOfFreeBlocks++;
            mallocMetadataList.numOfUsedBlocks--;
            mallocMetadataList.sizeOfFreeBlocks+=current->size;
            mallocMetadataList.sizeOfUsedBlocks-=current->size;
            return newBlockPtr;
        }
        current = current->next;
    }
    return NULL;
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
