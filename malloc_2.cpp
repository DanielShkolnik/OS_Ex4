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

    MallocMetadataNode(void* allocatedBlockPtr, size_t size):allocatedBlockPtr(allocatedBlockPtr),size(size),
                        is_free(false),next(nullptr),prev(nullptr){};

    MallocMetadataNode(const MallocMetadataNode &mallocMetadataNode) = delete;
    MallocMetadataNode &operator=(const MallocMetadataNode &mallocMetadataNode) = delete;

    ~MallocMetadataNode() = default;
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

    MallocMetadataNodeList():numOfFreeBlocks(0),sizeOfFreeBlocks(0),numOfUsedBlocks(0),sizeOfUsedBlocks(0),
                            sizeOfSingleMetadataBlock(0),head(nullptr),tail(nullptr){};

    MallocMetadataNodeList(const MallocMetadataNodeList &mallocMetadataNodeList) = delete;
    MallocMetadataNodeList &operator=(const MallocMetadataNodeList &mallocMetadataNodeList) = delete;

    ~MallocMetadataNodeList(){
        MallocMetadataNode *current = this->head;
        MallocMetadataNode *prev = current;
        while (current != nullptr) {
            current = current->next;
            delete prev;
            prev = current;
        }
    }

};

// TODO: remembe to add "delete mallocMetadataNodeList;"
MallocMetadataNodeList* mallocMetadataNodeList= new MallocMetadataNodeList();


void* smalloc(size_t size){
    if(size<=0 || size>1e8) return NULL;
    MallocMetadataNode* current = mallocMetadataNodeList->head->next;

    while (current != nullptr){
        if(current->is_free && current->size>size){
            current->is_free = false;
            mallocMetadataNodeList->numOfFreeBlocks--;
            mallocMetadataNodeList->numOfUsedBlocks++;
            mallocMetadataNodeList->sizeOfFreeBlocks-=current->size;
            mallocMetadataNodeList->sizeOfUsedBlocks+=current->size;
            return current->allocatedBlockPtr;
        }
        current = current->next;
    }

    void* previousBRK =  sbrk(size);
    if(*((int*)previousBRK) == -1) return NULL;

    MallocMetadataNode* newMetadataNode = new MallocMetadataNode(previousBRK,size);

    mallocMetadataNodeList->tail->next = newMetadataNode;
    newMetadataNode->prev=mallocMetadataNodeList->tail;
    mallocMetadataNodeList->tail = newMetadataNode;

    return previousBRK;
}

void* scalloc(size_t num, size_t size){

}

void sfree(void* p){

}

void* srealloc(void* oldp, size_t size){

}

size_t _num_free_blocks(){
    return mallocMetadataNodeList->numOfFreeBlocks;
}

size_t _num_free_bytes(){
    return mallocMetadataNodeList->sizeOfFreeBlocks;
}

size_t _num_allocated_blocks(){
    size_t blocksTotalSizeWithoutMetadata = (mallocMetadataNodeList->sizeOfFreeBlocks + mallocMetadataNodeList->sizeOfUsedBlocks);
    size_t metadataTotalSize = (mallocMetadataNodeList->numOfFreeBlocks + mallocMetadataNodeList->numOfUsedBlocks)*(mallocMetadataNodeList->sizeOfSingleMetadataBlock);
    return blocksTotalSizeWithoutMetadata + metadataTotalSize;

}

size_t _num_allocated_bytes(){
    return (mallocMetadataNodeList->sizeOfFreeBlocks + mallocMetadataNodeList->sizeOfUsedBlocks);
}

size_t _num_meta_data_bytes(){
    return (mallocMetadataNodeList->numOfFreeBlocks + mallocMetadataNodeList->numOfUsedBlocks)*(mallocMetadataNodeList->sizeOfSingleMetadataBlock);
}

size_t _size_meta_data(){
    return mallocMetadataNodeList->sizeOfSingleMetadataBlock;
}
