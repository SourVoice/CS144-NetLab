#include "byte_stream.hh"

#include <algorithm>
#include <sstream>
#include <stdexcept>
// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity) : _capacity(capacity) {}

size_t ByteStream::write(const string &data) {
    size_t length = data.size();
    if (length > _capacity - _buffer.size()) {
        length = _capacity - _buffer.size();
    }
    string new_s;
    new_s.assign(data.begin(), data.begin() + length);
    _buffer.append(BufferList(move(new_s)));
    _write_cnt += length;
    return length;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    size_t length = len;
    if (length > _buffer.size()) {
        // throw std::out_of_range("peek_output: len > _buffer.size()");
        length = _buffer.size();
    }
    string s;
    s = _buffer.concatenate();
    return string().assign(s.begin(),
                           s.begin() + length);  //  rewrite here, because assign will replacing its current contents
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    size_t length = len;
    if (length > _buffer.size()) {
        // throw std::out_of_range("pop_output: len > _buffer.size()");
        length = _buffer.size();
    }
    _buffer.remove_prefix(length);
    _read_cnt += length;
    return;
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    string s;
    s = peek_output(len);
    pop_output(len);
    return s;
}

void ByteStream::end_input() {
    _end = true;
    return;
}

bool ByteStream::input_ended() const { return _end; }

size_t ByteStream::buffer_size() const { return _buffer.size(); }

bool ByteStream::buffer_empty() const { return _buffer.size() == 0; }

bool ByteStream::eof() const { return input_ended() && buffer_empty(); }

size_t ByteStream::bytes_written() const { return _write_cnt; }

size_t ByteStream::bytes_read() const { return _read_cnt; }

size_t ByteStream::remaining_capacity() const { return _capacity - _buffer.size(); }
