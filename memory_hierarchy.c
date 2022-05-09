/*************************************************************************************|
|   1. YOU ARE NOT ALLOWED TO SHARE/PUBLISH YOUR CODE (e.g., post on piazza or online)|
|   2. Fill memory_hierarchy.c                                                        |
|   3. Do not use any other .c files neither alter mipssim.h or parser.h              |
|   4. Do not include any other library files                                         |
|*************************************************************************************/

#include "mipssim.h"

uint32_t cache_type = 0;

#define OFFSET 4 //Offset is always 4
#define MASK 12 //Mask of offset

typedef uint32_t arrCacheLine[4];

//Cache Variables: 0:Direct/Fully Assoc, 1:Two-way
int lineNum;
int indBitsNum;
int tagBitsNum;
uint32_t *arrTag0;
uint32_t *arrTag1;
uint8_t *arrValBit0;
uint8_t *arrValBit1;
arrCacheLine *arrData0;
arrCacheLine *arrData1;
int *arrLRU0;
int *arrLRU1;
int currIndex;
int updatedIndex;

void memory_state_init(struct architectural_state *arch_state_ptr)
{
    arch_state_ptr->memory = (uint32_t *)calloc(MEMORY_WORD_NUM, sizeof(uint32_t));
    if (cache_size == 0)
    {
        // CACHE DISABLED
        memory_stats_init(arch_state_ptr, 0); // WARNING: we initialize for no cache 0
    }
    else
    {
        switch (cache_type)
        {
        case CACHE_TYPE_DIRECT: // direct mapped
            lineNum = cache_size / 16;
            indBitsNum = log2(lineNum);
            tagBitsNum = 32 - indBitsNum - OFFSET;
            arrData0 = (arrCacheLine *)malloc(sizeof(arrCacheLine) *lineNum);
            arrValBit0 = (uint8_t *)calloc(lineNum, sizeof(uint8_t));
            arrTag0 = (uint32_t *)malloc(tagBitsNum *lineNum);
            break;
        case CACHE_TYPE_FULLY_ASSOC: // fully associative
            lineNum = cache_size / 16;
            tagBitsNum = 32 - OFFSET;
            arrData0 = (arrCacheLine *)malloc(sizeof(arrCacheLine) *lineNum);
            arrValBit0 = (uint8_t *)calloc(lineNum, sizeof(uint8_t));
            arrTag0 = (uint32_t *)malloc(tagBitsNum *lineNum);
            arrLRU0 = (int *)calloc(lineNum, sizeof(int));
            currIndex, updatedIndex = 0;
            break;
        case CACHE_TYPE_2_WAY: // 2-way associative
            lineNum = cache_size / 32;
            indBitsNum = log2(lineNum);
            tagBitsNum = 32 - OFFSET - indBitsNum ;
            arrData0 = (arrCacheLine *)malloc(sizeof(arrCacheLine) *lineNum);
            arrValBit0 = (uint8_t *)calloc(lineNum, sizeof(uint8_t));
            arrTag0 = (uint32_t *)malloc(tagBitsNum *lineNum);
            arrLRU0 = (int *)calloc(lineNum, sizeof(int));
            arrData1 = (arrCacheLine *)malloc(sizeof(arrCacheLine) *lineNum);
            arrValBit1 = (uint8_t *)calloc(lineNum, sizeof(uint8_t));
            arrTag1 = (uint32_t *)malloc(tagBitsNum *lineNum);
            arrLRU1 = (int *)calloc(lineNum, sizeof(int));
            updatedIndex = 0;
            break;
        }
        memory_stats_init(arch_state_ptr,tagBitsNum);
    }
}
void upLRU(int index)
{
    updatedIndex++;
    arrLRU0[index] = updatedIndex;
    for (int i = 0; i <lineNum; i++)
    {
        if (arrLRU0[currIndex] > arrLRU0[i])
        {
        currIndex = i;
        }
    }
}
// returns data on memory[address / 4]
int memory_read(int address)
{
    arch_state.mem_stats.lw_total++;
    check_address_is_word_aligned(address);
    uint32_t tagBits = (uint32_t)address >> (32 - tagBitsNum);

    int tagline = address >>OFFSET;
    int indexBits = get_piece_of_a_word(address, OFFSET, indBitsNum);
    tagline = tagline *OFFSET;

    uint32_t offset_lw = (uint32_t)(address & MASK) / OFFSET; //12 is MASK of the OFFSET
    int condition0;
    int condition1;
    if (cache_size == 0)
    {
        // CACHE DISABLED
        return (int)arch_state.memory[address / 4];
    }
    else
    {
        // CACHE ENABLED
        switch (cache_type)
        {
        case CACHE_TYPE_DIRECT: // direct mapped
            condition0 = ((arrValBit0[indexBits] == 1) && (arrTag0[indexBits]) == tagBits) ? 1:0;
            switch (condition0)
            {
            case 1:
                arch_state.mem_stats.lw_cache_hits++;
                printf("!!!  Read Hit  !!!\n");
                return (int)arrData0[indexBits][offset_lw];
                break;
            case 0:
                for (int i = 0; i < OFFSET; i++)
                {
                    arrData0[indexBits][i] = arch_state.memory[tagline + i];
                }
                arrTag0[indexBits] = tagBits;
                arrValBit0[indexBits] = 1;
                printf("!!!  Read Miss  !!!\n");
                return (int)arrData0[indexBits][offset_lw];
                break;
            }
            break;
        case CACHE_TYPE_FULLY_ASSOC: // fully associative
            for (int i = 0; i < lineNum; i++){
                condition0 = ((arrValBit0[i] == 1) && (arrTag0[i]) == tagBits) ? 1:0;
                switch (condition0)
                {
                case 1:
                    arch_state.mem_stats.lw_cache_hits++;
                    upLRU(i);
                    printf("!!! Read Hit !!!\n");
                    return (int)arrData0[i][offset_lw];
                    break;                
                case 0:
                    break;
                }

            }
            for (int i = 0; i <OFFSET; i++)
            {
                arrData0[currIndex][i] = arch_state.memory[tagline + i];
            }
            arrTag0[currIndex] = tagBits;
            arrValBit0[currIndex] = 1;
            upLRU(currIndex);
            printf("!!! Read Miss !!!\n");
            return (int)arch_state.memory[address / 4];
            break;
        case CACHE_TYPE_2_WAY: // 2-way associative
            condition0 = ((arrValBit0[indexBits] == 1) && (arrTag0[indexBits]) == tagBits) ? 1:0;
            condition1 = ((arrValBit1[indexBits] == 1) && (arrTag1[indexBits]) == tagBits) ? 1:0;
            if(condition0||condition1){
                arch_state.mem_stats.lw_cache_hits++;
                updatedIndex++;
                if (condition0)
                {
                    arrLRU0[indexBits] =updatedIndex;
                    printf("!!! 0 Read Hit 0 !!!\n");
                    return (int)arrData0[indexBits][offset_lw];
                }
                else
                {
                    arrLRU1[indexBits] =updatedIndex;
                    printf("!!! 1 Read Hit 1 !!!\n");
                    return (int)arrData1[indexBits][offset_lw];
                }   
            }
            else if (arrLRU0[indexBits] <= arrLRU1[indexBits])
            {
                for (int i = 0; i <OFFSET; i++)
                {
                    arrData0[indexBits][i] = arch_state.memory[tagline + i];
                }
                arrTag0[indexBits] = tagBits;
                arrValBit0[indexBits] = 1;
                updatedIndex++;
                arrLRU0[indexBits] =updatedIndex;
                printf("!!! 0 Read Miss 0 !!!\n");
                return (int)arrData0[indexBits][offset_lw];
            }
            else
            {
                for (int i = 0; i < OFFSET; i++)
                {
                    arrData1[indexBits][i] = arch_state.memory[tagline + i];
                }
                arrTag1[indexBits] = tagBits;
                arrValBit1[indexBits] = 1;
                updatedIndex++;
                arrLRU1[indexBits] =updatedIndex;
                printf("!!! 1 Read Miss 1 !!!\n");
                return (int)arrData1[indexBits][offset_lw];
            }
            break;
        }
    }
    return 0;
}

// writes data on memory[address / 4]
void memory_write(int address, int write_data)
{
    arch_state.mem_stats.sw_total++;
    check_address_is_word_aligned(address);
    uint32_t tagBits = (uint32_t)address >> (32 - tagBitsNum);

    int indexBits = get_piece_of_a_word(address, OFFSET, indBitsNum);

    uint32_t offset_sw = (uint32_t)(address & MASK) / OFFSET; 
    int condition0;
    int condition1;
    if (cache_size == 0)
    {
        // CACHE DISABLED
        arch_state.memory[address / 4] = (uint32_t)write_data;
    }
    else
    {
        // CACHE ENABLED
        /// @students: your implementation must properly increment: arch_state_ptr->mem_stats.sw_cache_hits
        switch (cache_type)
        {
        case CACHE_TYPE_DIRECT: // direct mapped
            condition0 = ((arrValBit0[indexBits] == 1) && (arrTag0[indexBits]) == tagBits) ? 1:0;
            switch (condition0)
            {
            case 1:
                arch_state.mem_stats.sw_cache_hits++;
                arrData0[indexBits][offset_sw] = (uint32_t)write_data;
                arch_state.memory[address / 4] = (uint32_t)write_data;
                printf("!!!Write Hit!!!\n");
                break;
            case 0:
                arch_state.memory[address / 4] = (uint32_t)write_data;
                printf("!!!Write Miss!!! \n");
                break;
            }
            break;
        case CACHE_TYPE_FULLY_ASSOC: // fully associative
            for (int i = 0; i < lineNum; i++){
                condition0 = ((arrValBit0[i] == 1) && (arrTag0[i]) == tagBits) ? 1:0;
                switch (condition0)
                {
                case 1:
                    arch_state.mem_stats.sw_cache_hits++;
                    upLRU(i);
                    arrData0[i][offset_sw] = (uint32_t)write_data;
                    printf("!!!Write Hit!!!\n");
                    break;
                case 0:
                    break;
                }
            }
            arch_state.memory[address / 4] = (uint32_t)write_data;
            printf("!!!Write Miss!!! \n");
            break;
        case CACHE_TYPE_2_WAY: // 2-way associative
            condition0 = ((arrValBit0[indexBits] == 1) && (arrTag0[indexBits]) == tagBits) ? 1:0;
            condition1 = ((arrValBit1[indexBits] == 1) && (arrTag1[indexBits]) == tagBits) ? 1:0;
            if(condition0||condition1){
                arch_state.mem_stats.sw_cache_hits++;
                if (condition0)
                {
                    arrData0[indexBits][offset_sw] = (uint32_t)write_data;
                }
                else
                {
                    arrData1[indexBits][offset_sw] = (uint32_t)write_data;
                }
                arch_state.memory[address / 4] = (uint32_t)write_data;
                updatedIndex++;
                if (condition0)
                {
                    arrLRU0[indexBits] = updatedIndex;
                    printf("!!! 0 Write Hit 0 !!!\n");
                }
                else
                {
                    arrLRU1[indexBits] = updatedIndex;
                    printf("!!! 1 Write Hit 1 !!! \n");
                } 
            }
            else
            {
                arch_state.memory[address / 4] = (uint32_t)write_data;
                printf("!!!Write Miss!!! \n");
            }
            break;
        }
    }
}