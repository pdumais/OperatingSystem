#include "utils.h"
#include "netcard.h"

#define ROUTE_ENTRY_COUNT 128

extern struct NetworkConfig* net_getConfig(unsigned char index);
extern unsigned long arp_getMAC(unsigned int ip, unsigned char* interface);

struct RouteEntry
{
    unsigned int    network;
    unsigned int    netmask;
    unsigned int    gateway;
    unsigned char   metric;
    unsigned char   interface;
};

struct RouteEntry routingTable[ROUTE_ENTRY_COUNT]={0};

void ip_routing_addRoute(unsigned int network, unsigned int netmask, unsigned int gateway, unsigned char metric, unsigned long interface)
{
    if (interface>255) return;
    
    unsigned long i;
    for (i=0;i<ROUTE_ENTRY_COUNT;i++)
    {
        struct RouteEntry *route = &routingTable[i];
        if (route->metric == 0)
        {
            route->network = network;
            route->netmask = netmask;
            route->gateway = gateway;
            route->metric = metric;
            route->interface = (unsigned char)interface;
            return;
        }
    }
}


unsigned long ip_routing_route(unsigned int destinationIP, unsigned char* interface)
{
    struct RouteEntry *chosenRoute = 0;
    unsigned long i;
    for (i=0;i<ROUTE_ENTRY_COUNT;i++)
    {
        struct RouteEntry *route = &routingTable[i];
        if (route->metric>0)
        {
            if ((destinationIP&route->netmask) == route->network)
            {
                if (chosenRoute!=0)
                {
                    if (chosenRoute->metric > route->metric)
                    {
                        chosenRoute = route;
                    }
                }
                else
                {
                    chosenRoute = route;
                }
            }
        }
    }

    if (chosenRoute==0) return 0;

    // A gateway would be set on a route only if the route is a default route.
    if (chosenRoute->gateway == 0)
    {
        // There is no gateway defined, so just flood on that interface.
        *interface = chosenRoute->interface;
        return arp_getMAC(destinationIP,chosenRoute->interface);
    }
    else
    {
        *interface = chosenRoute->interface;
        return arp_getMAC(chosenRoute->gateway,chosenRoute->interface);

    }
    return 0;

}

