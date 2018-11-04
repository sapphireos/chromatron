// <license>
// 
//     This file is part of the Sapphire Operating System.
// 
//     Copyright (C) 2013-2018  Jeremy Billheimer
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

#include "trig.h"

#ifdef ESP8266
    #define PROGMEM
    #define pgm_read_word(a) *a
#else
    #include "sapphire.h"
#endif


static const PROGMEM int16_t sine_lookup[] = {
    0	,
    201	,
    402	,
    603	,
    804	,
    1005	,
    1206	,
    1407	,
    1608	,
    1809	,
    2009	,
    2210	,
    2410	,
    2611	,
    2811	,
    3012	,
    3212	,
    3412	,
    3612	,
    3811	,
    4011	,
    4210	,
    4410	,
    4609	,
    4808	,
    5007	,
    5205	,
    5404	,
    5602	,
    5800	,
    5998	,
    6195	,
    6393	,
    6590	,
    6786	,
    6983	,
    7179	,
    7375	,
    7571	,
    7767	,
    7962	,
    8157	,
    8351	,
    8545	,
    8739	,
    8933	,
    9126	,
    9319	,
    9512	,
    9704	,
    9896	,
    10087	,
    10278	,
    10469	,
    10659	,
    10849	,
    11039	,
    11228	,
    11417	,
    11605	,
    11793	,
    11980	,
    12167	,
    12353	,
    12539	,
    12725	,
    12910	,
    13094	,
    13279	,
    13462	,
    13645	,
    13828	,
    14010	,
    14191	,
    14372	,
    14553	,
    14732	,
    14912	,
    15090	,
    15269	,
    15446	,
    15623	,
    15800	,
    15976	,
    16151	,
    16325	,
    16499	,
    16673	,
    16846	,
    17018	,
    17189	,
    17360	,
    17530	,
    17700	,
    17869	,
    18037	,
    18204	,
    18371	,
    18537	,
    18703	,
    18868	,
    19032	,
    19195	,
    19357	,
    19519	,
    19680	,
    19841	,
    20000	,
    20159	,
    20317	,
    20475	,
    20631	,
    20787	,
    20942	,
    21096	,
    21250	,
    21403	,
    21554	,
    21705	,
    21856	,
    22005	,
    22154	,
    22301	,
    22448	,
    22594	,
    22739	,
    22884	,
    23027	,
    23170	,
    23311	,
    23452	,
    23592	,
    23731	,
    23870	,
    24007	,
    24143	,
    24279	,
    24413	,
    24547	,
    24680	,
    24811	,
    24942	,
    25072	,
    25201	,
    25329	,
    25456	,
    25582	,
    25708	,
    25832	,
    25955	,
    26077	,
    26198	,
    26319	,
    26438	,
    26556	,
    26674	,
    26790	,
    26905	,
    27019	,
    27133	,
    27245	,
    27356	,
    27466	,
    27575	,
    27683	,
    27790	,
    27896	,
    28001	,
    28105	,
    28208	,
    28310	,
    28411	,
    28510	,
    28609	,
    28706	,
    28803	,
    28898	,
    28992	,
    29085	,
    29177	,
    29268	,
    29358	,
    29447	,
    29534	,
    29621	,
    29706	,
    29791	,
    29874	,
    29956	,
    30037	,
    30117	,
    30195	,
    30273	,
    30349	,
    30424	,
    30498	,
    30571	,
    30643	,
    30714	,
    30783	,
    30852	,
    30919	,
    30985	,
    31050	,
    31113	,
    31176	,
    31237	,
    31297	,
    31356	,
    31414	,
    31470	,
    31526	,
    31580	,
    31633	,
    31685	,
    31736	,
    31785	,
    31833	,
    31880	,
    31926	,
    31971	,
    32014	,
    32057	,
    32098	,
    32137	,
    32176	,
    32213	,
    32250	,
    32285	,
    32318	,
    32351	,
    32382	,
    32412	,
    32441	,
    32469	,
    32495	,
    32521	,
    32545	,
    32567	,
    32589	,
    32609	,
    32628	,
    32646	,
    32663	,
    32678	,
    32692	,
    32705	,
    32717	,
    32728	,
    32737	,
    32745	,
    32752	,
    32757	,
    32761	,
    32765	,
    32766	,

};

#define QUARTER16 ( 65536 / 4 )

// 65535 = (65535/65536) * 360 degrees
int16_t sine( uint16_t a ){

    uint8_t index;

    if( a < QUARTER16 ){

        index = a / ( QUARTER16 / 256 );
        return pgm_read_word( &sine_lookup[index] );
    }
    else if( a < ( QUARTER16 * 2 ) ){

        index = ( a / ( QUARTER16 / 256 ) ) - 256;
        return pgm_read_word( &sine_lookup[255 - index] );
    }
    else if( a < ( QUARTER16 * 3 ) ){

        index = ( a / ( QUARTER16 / 256 ) ) - 512;
        return -1 * pgm_read_word( &sine_lookup[index] );
    }
    else{

        index = ( a / ( QUARTER16 / 256 ) ) - 768;
        return -1 * pgm_read_word( &sine_lookup[255 - index] );
    }
}

int16_t cosine( uint16_t a ){

    return sine( a + QUARTER16 );
}

int16_t triangle( uint16_t a ){

    if( a < QUARTER16 ){

        return a * 2;
    }
    else if( a < ( QUARTER16 * 2 ) ){

        return ( ( QUARTER16 * 2 ) - a ) * 2;
    }
    else if( a < ( QUARTER16 * 3 ) ){

        return ( ( QUARTER16 * 2 ) - a ) * 2;
    }
    else{

        return ( -1 * ( ( QUARTER16 * 4 ) - a ) ) * 2;
    }
}
