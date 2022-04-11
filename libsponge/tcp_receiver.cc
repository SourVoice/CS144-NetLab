#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    // size_t seg_length = seg.length_in_sequence_space();
    TCPHeader _header = seg.header();
	WrappingInt32 payload_first_seqno = _header.seqno;

    // by pass the syn hasn't arrived;
    if (_SYN_received == false && _header.syn == true) {
        _SYN_received = true;
        _isn = _header.seqno;
    } else if (_SYN_received == true && _header.syn == true) {
        return;
    }

    if(_header.syn) {
        _SYN_received = true;
        _isn = _header.seqno;
        payload_first_seqno = _header.seqno + 1;
    }
    // set FIN_received true
    if (_SYN_received == true && _header.fin) {
        _FIN_received = true;
    }

    string data = seg.payload().copy();
    uint64_t checkpoint = TCPReceiver::stream_out().bytes_written();
    uint64_t absolute_seqno = unwrap(payload_first_seqno, _isn, checkpoint);
    if (absolute_seqno == 0) {
        return;
    }
    uint64_t index = unwrap(payload_first_seqno, _isn, checkpoint) - 1;

    _reassembler.push_substring(data, index, _header.fin);

    return;
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if (!_SYN_received)
        return std::nullopt;

    uint64_t n = stream_out().bytes_written();

    // fin segment also contain a seqno and paylaod
    if (stream_out().input_ended()) {
        n = n + 1;
    }

    return wrap(n + 1, _isn);
}

size_t TCPReceiver::window_size() const { return _reassembler.stream_out().remaining_capacity(); }
