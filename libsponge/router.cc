#include "router.hh"

#include <iostream>

using namespace std;

// Dummy implementation of an IP router

// Given an incoming Internet datagram, the router decides
// (1) which interface to send it out on, and
// (2) what next hop address to send it to.

// For Lab 6, please replace with a real implementation that passes the
// automated checks run by `make check_lab6`.

// You will need to add private members to the class declaration in `router.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

//! \param[in] route_prefix The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
//! \param[in] prefix_length For this route to be applicable, how many high-order (most-significant) bits of the route_prefix will need to match the corresponding bits of the datagram's destination address?
//! \param[in] next_hop The IP address of the next hop. Will be empty if the network is directly attached to the router (in which case, the next hop address should be the datagram's final destination).
//! \param[in] interface_num The index of the interface to send the datagram out on.
void Router::add_route(const uint32_t route_prefix,
                       const uint8_t prefix_length,
                       const optional<Address> next_hop,
                       const size_t interface_num) {
    cerr << "DEBUG: adding route " << Address::from_ipv4_numeric(route_prefix).ip() << "/" << int(prefix_length)
         << " => " << (next_hop.has_value() ? next_hop->ip() : "(direct)") << " on interface " << interface_num << "\n";

    // Your code here.
    _route_table.push_back({route_prefix, prefix_length, next_hop, interface_num});
    DUMMY_CODE(route_prefix, prefix_length, next_hop, interface_num);
}

//! \param[in] dgram The datagram to be routed
void Router::route_one_datagram(InternetDatagram &dgram) {
    // Your code here.
    int forward_interface = -1;
    uint32_t dst_ip = dgram.header().dst;
    uint8_t longest = 0;
    for (auto &route : _route_table) {
        if (route.prefix_length >= longest) {
            uint32_t mask =
                route.prefix_length == 0 ? 0 : (0xffffffff << (32 - route.prefix_length));
            if ((dst_ip & mask) == (route.route_prefix & mask)) {
                longest = route.prefix_length;
                forward_interface = route.interface_num;
            }
        }
    }

    // If the TTL was zero already, or hits zero after the decrement, the router should drop the datagram
    if (forward_interface == -1 || dgram.header().ttl <= 1)
        return;
    dgram.header().ttl--;
    // If the router is directly attached to the network in question, the next hop will be an empty optional.  In
    // that case, the next hop is the datagram¡¯s destination address.
    // But if the router is
    // connected to the network in question through some other router, the next hop will
    // contain the IP address of the next router along the path.
    if (_route_table[forward_interface].next_hop.has_value())
        interface(forward_interface).send_datagram(dgram, _route_table[forward_interface].next_hop.value());
    else
        interface(forward_interface).send_datagram(dgram, Address::from_ipv4_numeric(dgram.header().dst));
    DUMMY_CODE(dgram);
}

void Router::route() {
    // Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
    for (auto &interface : _interfaces) {
        auto &queue = interface.datagrams_out();
        while (not queue.empty()) {
            route_one_datagram(queue.front());
            queue.pop();
        }
    }
}
