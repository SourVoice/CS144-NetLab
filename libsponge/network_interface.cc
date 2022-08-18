#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

#include <iostream>

// Dummy implementation of a network interface
// Translates from {IP datagram, next hop address} to link-layer frame, and from link-layer frame to IP datagram

// For Lab 5, please replace with a real implementation that passes the
// automated checks run by `make check_lab5`.

// You will need to add private members to the class declaration in `network_interface.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}
using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address)
    : _ethernet_address(ethernet_address), _ip_address(ip_address) {
    cerr << "DEBUG: Network interface has Ethernet address " << to_string(_ethernet_address) << " and IP address "
         << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be another host if directly connected to the same network as the destination)
//! (Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric() method.)
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
    // convert IP address of next hop to raw 32-bit representation (used in ARP header)
    //    const uint32_t next_hop_ip = next_hop.ipv4_numeric();

    auto it = _arp_table.find(next_hop);

    // if next hop is in arp table, send frame with arp table entry
    if (it != _arp_table.end()) {
        // initialize Ethernet header for frame
        EthernetHeader dgram_header;
        dgram_header.src = _ethernet_address;
        dgram_header.dst = it->second.first;
        dgram_header.type = EthernetHeader::TYPE_IPv4;

        EthernetFrame frame;
        frame.header() = dgram_header;
        frame.payload() = dgram.serialize();
        _frames_out.push(frame);
    } else {
        // 不在arp表中, 则将数据放入arp缓存, 并且进行广播
        _dgram_cache.push_back(std::make_pair(dgram, next_hop));
        // 如果已经广播过, 则不再进行广播, 否则进行广播
        if (_arp_request_cache.find(next_hop) == _arp_request_cache.end()) {
            EthernetFrame frame = make_broadcast_frame(next_hop);
            _frames_out.push(frame);
            _arp_request_cache[next_hop] = _time;  //记录请求发出的时间
        }
    }

    DUMMY_CODE(dgram, next_hop);
}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    EthernetHeader header = frame.header();
    /*
    ignore any frames not destined for the network interface (meaning, the Ethernet
    destination is either the broadcast address or the interface’s own Ethernet address
    stored in the ethernet address member variable).
    */
    if (header.dst != _ethernet_address && header.dst != ETHERNET_BROADCAST) {
        return nullopt;
    }

    // If the inbound frame is IPv4
    if (header.type == header.TYPE_IPv4) {
        InternetDatagram dgram;
        ParseResult result = dgram.parse(frame.payload());
        if (result == ParseResult::NoError) {
            return dgram;
        } else {
            return nullopt;
        }
    }
    // If the inbound frame is ARP
    else if (header.type == header.TYPE_ARP) {
        ARPMessage arp_msg;
        ParseResult result = arp_msg.parse(frame.payload());
        /*
        if successful, remember the mapping between the sender’s IP address
        and Ethernet address for 30 seconds.
        */
        if (result == ParseResult::NoError) {
            if (arp_msg.opcode == ARPMessage::OPCODE_REQUEST &&
                arp_msg.target_ip_address == _ip_address.ipv4_numeric()) {
                ARPMessage arp_reply;
                arp_reply.opcode = ARPMessage::OPCODE_REPLY;
                arp_reply.sender_ethernet_address = _ethernet_address;
                arp_reply.target_ethernet_address = arp_msg.sender_ethernet_address;
                arp_reply.sender_ip_address = _ip_address.ipv4_numeric();
                arp_reply.target_ip_address = arp_msg.sender_ip_address;

                EthernetHeader header_reply;
                header_reply.type = EthernetHeader::TYPE_ARP;
                header_reply.src = _ethernet_address;
                header_reply.dst = arp_msg.sender_ethernet_address;

                EthernetFrame frame_reply;
                frame_reply.header() = header_reply;
                frame_reply.payload() = arp_reply.serialize();
                _frames_out.push(frame_reply);
            }
        }
        // After send reply, update arp table, and send datagrams in arp cache
        _arp_table[Address::from_ipv4_numeric(arp_msg.sender_ip_address)] = {arp_msg.sender_ethernet_address, _time};
        for (auto it = _dgram_cache.begin(); it != _dgram_cache.end();) {
            if (it->second == Address::from_ipv4_numeric(arp_msg.sender_ip_address)) {
                send_datagram(it->first, it->second);
                _dgram_cache.erase(it++);
            } else {
                ++it;
            }
        }
        // get a valid arp reply, we remove it from arp request cache
        _arp_request_cache.erase(Address::from_ipv4_numeric(arp_msg.sender_ip_address));
    }
    return {};
    DUMMY_CODE(frame);
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) {
    _time += ms_since_last_tick;
    for (auto it = _arp_table.begin(); it != _arp_table.end();) {
        //删除arp表中的过期的项
        if (_time - it->second.second >= 30 * 1000) {
            _arp_table.erase(it++);
        } else {
            ++it;
        }
    }
    //更新arp请求缓存中的时间
    for (auto it = _arp_request_cache.begin(); it != _arp_request_cache.end(); it++) {
        // 超过五秒没有收到arp回复, 则重新发送arp请求, 并且更新arp请求缓存中的时间
        if (_time - it->second >= 5 * 1000) {
            EthernetFrame frame = make_broadcast_frame(it->first);
            _frames_out.push(frame);
            it->second = _time;
        }
    }

    DUMMY_CODE(ms_since_last_tick);
}

// ! param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be another host if directly connected to the same network as the destination)
//! This function would wrap the ARP request with destination in an Ethernet frame and send it out the interface.
EthernetFrame NetworkInterface::make_broadcast_frame(const Address &dst_ip) {
    ARPMessage arp_msg;
    arp_msg.opcode = ARPMessage::OPCODE_REQUEST;
    arp_msg.sender_ethernet_address = _ethernet_address;
    arp_msg.target_ethernet_address = {};
    arp_msg.sender_ip_address = _ip_address.ipv4_numeric();
    arp_msg.target_ip_address = dst_ip.ipv4_numeric();

    EthernetHeader eth_header;
    eth_header.src = _ethernet_address;
    eth_header.dst = ETHERNET_BROADCAST;
    eth_header.type = EthernetHeader::TYPE_ARP;

    // wrap to ethernet frame
    EthernetFrame frame;
    frame.header() = eth_header;
    frame.payload() = arp_msg.serialize();
    return frame;
}