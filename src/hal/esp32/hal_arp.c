// <license>
// 
//     This file is part of the Sapphire Operating System.
// 
//     Copyright (C) 2013-2022  Jeremy Billheimer
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


#include "system.h"

#include "lwip/netif.h"
#include "lwip/etharp.h"

#include "hal_arp.h"



bool hal_arp_b_find( ip_addr4_t ip ){

    if( ip_b_check_broadcast( ip ) || !ip_b_check_subnet( ip ) ){

        // report true for broadcasts and for IPs not on the subnet (since they'll route through a gateway)
        // they aren't in the ARP table, but they don't use ARP.

        return TRUE;
    }

    ip4_addr_t ipaddr;
    ipaddr.addr = HTONL( ip_u32_to_int( ip ) );

    struct eth_addr *eth_ret;
    const ip4_addr_t *ipaddr_ret;

    for (struct netif* interface = netif_list; interface != 0; interface = interface->next){

        if( etharp_find_addr( interface, &ipaddr, &eth_ret, &ipaddr_ret ) >= 0 ){

            return TRUE;
        }
    }

    return FALSE;
}


void hal_arp_v_gratuitous_arp( void ){

    for (struct netif* interface = netif_list; interface != 0; interface = interface->next){
        
        if( interface->hwaddr_len != ETH_HWADDR_LEN ){

            continue;
        }

        etharp_gratuitous(interface);
    }
}