/*
// <license>
// 
//     This file is part of the Sapphire Operating System.
// 
//     Copyright (C) 2013-2021  Jeremy Billheimer
// 
// 
//     This program is free software: you can redistribute it and/or modify
//     it under the terms of the GNU General Public License as published by
//     the Free Software Foundation, either version 3 of the License, or
//     (at your option) any later version.
// 
//     This program is distributed in the hope that it will be useful,
//     but WITHOUT ANY WARRANTY; without even the implied warranty of
//     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//     GNU General Public License for more details.
// 
//     You should have received a copy of the GNU General Public License
//     along with this program.  If not, see <http://www.gnu.org/licenses/>.
// 
// </license>
 */

#include "bool.h"
#include "util.h"

// yes, we're writing our own fabs because for some reason avr-libc doesn't actually have it
float f_abs( float x ){

    if( x < 0.0 ){

        x *= -1.0;
    }

    return x;
}

uint16_t abs16( int16_t a ){

    if( a < 0 ){

        return a * -1;
    }

    return a;
}

uint32_t abs32( int32_t a ){

    if( a < 0 ){

        return a * -1;
    }

    return a;
}

uint64_t abs64( int64_t a ){

    if( a < 0 ){

        return a * -1;
    }

    return a;
}

// return 1 if a > b
// return -1 if a < b
// return 0 if a == b
int8_t util_i8_compare_sequence_u16( uint16_t a, uint16_t b ){

    int16_t distance = (int16_t)( a - b );

    if( distance < 0 ){

        return -1;
    }
    else if( distance > 0 ){

        return 1;
    }

    return 0;
}

// return 1 if a > b
// return -1 if a < b
// return 0 if a == b
int8_t util_i8_compare_sequence_u32( uint32_t a, uint32_t b ){

    int32_t distance = (int32_t)( a - b );

    if( distance < 0 ){

        return -1;
    }
    else if( distance > 0 ){

        return 1;
    }

    return 0;
}

uint16_t util_u16_linear_interp(
    uint16_t x,
    uint16_t x0,
    uint16_t y0,
    uint16_t x1,
    uint16_t y1 ){

    uint16_t y_diff = y1 - y0;
    uint16_t x_diff = x1 - x0;

    // avoid divide by 0
    if( x_diff == 0 ){

        return y0;
    }

    uint32_t temp = (uint32_t)( x - x0 ) * y_diff;
    temp /= x_diff;

    return y0 + temp;
}

void util_v_bubble_sort_u16( uint16_t *array, uint8_t len ){

    // bubble sort
    bool swapped;
    do{

        swapped = FALSE;
    
        for( uint8_t i = 1; i < len; i++ ){            

            if( array[i - 1] > array[i] ){

                // swap values
                uint16_t temp = array[i];
                array[i] = array[i - 1];
                array[i - 1] = temp;

                swapped = TRUE;
            }
        }                

    } while( swapped );
}

void util_v_bubble_sort_u32( uint32_t *array, uint8_t len ){

    // bubble sort
    bool swapped;
    do{

        swapped = FALSE;
    
        for( uint8_t i = 1; i < len; i++ ){            

            if( array[i - 1] > array[i] ){

                // swap values
                uint32_t temp = array[i];
                array[i] = array[i - 1];
                array[i - 1] = temp;

                swapped = TRUE;
            }
        }                

    } while( swapped );
}


void util_v_bubble_sort_reversed_u32( uint32_t *array, uint8_t len ){

    // bubble sort
    bool swapped;
    do{

        swapped = FALSE;
    
        for( uint8_t i = 1; i < len; i++ ){            

            if( array[i - 1] < array[i] ){

                // swap values
                uint32_t temp = array[i];
                array[i] = array[i - 1];
                array[i - 1] = temp;

                swapped = TRUE;
            }
        }                

    } while( swapped );
}

int16_t util_i16_ewma( int16_t new, int16_t old, uint8_t ratio ){

    int16_t temp = ( ( (int32_t)ratio * new ) / 256 ) +  
                   ( ( (int32_t)( 256 - ratio ) * old ) / 256 );

    // check if filter is unchanging
    if( temp == old ){

        // adjust by minimum
        if( new > old ){

            temp++;
        }
        else if( new < old ){

            temp--;
        }
    }   

    // check for rounding errors
    // if( ( new > old ) && ( temp < old ) ){

    //     temp = old;
    // }
    // else if( ( new < old ) && ( temp > old ) ){

    //     temp = old;
    // }

    return temp;
}

uint16_t util_u16_ewma( uint16_t new, uint16_t old, uint8_t ratio ){

    uint16_t temp = ( ( (uint32_t)ratio * new ) / 256 ) +  
                    ( ( (uint32_t)( 256 - ratio ) * old ) / 256 );

    // check if filter is unchanging
    if( temp == old ){

        // adjust by minimum
        if( new > old ){

            temp++;
        }
        else if( new < old ){

            temp--;
        }
    }   

    return temp;
}


uint32_t util_u32_ewma( uint32_t new, uint32_t old, uint8_t ratio ){

    uint32_t temp = ( ( (uint64_t)ratio * new ) / 256 ) +  
                    ( ( (uint64_t)( 256 - ratio ) * old ) / 256 );

    // check if filter is unchanging
    if( temp == old ){

        // adjust by minimum
        if( new > old ){

            temp++;
        }
        else if( new < old ){

            temp--;
        }
    }   

    return temp;
}

uint8_t util_u8_average( uint8_t data[], uint8_t len ){

    uint16_t sum = 0;
    for( uint8_t i = 0; i < len; i++ ){

        sum += data[i];
    }

    return sum / len;
}

int16_t util_i16_average( int16_t data[], uint16_t len ){

    int32_t sum = 0;
    for( uint16_t i = 0; i < len; i++ ){

        sum += data[i];
    }

    return sum / len;
}

uint16_t util_u16_average( uint16_t data[], uint16_t len ){

    uint32_t sum = 0;
    for( uint16_t i = 0; i < len; i++ ){

        sum += data[i];
    }

    return sum / len;
}


