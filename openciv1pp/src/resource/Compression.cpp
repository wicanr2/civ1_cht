#include "Compression.h"
#include <map>

namespace oc1 {

// ---------------- RLE ----------------
void rleCompress(const std::vector<uint8_t>& in, std::vector<uint8_t>& out, int minCount) {
    int oldValue = -1;
    int count = 0;
    if (minCount < 2) minCount = 2;

    auto flush = [&]() {
        if (count > minCount) {
            out.push_back(uint8_t(oldValue));
            if (oldValue == 0x90) out.push_back(0);
            out.push_back(0x90);
            out.push_back(uint8_t(count));
        } else {
            for (int i = 0; i < count; ++i) {
                out.push_back(uint8_t(oldValue));
                if (oldValue == 0x90) out.push_back(0);
            }
        }
    };

    for (uint8_t cb : in) {
        int c = cb;
        if (c == oldValue && count < 255) {
            ++count;
        } else {
            flush();
            oldValue = c;
            count = 1;
        }
    }
    flush();
}

void rleDecompress(const std::vector<uint8_t>& in, std::vector<uint8_t>& out) {
    uint8_t oldValue = 0;
    std::size_t i = 0;
    while (i < in.size()) {
        int c = in[i++];
        if (c == 0x90) {
            if (i >= in.size()) break;
            int len = in[i++];
            if (len == 0) {
                oldValue = uint8_t(c);
                out.push_back(oldValue);
            } else {
                for (int k = 1; k < len; ++k) out.push_back(oldValue);
            }
        } else {
            oldValue = uint8_t(c);
            out.push_back(oldValue);
        }
    }
}

// ---------------- LZW ----------------
void lzwDecompress(const std::vector<uint8_t>& in, std::vector<uint8_t>& out, int startBits, int maxBits) {
    int inputData = 0, inputLen = 0;
    int bitCount = startBits;
    int bitMask = (1 << bitCount) - 1;
    std::size_t pos = 0;

    std::vector<std::vector<uint8_t>> dict;
    auto initDict = [&]() {
        dict.clear();
        dict.reserve(1 << maxBits);
        for (int i = 0; i < 256; ++i) dict.push_back({uint8_t(i)});
    };
    initDict();

    std::vector<uint8_t> w;
    w.push_back(0); // ignore position 256

    for (;;) {
        while (inputLen < bitCount) {
            if (pos >= in.size()) break;
            inputData |= int(in[pos++]) << inputLen;
            inputLen += 8;
        }
        if (inputLen < bitCount) break;

        int code = inputData & bitMask;
        if (code == 256) break; // reserved (C# throws); stop cleanly
        inputLen -= bitCount;
        inputData >>= bitCount;

        std::vector<uint8_t> entry;
        if (code < int(dict.size())) {
            entry = dict[code];
        } else if (code == int(dict.size())) {
            entry = w;
            if (!w.empty()) entry.push_back(w[0]);
        }

        for (uint8_t b : entry) out.push_back(b);

        if (!entry.empty()) w.push_back(entry[0]);

        if (int(dict.size()) >= bitMask) {
            ++bitCount;
            bitMask = (1 << bitCount) - 1;
        }

        if (bitCount > maxBits) {
            bitCount = startBits;
            bitMask = (1 << bitCount) - 1;
            initDict();
            w.clear();
            w.push_back(0);
        } else {
            if (!w.empty()) dict.push_back(w);
            w = entry;
        }
    }
}

void lzwCompress(const std::vector<uint8_t>& in, std::vector<uint8_t>& out, int startBits, int maxBits) {
    int bitCount = startBits;
    int bitMask = (1 << bitCount) - 1;
    int outputData = 0, outputLen = 0;

    std::map<std::vector<uint8_t>, int> dict;
    auto initDict = [&]() {
        dict.clear();
        for (int i = 0; i < 256; ++i) dict[{uint8_t(i)}] = int(dict.size());
        dict[std::vector<uint8_t>{}] = int(dict.size()); // ignore position 256
    };
    initDict();

    out.push_back(uint8_t(maxBits)); // first output byte = max bit count

    std::vector<uint8_t> w, wc;
    std::size_t pos = 0;
    auto emit = [&](int code) {
        outputData |= code << outputLen;
        outputLen += bitCount;
        while (outputLen >= 8) {
            out.push_back(uint8_t(outputData & 0xFF));
            outputData >>= 8;
            outputLen -= 8;
        }
    };

    while (pos < in.size()) {
        int c = in[pos++];
        wc = w;
        wc.push_back(uint8_t(c));

        if (dict.find(wc) != dict.end()) {
            w = wc;
        } else {
            emit(dict.at(w));

            if (int(dict.size()) > bitMask) {
                ++bitCount;
                bitMask = (1 << bitCount) - 1;
                w.assign(1, uint8_t(c));
            }

            if (bitCount > maxBits) {
                bitCount = startBits;
                bitMask = (1 << bitCount) - 1;
                initDict();
                --pos;          // reread current byte
                w.clear();
            } else {
                dict[wc] = int(dict.size());
                w.assign(1, uint8_t(c));
            }
        }
    }

    if (!w.empty()) {
        emit(dict.at(w));
        if (outputLen > 0) out.push_back(uint8_t(outputData & 0xFF));
    }
}

} // namespace oc1
