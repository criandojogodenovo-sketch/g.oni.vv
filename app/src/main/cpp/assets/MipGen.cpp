// assets/MipGen.cpp — cadeia de mips em CPU (F5.1-A). GL-free.
#include "assets/MipGen.h"

namespace vv {

void halveImageRGBA(const RawImage& in, RawImage& out) {
    out = RawImage{};
    if (!in.ok()) {
        return;
    }
    const u32 nw = in.width > 1 ? in.width / 2 : 1;
    const u32 nh = in.height > 1 ? in.height / 2 : 1;
    out.width = nw;
    out.height = nh;
    out.rgba.resize(static_cast<size_t>(nw) * nh * 4);
    for (u32 y = 0; y < nh; ++y) {
        for (u32 x = 0; x < nw; ++x) {
            const u32 x0 = x * 2;
            const u32 y0 = y * 2;
            const u32 x1 = (x0 + 1 < in.width) ? x0 + 1 : x0;
            const u32 y1 = (y0 + 1 < in.height) ? y0 + 1 : y0;
            for (int c = 0; c < 4; ++c) {
                const u32 sum =
                    in.rgba[(static_cast<size_t>(y0) * in.width + x0) * 4 + c] +
                    in.rgba[(static_cast<size_t>(y0) * in.width + x1) * 4 + c] +
                    in.rgba[(static_cast<size_t>(y1) * in.width + x0) * 4 + c] +
                    in.rgba[(static_cast<size_t>(y1) * in.width + x1) * 4 + c];
                out.rgba[(static_cast<size_t>(y) * nw + x) * 4 + c] =
                    static_cast<u8>(sum / 4u);
            }
        }
    }
}

void genMipChainRGBA(const RawImage& img, std::vector<RawImage>& chain) {
    chain.clear();
    if (!img.ok()) {
        return;
    }
    chain.push_back(img);
    while (chain.back().width > 1 || chain.back().height > 1) {
        RawImage next;
        halveImageRGBA(chain.back(), next);
        chain.push_back(std::move(next));
    }
}

} // namespace vv
