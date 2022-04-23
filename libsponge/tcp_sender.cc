#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <algorithm>  // must have this include
#include <random>
#include <string>
// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity)
    , _retransmission_timeout(
          retx_timeout)  // must initialize _retransmission_timeout to retx_timeout when constructor is called
{}

uint64_t TCPSender::bytes_in_flight() const { return _bytes_been_send; }

void TCPSender::fill_window() {
    if (_syn_send == false) {
        _syn_send = true;
        TCPSegment seg;
        seg.header().syn = true;
        AddSegmentToQueue(seg);
        return;
    }

    // when the window_size of the receiver is 0, we just treat it as 1
    size_t win_size = _window_size ? _window_size : 1;

    size_t remain;  // available  in window_size
    while ((remain = win_size - (_next_seqno - _ackno_receive)) != 0) {
        // the window must always update

        if (remain == 0 || _fin_send == true) {
            break;
        }

        // add new segment to _segments_out
        TCPSegment newSeg;
        string data = _stream.read(min(TCPConfig::MAX_PAYLOAD_SIZE, remain));  // my read function is rewrite here
        newSeg.payload() = Buffer(std::move(data));
        if (_stream.eof() && newSeg.length_in_sequence_space() < win_size) {
            _fin_send = true;
            newSeg.header().fin = true;
        }

        if (newSeg.length_in_sequence_space() == 0) {
            return;
        }
        AddSegmentToQueue(newSeg);
    }
    return;
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    size_t abs_ackno = unwrap(ackno, _isn, _ackno_receive);

    _window_size = window_size;
    // ilegal abs_ackno, (what the receiver want is too ahead of the sender can send)
    if (!ackLegal(abs_ackno)) {
        return;
    }

    _base_seqno = _ackno_receive;

    // update what the sender has acknowledged last(the left of the sender window)
    _ackno_receive = abs_ackno;

    // remove all the segment before abs_ackno(when the ackno_receive is legal, means all segment before it has been
    // receive by the sender)
    while (!_segments_outstanding.empty()) {
        TCPSegment seg_in_front = _segments_outstanding.front();
        size_t seqno_front = unwrap(seg_in_front.header().seqno, _isn, _next_seqno);
        if (seqno_front + seg_in_front.length_in_sequence_space() <= abs_ackno) {
            _bytes_been_send -= seg_in_front.length_in_sequence_space();

            _segments_outstanding.pop();

        } else {
            break;
        }
    }
    // update base
    _base_seqno = _ackno_receive;

    // after remove all the segment which has been received by the receiver, go on fill the _segments_out(which means
    // the receiver's window)

    fill_window();

    //重传机制

    // receive a legal abs_ackno, then set RTO to "initial", also the consecutive retransmissions
    _retransmission_timeout = _initial_retransmission_timeout;
    _consecutive_restransmissions = 0;

    if (!_segments_outstanding.empty()) {
        _time_start = true;
        _time_split = 0;
    }
    return;
}

void TCPSender::AddSegmentToQueue(TCPSegment &newSeg) {
    newSeg.header().seqno = wrap(_next_seqno, _isn);

    _bytes_been_send += newSeg.length_in_sequence_space();
    _next_seqno += newSeg.length_in_sequence_space();

    _segments_outstanding.push(newSeg);
    segments_out().push(newSeg);

    // after each segment was send, we start a timer
    StartTimer();
    return;
}
bool TCPSender::ackLegal(uint64_t ackno) {
    if (ackno <= _next_seqno && ackno > unwrap(_segments_outstanding.front().header().seqno, _isn, _next_seqno)) {
        return true;
    }
    return false;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    _time_split += ms_since_last_tick;
    if (_time_split >= _retransmission_timeout && !_segments_outstanding.empty()) {
        segments_out().push(_segments_outstanding.front());

        // when window_size is 0, we don't update rto, but except the first segment
        //注意，窗口大小为 0 时不需要增加 RTO。但是发送 SYN 时，窗口为初始值也为 0，而 SYN 超时是需要增加 RTO 的。
        if (_window_size > 0 || _segments_outstanding.front().header().syn) {
            _retransmission_timeout *= 2;

            _consecutive_restransmissions++;
        }
        _time_start = true;
        _time_split = 0;
    }

    // when all outstanding data has been acknowledged, stop the timer
    if (_segments_outstanding.empty()) {
        _time_start = false;
    }
    return;
}

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_restransmissions; }
void TCPSender::StartTimer() {
    if (!_time_start) {
        _time_split = 0;

        _time_start = true;
    }
    return;
}

void TCPSender::send_empty_segment() {
    TCPSegment emptySeg;
    emptySeg.header().seqno = wrap(_next_seqno, _isn);
    segments_out().push(emptySeg);
}
