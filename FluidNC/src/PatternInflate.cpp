// Copyright (c) 2026 Dune Weaver Mini fork contributors
// Use of this source code is governed by a GPLv3 license that can be found in the LICENSE file.

#include "PatternInflate.h"
#include "FileStream.h"
#include "Config.h"  // log_*

#include <cstdlib>
#include <new>

#include "esp32/rom/miniz.h"  // tinfl_decompress (in ROM)

bool PatternInflate::isZlibHeader(uint8_t cmf, uint8_t flg, uint32_t& window) {
    if ((cmf & 0x0F) != 8) {  // CM: deflate
        return false;
    }
    uint32_t cinfo = cmf >> 4;
    if (cinfo > 7) {
        return false;
    }
    if (((uint32_t(cmf) << 8) | flg) % 31 != 0) {  // FCHECK
        return false;
    }
    if (flg & 0x20) {  // FDICT: preset dictionaries are never used for patterns
        return false;
    }
    window = 1u << (cinfo + 8);
    return true;
}

bool PatternInflate::sniff(FileStream& src, uint32_t& window) {
    size_t  pos = src.position();
    uint8_t hdr[2];
    size_t  n = src.read(hdr, 2);
    src.set_position(pos);
    return n == 2 && isZlibHeader(hdr[0], hdr[1], window);
}

PatternInflate::PatternInflate(uint32_t window) : _ringSize(window) {
    if (window == 0 || window > kMaxWindow || (window & (window - 1))) {
        log_error("Pattern: compression window " << window << " not supported (max " << kMaxWindow << ")");
        return;
    }
    _d    = malloc(sizeof(tinfl_decompressor));
    _ring = static_cast<uint8_t*>(malloc(_ringSize));
    if (!_d || !_ring) {
        log_error("Pattern: not enough memory to decompress");
        free(_d);
        free(_ring);
        _d    = nullptr;
        _ring = nullptr;
        return;
    }
    tinfl_init(static_cast<tinfl_decompressor*>(_d));
}

PatternInflate::~PatternInflate() {
    free(_d);
    free(_ring);
}

int PatternInflate::get(FileStream& src) {
    while (_avail == 0) {
        if (_done || _err || !ok()) {
            return -1;
        }
        if (_inPos == _inLen && !_inEof) {
            _inLen = src.read(_in, sizeof(_in));
            _inPos = 0;
            if (_inLen == 0) {
                _inEof = true;
            }
        }
        size_t    inBytes  = _inLen - _inPos;
        size_t    outBytes = _ringSize - _outOfs;
        mz_uint32 flags    = TINFL_FLAG_PARSE_ZLIB_HEADER | (_inEof ? 0 : TINFL_FLAG_HAS_MORE_INPUT);
        // Wrapping-buffer mode: (next - start) + size is the ring size, a power
        // of two no smaller than the stream's window (tinfl checks the header's
        // window against it and fails rather than reading stale history).
        tinfl_status st = tinfl_decompress(
            static_cast<tinfl_decompressor*>(_d), _in + _inPos, &inBytes, _ring, _ring + _outOfs, &outBytes, flags);
        _inPos += inBytes;
        _readPos = _outOfs;
        _avail   = outBytes;
        _outOfs  = (_outOfs + outBytes) & (_ringSize - 1);

        if (st == TINFL_STATUS_DONE) {
            _done = true;  // bytes produced by this call are still handed out
        } else if (st < 0) {
            _err = true;
            log_error("Pattern: decompression failed (" << int(st) << ")");
        } else if (st == TINFL_STATUS_NEEDS_MORE_INPUT && _inEof && outBytes == 0) {
            _err = true;
            log_error("Pattern: compressed file is truncated");
        }
    }
    --_avail;
    return _ring[_readPos++];
}
