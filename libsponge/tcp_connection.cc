#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return _time_since_last_segment_received; }

void TCPConnection::segment_received(const TCPSegment &seg) {
    // 1. return if not active
    if (!active())
        return;
    _time_since_last_segment_received = 0;

    // 2. when in the initial state(closed state)
    if (_sender.next_seqno_absolute() == 0 && !_receiver.ackno().has_value()) {
        if (!seg.header().syn)
            return;
        _receiver.segment_received(seg);
        connect();
        return;
    }

    if (is_in_syn_sent_state()) {
        // 2. if in the syn_sent state, but the seg contains ack and contents
        if (seg.header().ack && seg.payload().size() > 0)
            return;
        if (!seg.header().ack && seg.header().syn) {
            _sender.send_empty_segment();
            _receiver.segment_received(seg);
            return;
        }
        if (seg.header().rst) {
            _need_to_send_rst = false;
            unclean_shutdown();
        }
    }

    // 4. check legal ackno, if the ack flag is set, tells the TCPSender about the fields it cares(ackno and win)
    _receiver.segment_received(seg);
    WrappingInt32 _ackno = seg.header().ackno;
    uint16_t _window_size = seg.header().win;
    _sender.ack_received(_ackno, _window_size);

    // exception:
    // receive a rst flag, then we just set both the inbound and outbound streams to the error
    // send empty segment to test if the connection is still on (when the seqno is invalid)
    if (_sender.stream_in().buffer_empty() && seg.length_in_sequence_space()) {
        _sender.send_empty_segment();
    }
    if (seg.header().rst) {
        _sender.send_empty_segment();
        unclean_shutdown();
        return;
    }

    // when solve all the situations we just send all the segment to the sender
    send_segment_out();
    return;
}

bool TCPConnection::active() const { return _active; }

size_t TCPConnection::write(const string &data) {
    // the number of bytes from `data` that were actually written
    size_t have_written_size = _sender.stream_in().write(data);
    _sender.fill_window();
    send_segment_out();
    return have_written_size;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    if (!_active)
        return;
    _time_since_last_segment_received += ms_since_last_tick;

    // tell the sender about the passage of thme
    _sender.tick(ms_since_last_tick);

    // the number of consecutive retransmissions is more than an upper limit TCPConfig::MAX_RETX_ATTEMPTS
    if (_sender.consecutive_retransmissions() > _cfg.MAX_RETX_ATTEMPTS) {
        _need_to_send_rst = true;
        unclean_shutdown();
    }
    send_segment_out();
    return;
}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    _sender.fill_window();
    send_segment_out();
    return;
}

void TCPConnection::connect() {
    // when construct a conection, we send a segment with syn
    _sender.fill_window();
    _active = true;
    send_segment_out();
}

void TCPConnection::unclean_shutdown() {
    inbound_stream().set_error();
    _sender.stream_in().set_error();

    _active = false;
    if (_need_to_send_rst) {
        _need_to_send_rst = false;
        TCPSegment seg = _sender.segments_out().front();
        _sender.segments_out().pop();
        if (_receiver.ackno().has_value()) {
            seg.header().ackno = _receiver.ackno().value();
        }
        seg.header().rst = true;
        seg.header().ack = true;
        seg.header().win = _receiver.window_size();
        segments_out().push(seg);
    }
    return;
}

void TCPConnection::clean_shutdown() {
    // inbound_stream ends before outbound_stream reached EOF
    // cerr << "_receiver is end? :" << _receiver.stream_out().input_ended() << endl;
    if (_receiver.stream_out().input_ended()) {
        if (!_sender.stream_in().eof())
            _linger_after_streams_finish = false;
        else if (bytes_in_flight() == 0) {
            // Prereq#1: The inbound_stream has been fully assembled and has ended
            // Prereq#3: The outbound_stream has been fully acknoledged by the reomte peer
            if (_linger_after_streams_finish == false || time_since_last_segment_received() >= 10 * _cfg.rt_timeout) {
                _active = false;
            }
        }
    }
}

void TCPConnection::send_segment_out() {
    TCPSegment _newSeg;

    // the incoming segment occupied any sequence nums, we reflect an update in the ackno and window_size
    while (!_sender.segments_out().empty()) {
        _newSeg = _sender.segments_out().front();
        _sender.segments_out().pop();

        if (_receiver.ackno().has_value()) {
            _newSeg.header().ack = true;
            _newSeg.header().ackno = _receiver.ackno().value();
            _newSeg.header().win = _receiver.window_size();
        }

        segments_out().push(_newSeg);
    }

    clean_shutdown();
    return;
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
            _need_to_send_rst = true;
            _sender.send_empty_segment();
            unclean_shutdown();
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}

bool TCPConnection::is_in_listen_state() { return !_receiver.ackno() && !_sender.next_seqno_absolute(); }

bool TCPConnection::is_in_syn_sent_state() {
    return _sender.next_seqno_absolute() && bytes_in_flight() == _sender.next_seqno_absolute() &&
           !_receiver.ackno().has_value();
}

bool TCPConnection::is_in_syn_receive_state() {
    return _receiver.ackno().has_value() && !_receiver.stream_out().input_ended();
}
