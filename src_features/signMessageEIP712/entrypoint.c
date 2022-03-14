#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>


enum {
    OFFSET_CLA = 0,
    OFFSET_INS,
    OFFSET_P1,
    OFFSET_P2,
    OFFSET_LC,
    OFFSET_DATA
};

typedef enum
{
    // contract defined struct
    TYPE_CUSTOM = 0,
    // native types
    TYPE_SOL_INT,
    TYPE_SOL_UINT,
    TYPE_SOL_ADDRESS,
    TYPE_SOL_BOOL,
    TYPE_SOL_STRING,
    TYPE_SOL_BYTE,
    TYPE_SOL_BYTES_FIX,
    TYPE_SOL_BYTES_DYN,
    TYPES_COUNT
}   e_type;

typedef enum
{
    ARRAY_DYNAMIC = 0,
    ARRAY_FIXED_SIZE
}   e_array_type;


// APDUs INS
#define INS_STRUCT_DEF  0x18
#define INS_STRUCT_IMPL 0x1A

// TypeDesc masks
#define TYPE_MASK       (0xF)
#define ARRAY_MASK      (1 << 7)
#define TYPESIZE_MASK   (1 << 6)


#define     SIZE_MEM_BUFFER 1024
uint8_t     mem_buffer[SIZE_MEM_BUFFER];
uint16_t    mem_idx;

uint8_t     *structs_array;
uint8_t     *current_struct_fields_array;



bool    set_struct_name(uint8_t *data)
{
    // increment number of structs
    *structs_array += 1;

    // copy length
    mem_buffer[mem_idx++] = data[OFFSET_LC];

    // copy name
    memmove(&mem_buffer[mem_idx], &data[OFFSET_DATA], data[OFFSET_LC]);
    mem_idx += data[OFFSET_LC];

    // initialize number of fields
    current_struct_fields_array = &mem_buffer[mem_idx];
    mem_buffer[mem_idx++] = 0;
    return true;
}

bool    set_struct_field(uint8_t *data)
{
    uint8_t data_idx = OFFSET_DATA;
    uint8_t type_desc, len;

    // increment number of struct fields
    *current_struct_fields_array += 1;

    // copy TypeDesc
    type_desc = data[data_idx++];
    mem_buffer[mem_idx++] = type_desc;

    // check TypeSize flag in TypeDesc
    if (type_desc & TYPESIZE_MASK)
    {
        // copy TypeSize
        mem_buffer[mem_idx++] = data[data_idx++];
    }
    else if ((type_desc & TYPE_MASK) == TYPE_CUSTOM)
    {
        len = data[data_idx++];
        // copy custom struct name length
        mem_buffer[mem_idx++] = len;
        // copy name
        memmove(&mem_buffer[mem_idx], &data[data_idx], len);
        mem_idx += len;
        data_idx += len;
    }
    if (type_desc & ARRAY_MASK)
    {
        len = data[data_idx++];
        mem_buffer[mem_idx++] = len;
        while (len-- > 0)
        {
            mem_buffer[mem_idx++] = data[data_idx];
            switch (data[data_idx++])
            {
                case ARRAY_DYNAMIC: // nothing to do
                    break;
                case ARRAY_FIXED_SIZE:
                    mem_buffer[mem_idx++] = data[data_idx++];
                    break;
                default:
                    // should not be in here :^)
                    break;
            }
        }
    }

    // copy length
    len = data[data_idx++];
    mem_buffer[mem_idx++] = len;

    // copy name
    memmove(&mem_buffer[mem_idx], &data[data_idx], len);
    mem_idx += len;
    return true;
}

void    dump_mem(void)
{
    uint8_t *mem = structs_array + 1;
    uint8_t type_desc;
    uint8_t fields_count;
    uint8_t array_depth;

    for (int i = 0; i < *structs_array; ++i)
    {
        fwrite(mem + 1, *mem, sizeof(*mem), stdout);
        printf("(");
        mem += (1 + *mem);
        fields_count = *mem;
        mem += 1;
        for (int y = 0; y < fields_count; ++y)
        {
            if (y > 0) printf(",");
            type_desc = *mem;
            mem += 1;
            switch (type_desc & TYPE_MASK)
            {
                case TYPE_CUSTOM:
                    fwrite(mem + 1, *mem, sizeof(*mem), stdout);
                    mem += (1 + *mem);
                    if (type_desc & TYPESIZE_MASK) mem += 1;
                    break;
                case TYPE_SOL_UINT:
                    printf("u");
                case TYPE_SOL_INT:
                    printf("int");
                    if (type_desc & TYPESIZE_MASK)
                    {
                        printf("%d", (*mem * 8));
                        mem += 1;
                    }
                    break;
                case TYPE_SOL_ADDRESS:
                    printf("address");
                    if (type_desc & TYPESIZE_MASK) mem += 1;
                    break;
                case TYPE_SOL_BOOL:
                    printf("bool");
                    if (type_desc & TYPESIZE_MASK) mem += 1;
                    break;
                case TYPE_SOL_STRING:
                    printf("string");
                    if (type_desc & TYPESIZE_MASK) mem += 1;
                    break;
                case TYPE_SOL_BYTE:
                    printf("byte");
                    if (type_desc & TYPESIZE_MASK) mem += 1;
                    break;
                case TYPE_SOL_BYTES_FIX:
                    printf("bytes");
                    if (type_desc & TYPESIZE_MASK)
                    {
                        printf("%d", *mem);
                        mem += 1;
                    }
                    break;
                case TYPE_SOL_BYTES_DYN:
                    printf("bytes");
                    if (type_desc & TYPESIZE_MASK) mem += 1;
                    break;
                default:
                    // should not be in here :^)
                    break;
            }
            if (type_desc & ARRAY_MASK)
            {
                array_depth = *mem++;
                while (array_depth-- > 0)
                {
                    switch (*mem++)
                    {
                        case ARRAY_DYNAMIC:
                            printf("[]");
                            break;
                        case ARRAY_FIXED_SIZE:
                            printf("[%u]", *mem++);
                            break;
                        default:
                            // should not be in here :^)
                            break;
                    }
                }
            }
            printf(" ");
            fwrite(mem + 1, *mem, sizeof(*mem), stdout);
            mem += (1 + *mem);
        }
        printf(")\n");
    }
}

bool    handle_apdu(uint8_t *data)
{
    switch (data[OFFSET_INS])
    {
        case INS_STRUCT_DEF:
            switch (data[OFFSET_P2])
            {
                case 0x00:
                    set_struct_name(data);
                    break;
                case 0xFF:
                    set_struct_field(data);
                    break;
                default:
                    printf("Unknown P2 0x%x for APDU 0x%x\n", data[OFFSET_P2], data[OFFSET_INS]);
                    return false;
            }
            break;
        case INS_STRUCT_IMPL:
            break;
        default:
            printf("Unrecognized APDU");
            return false;
    }
    return true;
}

void    init_heap(void)
{
    // init global variables
    mem_idx = 0;

    // set types pointer
    structs_array = &mem_buffer[mem_idx];

    // create len(types)
    mem_buffer[mem_idx++] = 0;
}

int     main(void)
{
    uint8_t         buf[256];
    uint8_t         idx;
    int             state;
    uint8_t         payload_size;

    init_heap();

    state = OFFSET_CLA;
    idx = 0;
    while (true)
    {
        if (fread(&buf[idx], sizeof(buf[0]), 1, stdin) == 0) break;
        switch (state)
        {
            case OFFSET_CLA:
            case OFFSET_INS:
            case OFFSET_P1:
            case OFFSET_P2:
                state += 1;
                idx += 1;
                break;
            case OFFSET_LC:
                payload_size = buf[idx];
                state = OFFSET_DATA;
                idx += 1;
                break;
            case OFFSET_DATA:
                if (--payload_size == 0)
                {
                    handle_apdu(buf);
                    state = OFFSET_CLA;
                    idx = 0;
                }
                else idx += 1;
                break;
            default:
                printf("Unexpected APDU state!\n");
                return EXIT_FAILURE;
        }
    }
    dump_mem();
    printf("\n%d bytes used in RAM\n", mem_idx);
    return EXIT_SUCCESS;
}
