#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), _capacity(capacity) {}

long StreamReassembler::merge_segment(_segment &_seg_1, const _segment &_seg_2) {
    _segment lower_segment, upper_segment;
    if (_seg_1.begin < _seg_2.begin) {
        lower_segment = _seg_1;
        upper_segment = _seg_2;
    } else {
        lower_segment = _seg_2;
        upper_segment = _seg_1;
    }

    if (lower_segment.begin + lower_segment.length < upper_segment.begin) {  // can't merge to one segment
        return -1;
    } else if (lower_segment.begin + lower_segment.length >=
               upper_segment.begin + upper_segment.length) {  // the upper segment within the lower segment
        _seg_1 = lower_segment;
        return upper_segment.length;

    } else {  // the lower segment part within the upper
        _seg_1.begin = lower_segment.begin;
        _seg_1._data = lower_segment._data +
                       upper_segment._data.substr(lower_segment.begin + lower_segment.length - upper_segment.begin);
        _seg_1.length = _seg_1._data.length();
        return lower_segment.begin + lower_segment.length - upper_segment.begin;
    }
    return -1;
}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    if (index >= _capacity + _cur_index)  // if the index is out of range, data that arrive early
        return;

    _segment temp;
    bool set_eof_flag = false;
    if (index + data.size() <= _cur_index) {
        set_eof_flag = true;
    } else if (index < _cur_index) {
        temp._data.assign(data.begin() + _cur_index - index, data.end());
        temp.begin = _cur_index;
        temp.length = temp._data.length();
    } else {
        temp._data = data;
        temp.begin = index;
        temp.length = data.length();
    }

    // std::function<long(_segment, _segment)> merge_segment = [&](_segment _seg_1,
    //                                                             _segment _seg_2) -> long {  // merge _seg_2 to
    //                                                             _seg_1;
    // };

    auto it = _segment_set.lower_bound(temp);
    long to_merge_bytes = 0;

    if (set_eof_flag) {
        goto SET_EOF;
    }
    _unassembled_bytes += temp.length;

    do {
        if (true) {  // merge segments that has bigger index than temp
            while (it != _segment_set.end() && (to_merge_bytes = merge_segment(temp, *it)) >= 0) {
                _unassembled_bytes -= to_merge_bytes;
                _segment_set.erase(it);
                it = _segment_set.lower_bound(temp);
            }
        }
        if (it == _segment_set.begin()) {  //   if the first segment is the same as temp, break
            goto INSERT;
        }
        it--;
        if (true) {  // merge segments that has smaller index than temp
            while ((to_merge_bytes = merge_segment(temp, *it)) >= 0) {
                _unassembled_bytes -= to_merge_bytes;
                _segment_set.erase(it);
                it = _segment_set.lower_bound(temp);
                if (it == _segment_set.begin())
                    break;
                it--;
            }
        }
    } while (false);
INSERT:
    _segment_set.insert(temp);

    // write to the output stream
    if (!_segment_set.empty() && _segment_set.begin()->begin == _cur_index) {
        const _segment seg = *_segment_set.begin();
        size_t write_bytes = _output.write(seg._data);
        _cur_index += write_bytes;
        _unassembled_bytes -= write_bytes;
        _segment_set.erase(_segment_set.begin());
    }

SET_EOF:
    if (eof) {
        _is_eof = true;
        _end_pos_index = index + data.size();
    }

    if (_is_eof && empty()) {
        _output.end_input();
    }
    return;
}

size_t StreamReassembler::unassembled_bytes() const { return _unassembled_bytes; }

bool StreamReassembler::empty() const { return _unassembled_bytes == 0; }
