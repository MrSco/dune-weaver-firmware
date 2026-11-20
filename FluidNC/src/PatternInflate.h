// Copyright (c) 2026 Dune Weaver Mini fork contributors
// Use of this source code is governed by a GPLv3 license that can be found in the LICENSE file.

#pragma once

// DW Mini fork: transparent decompression of zlib-compressed pattern files.
//
// The Mini caches patterns in ~1.9 MB of flash, so the host uploads them
// zlib-compressed (deflate, 4 KB window: ~4.1x smaller on a real library).
// Files keep their .thr name; a file is treated as compressed when it starts
// with a valid zlib header.  Plain text patterns cannot match one: the header
// check (CM=8, FCHECK, no preset dictionary) rejects every two-byte prefix a
// .thr or G-code line can start with.
//
// Decompression uses the inflater in the ESP32 ROM (tinfl), with a ring
// buffer sized to the stream's declared window, capped at kMaxWindow to keep
// the heap cost near 15 KB.

#include <cstddef>
#include <cstdint>

class FileStream;

class PatternInflate {
public:
    static constexpr uint32_t kMaxWindow = 8192;

    // True if the two bytes are a zlib header; window gets the declared
    // LZ77 window size in bytes.
    static bool isZlibHeader(uint8_t cmf, uint8_t flg, uint32_t& window);

    // True if the stream's next two bytes are a zlib header.  The stream
    // position is left where it was.
    static bool sniff(FileStream& src, uint32_t& window);

    explicit PatternInflate(uint32_t window);
    ~PatternInflate();
    PatternInflate(const PatternInflate&)            = delete;
    PatternInflate& operator=(const PatternInflate&) = delete;

    // False if the buffers could not be allocated (or window too large).
    bool ok() const { return _d && _ring; }

    // Next decompressed byte read from src, or -1 at the end of the stream or
    // on an error (failed() says which).
    int get(FileStream& src);

    bool failed() const { return _err; }

private:
    void*    _d    = nullptr;  // tinfl_decompressor (opaque: ROM header stays in the .cpp)
    uint8_t* _ring = nullptr;
    uint32_t _ringSize;
    uint32_t _outOfs  = 0;  // where the inflater writes next
    uint32_t _readPos = 0;  // next decompressed byte to hand out
    uint32_t _avail   = 0;  // decompressed bytes not yet handed out

    uint8_t _in[512];
    size_t  _inPos = 0;
    size_t  _inLen = 0;
    bool    _inEof = false;
    bool    _done  = false;
    bool    _err   = false;
};
