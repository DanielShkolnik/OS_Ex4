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

void blockSplit(MallocMetadataNode* block, size_t usedSize){
    size_t prevSize = block->size;
    block->size=usedSize + sizeof(MallocMetadataNode);
    block->is_free = false;
    MallocMetadataNode* newBlock = (MallocMetadataNode*)((unsigned long)((MallocMetadataNode*)block+1)+usedSize);
    newBlock->size = prevSize - block->size;
    newBlock->is_free = true;
    newBlock->prev = block;
    newBlock->next = block->next;
    if(newBlock->next != nullptr) newBlock->next->prev = newBlock;
    block->next = newBlock;

    mallocMetadataList.numOfUsedBlocks++;
    mallocMetadataList.sizeOfFreeBlocks-=block->size;
    mallocMetadataList.sizeOfUsedBlocks+=block->size;

    if(mallocMetadataList.tail == block) mallocMetadataList.tail = newBlock;
};



void* smalloc(size_t size){
    if(size<=0 || size>1e8) return NULL;
    MallocMetadataNode* current = mallocMetadataList.head;

    while (current != nullptr){
        if(current->is_free && current->size>(size+sizeof(MallocMetadataNode))){
            if(current->size>=size+2*sizeof(MallocMetadataNode)+128){
                blockSplit((MallocMetadataNode*)current,size);
                return (void*)((MallocMetadataNode*)current+1);
            }
            else{
                current->is_free = false;
                mallocMetadataList.numOfFreeBlocks--;
                mallocMetadataList.numOfUsedBlocks++;
                mallocMetadataList.sizeOfFreeBlocks-=current->size;
                mallocMetadataList.sizeOfUsedBlocks+=current->size;
                return (void*)((MallocMetadataNode*)current+1);
            }
        }
        current = current->next;
    }

    MallocMetadataNode* newBlock = (MallocMetadataNode*)sbrk((intptr_t)(size+sizeof(MallocMetadataNode)));
    if(newBlock == (void*)(-1)) return NULL;


    initMallocMetadataNode(newBlock,size+sizeof(MallocMetadataNode));

    mallocMetadataList.tail->next = newBlock;
    newBlock->prev=mallocMetadataList.tail;
    mallocMetadataList.tail = newBlock;

    mallocMetadataList.numOfUsedBlocks++;
    mallocMetadataList.sizeOfUsedBlocks+=size+sizeof(MallocMetadataNode);

    return (void*)((MallocMetadataNode*)newBlock+1);
}

void* scalloc(size_t num, size_t size){
    if(size<=0 || num*size>1e8 || num<=0) return NULL;

    void* newBlockPtr = smalloc(size);
    if(newBlockPtr == NULL) return NULL;

    memset(newBlockPtr,0,size*num);
    return newBlockPtr;
}


void blockCombine(MallocMetadataNode* block){
    MallocMetadataNode* prevBlock = block->prev;
    MallocMetadataNode* nextBlock = block->next;
    //left and right blocks are free
    if((prevBlock!= nullptr && prevBlock->is_free) && (nextBlock!= nullptr && nextBlock->is_free)){
        prevBlock->size += block->size + nextBlock->size;
        prevBlock->next = nextBlock->next;
        if(nextBlock->next!= nullptr) nextBlock->next->prev = prevBlock;
        mallocMetadataList.numOfFreeBlocks-=2;
        if(mallocMetadataList.tail == block) mallocMetadataList.tail = prevBlock;
    }
    //left block is free
    else if(prevBlock!= nullptr && prevBlock->is_free){
        prevBlock->size += block->size;
        prevBlock->next = block->next;
        if(block->next!= nullptr) block->next->prev = prevBlock;
        mallocMetadataList.numOfFreeBlocks--;
        if(mallocMetadataList.tail == block) mallocMetadataList.tail = prevBlock;
    }
    //right block is free
    else if(nextBlock!= nullptr && nextBlock->is_free){
        block->size += nextBlock->size;
        block->next = nextBlock->next;
        if(nextBlock->next!= nullptr) nextBlock->next->prev = block;
        mallocMetadataList.numOfFreeBlocks--;
    }

}

void sfree(void* p){
    if(p == NULL) return;
    MallocMetadataNode* current = mallocMetadataList.head;

    while (current != nullptr && (void*)((MallocMetadataNode*)current+1)<=p){
        if((void*)((MallocMetadataNode*)current+1)==p){
            if(current->is_free) return;
            current->is_free = true;
            mallocMetadataList.numOfFreeBlocks++;
            mallocMetadataList.numOfUsedBlocks--;
            mallocMetadataList.sizeOfFreeBlocks+=current->size;
            mallocMetadataList.sizeOfUsedBlocks-=current->size;
            if((current->prev!= nullptr && current->prev->is_free) || (current->next!= nullptr && current->next->is_free)) blockCombine(current);
            return;
        }
        current = current->next;
    }
}

void* srealloc(void* oldp, size_t size){
    if(size<=0 || size>1e8) return NULL;
    if(oldp == NULL) return smalloc(size);

    MallocMetadataNode* current = mallocMetadataList.head;

    while (current != nullptr  && (void*)((MallocMetadataNode*)current+1)<=oldp){
        if((void*)((MallocMetadataNode*)current+1)==oldp){
            if(size<current->size) return (void*)((MallocMetadataNode*)current+1);

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
