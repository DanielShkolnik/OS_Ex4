//
// Created by danie on 13/06/2020.
//

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>

typedef struct MallocMetadataNode_t{
public:
    size_t size; // actual allocated block size
    size_t usedSize;
    bool is_free;
    MallocMetadataNode_t* next;
    MallocMetadataNode_t* prev;
    intptr_t numOfBytesToAlign;
}MallocMetadataNode;


void initMallocMetadataNode(MallocMetadataNode* node, size_t size, intptr_t alignAddress){
    node->size = size;
    node->usedSize = size;
    node->is_free = false;
    node->next = nullptr;
    node->prev = nullptr;
    node->numOfBytesToAlign = alignAddress;
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


size_t numOfBytesToAlignAdrress(intptr_t address){
    if(address%8==0) return 0;
    else return  8 - address%8;
}

size_t AlignSize(size_t size){
    if(size%8==0) return size;
    else return   size + 8 - size%8;
}

size_t sizeOfMallocMetadataNode = AlignSize(sizeof(MallocMetadataNode));


void blockSplit(MallocMetadataNode* block, size_t usedSize, bool isMalloc){
    size_t prevSize = block->size;
    block->size=usedSize + sizeOfMallocMetadataNode;
    block->is_free = false;
    MallocMetadataNode* newBlock = (MallocMetadataNode*)((unsigned long)((MallocMetadataNode*)block+1)+usedSize);
    size_t alignAddress = numOfBytesToAlignAdrress((intptr_t)newBlock);
    newBlock = (MallocMetadataNode*)((char*)newBlock + alignAddress);
    newBlock->numOfBytesToAlign = alignAddress;
    newBlock->size = prevSize - block->size;
    newBlock->is_free = true;
    newBlock->prev = block;
    newBlock->next = block->next;
    if(newBlock->next != nullptr) newBlock->next->prev = newBlock;
    block->next = newBlock;


    if(isMalloc){
        mallocMetadataList.numOfUsedBlocks++;
        mallocMetadataList.sizeOfFreeBlocks-=block->size;
        mallocMetadataList.sizeOfUsedBlocks+=block->size;
    }
    else{
        mallocMetadataList.numOfFreeBlocks++;
        mallocMetadataList.sizeOfUsedBlocks-=newBlock->size;
        mallocMetadataList.sizeOfFreeBlocks+=newBlock->size;
    }

    if(mallocMetadataList.tail == block) mallocMetadataList.tail = newBlock;
};


void* enlargeWildernessBlock(size_t size){
    size_t wildernessBlockSize = mallocMetadataList.tail->size;
    MallocMetadataNode* newBlock = (MallocMetadataNode*)sbrk((intptr_t)(size+sizeOfMallocMetadataNode-wildernessBlockSize));
    if(newBlock == (void*)(-1)) return NULL;
    mallocMetadataList.tail->size = size+sizeOfMallocMetadataNode;
    if(mallocMetadataList.tail->is_free){
        mallocMetadataList.tail->is_free = false;
        mallocMetadataList.numOfUsedBlocks++;
        mallocMetadataList.numOfFreeBlocks--;
        mallocMetadataList.sizeOfUsedBlocks+=size+sizeOfMallocMetadataNode;
        mallocMetadataList.sizeOfFreeBlocks-=wildernessBlockSize;
    }
    else{
        mallocMetadataList.sizeOfUsedBlocks+=size+sizeOfMallocMetadataNode-wildernessBlockSize;
    }
    return (void*)((MallocMetadataNode*)mallocMetadataList.tail+1);
}


void* smalloc(size_t size){
    if(size<=0 || size>1e8) return NULL;

    if(size>=128*1024){
        MallocMetadataNode* newBlock = (MallocMetadataNode*)mmap(NULL,AlignSize((size_t)(size+sizeOfMallocMetadataNode+7)),PROT_READ|PROT_WRITE|PROT_EXEC,MAP_ANON|MAP_PRIVATE,-1,0);
        if(newBlock == (void*)(-1)) return NULL;

        size_t alignAddress = numOfBytesToAlignAdrress((intptr_t)newBlock);
        newBlock = (MallocMetadataNode*)((char*)newBlock + alignAddress);

        initMallocMetadataNode(newBlock,size+sizeOfMallocMetadataNode,alignAddress);

        mallocMetadataList.numOfUsedBlocks++;
        mallocMetadataList.sizeOfUsedBlocks+=size+sizeOfMallocMetadataNode;

        return (void*)((MallocMetadataNode*)newBlock+1);
    }
    else{
        MallocMetadataNode* current = mallocMetadataList.head;

        while (current != nullptr){
            if(current->is_free && current->size>=(size+sizeOfMallocMetadataNode)){
                if(current->size>=size+2*sizeOfMallocMetadataNode+128+7){
                    blockSplit((MallocMetadataNode*)current,size,true);
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

        if(mallocMetadataList.tail != nullptr && mallocMetadataList.tail->is_free){
            return enlargeWildernessBlock(size);
        }
        else{
            MallocMetadataNode* newBlock = (MallocMetadataNode*)sbrk((intptr_t)(size+sizeOfMallocMetadataNode+7));
            if(newBlock == (void*)(-1)) return NULL;

            size_t alignAddress = numOfBytesToAlignAdrress((intptr_t)newBlock);
            newBlock = (MallocMetadataNode*)((char*)newBlock + alignAddress);

            initMallocMetadataNode(newBlock,size+sizeOfMallocMetadataNode,alignAddress);

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
            mallocMetadataList.sizeOfUsedBlocks+=size+sizeOfMallocMetadataNode;

            return (void*)((MallocMetadataNode*)newBlock+1);
        }
    }
}

void* scalloc(size_t num, size_t size){
    if(size<=0 || num*size>1e8 || num<=0) return NULL;

    void* newBlockPtr = smalloc(size*num);
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
        if(prevBlock->next == nullptr) mallocMetadataList.tail = prevBlock;
        if(prevBlock->prev == nullptr) mallocMetadataList.head = prevBlock;
    }
        //left block is free
    else if(prevBlock!= nullptr && prevBlock->is_free){
        prevBlock->size += block->size;
        prevBlock->next = block->next;
        if(block->next!= nullptr) block->next->prev = prevBlock;
        mallocMetadataList.numOfFreeBlocks--;
        if(prevBlock->next == nullptr) mallocMetadataList.tail = prevBlock;
        if(prevBlock->prev == nullptr) mallocMetadataList.head = prevBlock;

    }
        //right block is free
    else if(nextBlock!= nullptr && nextBlock->is_free){
        block->size += nextBlock->size;
        block->next = nextBlock->next;
        if(nextBlock->next!= nullptr) nextBlock->next->prev = block;
        mallocMetadataList.numOfFreeBlocks--;
        if(nextBlock->next == nullptr) mallocMetadataList.tail = nextBlock;
        if(nextBlock->prev == nullptr) mallocMetadataList.head = nextBlock;
    }

}

void sfree(void* p){
    if(p == NULL) return;
    MallocMetadataNode* current = (MallocMetadataNode*)p-1;
    if(current->size>=(size_t)(128*1024+sizeOfMallocMetadataNode)){
        mallocMetadataList.numOfUsedBlocks--;
        mallocMetadataList.sizeOfUsedBlocks-=current->usedSize;
        munmap((void*)((char*)p-current->numOfBytesToAlign),current->size);
    }
    else{
        if(current->is_free) return;
        current->is_free = true;
        mallocMetadataList.numOfFreeBlocks++;
        mallocMetadataList.numOfUsedBlocks--;
        mallocMetadataList.sizeOfFreeBlocks+=current->size;
        mallocMetadataList.sizeOfUsedBlocks-=current->size;
        if((current->prev!= nullptr && current->prev->is_free) || (current->next!= nullptr && current->next->is_free)) blockCombine(current);
    }
}

void* srealloc(void* oldp, size_t size){
    if(size<=0 || size>1e8) return NULL;
    if(oldp == NULL) return smalloc(size);
    MallocMetadataNode* current = (MallocMetadataNode*)oldp-1;
    size_t oldpSize = current->size-sizeOfMallocMetadataNode;
    if(size+sizeOfMallocMetadataNode<=current->size){
        if(current->size<128*1024){
            if(current->size>=size+2*sizeOfMallocMetadataNode+128+7) blockSplit((MallocMetadataNode*)current,size,false);
        }
        mallocMetadataList.sizeOfUsedBlocks-=current->size-(size+sizeOfMallocMetadataNode);
        current->usedSize=size+sizeOfMallocMetadataNode;
        return (void*)((MallocMetadataNode*)current+1);
    }
    if(size>=128*1024){
        void* newBlockPtr = smalloc(size);
        if(newBlockPtr == NULL) return NULL;

        memmove(newBlockPtr,oldp,oldpSize);
        sfree(oldp);

        return newBlockPtr;
    }
    else{
        MallocMetadataNode* prevBlock = current->prev;
        MallocMetadataNode* nextBlock = current->next;
        //realloc on wilderness block
        if(nextBlock == nullptr){
            void* newBlockPtr = enlargeWildernessBlock(size);
            if(newBlockPtr == NULL) return NULL;
            memmove(newBlockPtr,oldp,oldpSize);
            return newBlockPtr;
        }
        else{
            //Combine left
            if(prevBlock!= nullptr && prevBlock->is_free && prevBlock->size+current->size>=size+sizeOfMallocMetadataNode){
                mallocMetadataList.sizeOfUsedBlocks+=prevBlock->size;
                mallocMetadataList.sizeOfFreeBlocks-=prevBlock->size;
                prevBlock->is_free=false;
                prevBlock->size += current->size;
                prevBlock->next = current->next;
                if(current->next!= nullptr) current->next->prev = prevBlock;
                mallocMetadataList.numOfFreeBlocks--;
                if(mallocMetadataList.tail == current) mallocMetadataList.tail = prevBlock;

                memmove((void*)((MallocMetadataNode*)prevBlock+1),oldp,oldpSize);

                if(prevBlock->size>=size+2*sizeOfMallocMetadataNode+128+7) {
                    blockSplit((MallocMetadataNode*)prevBlock,size, false);
                    return (void *) ((MallocMetadataNode *) prevBlock + 1);
                }

                return (void*)((MallocMetadataNode*)prevBlock+1);
            }
                //Combine right
            else if(nextBlock!= nullptr && nextBlock->is_free && nextBlock->size+current->size>=size+sizeOfMallocMetadataNode){
                mallocMetadataList.sizeOfUsedBlocks+=nextBlock->size;
                mallocMetadataList.sizeOfFreeBlocks-=nextBlock->size;
                current->is_free=false;
                current->size += nextBlock->size;
                current->next = nextBlock->next;
                if(nextBlock->next!= nullptr) nextBlock->next->prev = current;
                mallocMetadataList.numOfFreeBlocks--;

                memmove((void*)((MallocMetadataNode*)current+1),oldp,oldpSize);

                if(current->size>=size+2*sizeOfMallocMetadataNode+128+7) {
                    blockSplit((MallocMetadataNode*)current,size, false);
                    return (void *) ((MallocMetadataNode *) current + 1);
                }

                return (void*)((MallocMetadataNode*)current+1);
            }
                //Combine left and right
            else if(prevBlock!= nullptr && prevBlock->is_free && nextBlock!= nullptr && nextBlock->is_free && prevBlock->size+nextBlock->size+current->size>=size+sizeOfMallocMetadataNode){
                mallocMetadataList.sizeOfUsedBlocks+=prevBlock->size + nextBlock->size;
                mallocMetadataList.sizeOfFreeBlocks-=prevBlock->size + nextBlock->size;
                prevBlock->is_free=false;
                prevBlock->size += current->size + nextBlock->size;
                prevBlock->next = nextBlock->next;
                if(nextBlock->next!= nullptr) nextBlock->next->prev = prevBlock;
                mallocMetadataList.numOfFreeBlocks-=2;
                if(mallocMetadataList.tail == current) mallocMetadataList.tail = prevBlock;

                memmove((void*)((MallocMetadataNode*)prevBlock+1),oldp,oldpSize);

                if(prevBlock->size>=size+2*sizeOfMallocMetadataNode+128+7) {
                    blockSplit((MallocMetadataNode*)prevBlock,size, false);
                    return (void *) ((MallocMetadataNode *) prevBlock + 1);
                }

                return (void*)((MallocMetadataNode*)prevBlock+1);
            }
                //No blocks to combine
            else{
                void* newBlockPtr = smalloc(size);
                if(newBlockPtr == NULL) return NULL;
                memmove(newBlockPtr,oldp,oldpSize);
                sfree(oldp);
                return newBlockPtr;
            }
        }
    }
}

size_t _num_free_blocks(){
    return mallocMetadataList.numOfFreeBlocks;
}

size_t _num_free_bytes(){
    return mallocMetadataList.sizeOfFreeBlocks-mallocMetadataList.numOfFreeBlocks*sizeOfMallocMetadataNode;
}

size_t _num_allocated_blocks(){
    return  mallocMetadataList.numOfUsedBlocks + mallocMetadataList.numOfFreeBlocks;

}

size_t _num_allocated_bytes(){
    return mallocMetadataList.sizeOfUsedBlocks + mallocMetadataList.sizeOfFreeBlocks - (mallocMetadataList.numOfFreeBlocks+mallocMetadataList.numOfUsedBlocks)*sizeOfMallocMetadataNode;
}

size_t _num_meta_data_bytes(){
    return (mallocMetadataList.numOfFreeBlocks+mallocMetadataList.numOfUsedBlocks)*sizeOfMallocMetadataNode;
}

size_t _size_meta_data(){
    return sizeOfMallocMetadataNode;
}
