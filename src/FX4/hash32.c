
#include "hash32.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef FX4_COMPILER

#include "program.h"

#define FNV_32BIT_OFFSET_BASIS    (uint32_t)0x811c9dc5
#define FNV_32BIT_PRIME           (uint32_t)16777619

typedef struct{
    uint32_t hash;
    char *s;
} hash_lookup_t;

#define HASH_DB_SIZE 128
static hash_lookup_t hash_db[HASH_DB_SIZE];


static uint32_t fnv1a( uint32_t base, uint8_t *data, uint16_t len ){

    uint32_t hash = base;

    while( len > 0 ){

        hash ^= *data;
        hash *= FNV_32BIT_PRIME;

        len--;
        data++;
    }

    return hash;
}

char* hash_s_get_str(uint32_t hash){

    for(int i = 0; i < HASH_DB_SIZE; i++){

        if(hash_db[i].hash == hash){

            return hash_db[i].s;
        }
    }

    return NULL;
}

uint32_t hash_u32_data( uint8_t *data, uint16_t len ){

    if( len == 0 ){

        return 0;
    }

    uint32_t hash = fnv1a( FNV_32BIT_OFFSET_BASIS, data, len );

    if(hash_s_get_str(hash) == 0){

        // add to lookup
        for(int i = 0; i < HASH_DB_SIZE; i++){

            if(hash_db[i].hash == 0){

                hash_db[i].hash = hash;

                hash_db[i].s = malloc(len + 1);
                memset(hash_db[i].s, 0, len + 1);
                memcpy(hash_db[i].s, data, len);

                break;
            }
        }    
    }

    return hash;
}

uint32_t hash_u32_string( char *src ){

    uint8_t len = strnlen( src, 255 );

    return hash_u32_data( (uint8_t *)src, len );
}

void hash_v_print(void){

    printf("Strings:\n");

    for(int i = 0; i < HASH_DB_SIZE; i++){

        if(hash_db[i].hash == 0){

            continue;
        }

        printf("\t%s\n", hash_db[i].s);
    }
}

int hash_get_count(void){

    int count = 0;

    for(int i = 0; i < HASH_DB_SIZE; i++){

        if(hash_db[i].hash == 0){

            continue;
        }

        count++;
    }

    return count;
}

char* hash_get_string_at_index(int index){

    if(index >= cnt_of_array(hash_db)){

        return NULL;
    }

    return hash_db[index].s;
}

#endif