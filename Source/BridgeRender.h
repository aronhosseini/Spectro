#pragma once
// Software renderer for the Spectro Meter Bridge (3D waterfall + 2D spectrogram).
// Pure standard C++ (no JUCE) so it can be unit-tested / previewed on its own.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace bridge
{
struct V3 { float x = 0, y = 0, z = 0; };
inline V3 operator+ (V3 a, V3 b) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
inline V3 operator- (V3 a, V3 b) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
inline V3 operator* (V3 a, float s) { return { a.x * s, a.y * s, a.z * s }; }
inline float dot (V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline V3 cross (V3 a, V3 b) { return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x }; }
inline V3 norm (V3 a) { const float l = std::sqrt (dot (a, a)); return l > 1e-9f ? a * (1.f / l) : a; }
inline float clamp01 (float v) { return v < 0.f ? 0.f : (v > 1.f ? 1.f : v); }
inline float lerp (float a, float b, float t) { return a + (b - a) * t; }

// ───────────── colour ramps (outputs 0..255)
inline void heatRGB (float v, float& r, float& g, float& b)
{
    // Ozone-style height ramp: dark blue / green valleys -> green -> yellow -> orange peaks
    static const float p[8] = { 0.f, .10f, .25f, .45f, .62f, .78f, .90f, 1.f };
    static const float c[8][3] = { { 8, 56, 72 }, { 8, 100, 86 }, { 14, 150, 64 }, { 36, 190, 36 }, { 175, 218, 22 }, { 255, 232, 15 }, { 255, 150, 0 }, { 255, 84, 0 } };
    v = clamp01 (v);
    for (int i = 1; i < 8; ++i)
        if (v <= p[i]) { const float t = (v - p[i - 1]) / (p[i] - p[i - 1]); r = lerp (c[i - 1][0], c[i][0], t); g = lerp (c[i - 1][1], c[i][1], t); b = lerp (c[i - 1][2], c[i][2], t); return; }
    r = c[7][0]; g = c[7][1]; b = c[7][2];
}

// preset 1 = Spectro heat, 2 = ice, 3 = mono
inline void rampRGB (int preset, float t, float& r, float& g, float& b)
{
    t = clamp01 (t);
    if (preset == 1) { heatRGB (t, r, g, b); return; }
    if (preset == 2)
    {
        if (t < 0.6f) { const float k = t / 0.6f; r = lerp (10, 90, k); g = lerp (30, 200, k); b = lerp (90, 255, k); }
        else          { const float k = (t - 0.6f) / 0.4f; r = lerp (90, 255, k); g = lerp (200, 255, k); b = 255.f; }
        return;
    }
    r = g = b = 255.f * std::pow (t, 0.9f);
}

// ───────────── data
struct Hist   // ring buffer of dB rows; newest row = head-1
{
    const float* data = nullptr; int rows = 0, cols = 0, head = 0;
    int ringRow (int age) const { return ((head - 1 - age) % rows + rows) % rows; }
    float at (int age, int col) const { return data[(size_t) ringRow (age) * (size_t) cols + (size_t) col]; }
};
struct Entry { Hist hist; float r = 255, g = 255, b = 255; bool grey = false; };   // r,g,b: tap colour 0..255

struct Image
{
    int w = 0, h = 0; std::vector<uint32_t> px;   // 0xAARRGGBB, opaque where drawn
    void reset (int W, int H) { w = W; h = H; px.assign ((size_t) W * (size_t) H, 0u); }
};

// Height curve shared by the surface and the dB ticks: lifts quiet / mid-level content so a normal
// mix fills the surface instead of hugging the floor of a -100..0 dB range.
inline float shapeV (float v) { return std::pow (clamp01 (v), 0.70f); }

struct Camera { float yaw = 0.f, pitch = 35.f, zoom = 1.f, panX = 0.f, panY = 0.f; };   // degrees; pan in pixels
struct Settings3D { int gridRows = 72, gridCols = 192; float heightScale = 1.f, lo = -100.f, hi = 0.f, depth = 1.6f; int preset = 0; };

inline uint32_t packRGB (float r, float g, float b)
{
    auto c = [] (float v) { return (uint32_t) (v < 0.f ? 0.f : (v > 255.f ? 255.f : v)); };
    return 0xff000000u | (c (r) << 16) | (c (g) << 8) | c (b);
}

// ───────────── 3D scene
class Scene3D
{
public:
    struct Proj { V3 pos, r, u, f; float focal = 1, cx = 0, cy = 0; int w = 0, h = 0; };

    Proj proj;
    float worldHeight = 0.75f, dispLo = -100.f, dispHi = 0.f;   // last render: surface height in world units, dB range
    std::vector<int> ids;    // per pixel: ((entry+1)<<20)|(row<<10)|col of the surface under it (0 = none) – used for hover read-out

    static Proj makeProj (const Camera& cam, int W, int H, float worldH, float depth)
    {
        Proj p; p.w = W; p.h = H;
        const float yaw = cam.yaw * 0.0174533f, pit = cam.pitch * 0.0174533f;
        const V3 target { 0.f, worldH * 0.35f, depth * 0.42f };
        const float dist = 3.6f / std::max (0.2f, cam.zoom);
        p.pos = target + V3 { std::sin (yaw) * std::cos (pit), std::sin (pit), -std::cos (yaw) * std::cos (pit) } * dist;
        p.f = norm (target - p.pos);
        p.r = norm (cross (V3 { 0, 1, 0 }, p.f));
        p.u = cross (p.f, p.r);
        p.focal = 0.5f * (float) H / std::tan (17.f * 0.0174533f);
        p.cx = 0.5f * (float) W + cam.panX; p.cy = 0.45f * (float) H + cam.panY;
        return p;
    }
    static bool project (const Proj& p, V3 q, float& sx, float& sy, float& iw)
    {
        const V3 d = q - p.pos; const float depth = dot (d, p.f);
        if (depth < 0.05f) return false;
        iw = 1.f / depth; sx = p.cx + dot (d, p.r) * iw * p.focal; sy = p.cy - dot (d, p.u) * iw * p.focal; return true;
    }

    void render (Image& img, const Camera& cam, const std::vector<Entry>& entries, const Settings3D& s)
    {
        W = img.w; H = img.h;
        std::fill (img.px.begin(), img.px.end(), 0xff050709u);   // fully opaque canvas colour, no transparent pixels
        zbuf.assign ((size_t) W * (size_t) H, 0.f);
        ids.assign ((size_t) W * (size_t) H, 0);
        const float worldH = 0.78f * s.heightScale;
        worldHeight = worldH; dispLo = s.lo; dispHi = s.hi;
        proj = makeProj (cam, W, H, worldH, s.depth);
        pix = img.px.data();

        const int nR = std::max (3, s.gridRows), nC = std::max (3, s.gridCols);
        const size_t n = (size_t) nR * (size_t) nC;
        for (auto* v : { &hv, &sx, &sy, &siw, &cr, &cg, &cb }) v->assign (n, 0.f);
        validV.assign (n, 1);

        for (size_t ei = 0; ei < entries.size(); ++ei)
        {
            const Entry& e = entries[ei];
            if (e.hist.rows < 2 || e.hist.cols < 2 || e.hist.data == nullptr) continue;
            const bool tapColours = (s.preset == 0);

            // heights
            for (int r = 0; r < nR; ++r)
            {
                const int age = (int) std::lround ((float) r / (float) (nR - 1) * (float) (e.hist.rows - 1));
                for (int c = 0; c < nC; ++c)
                {
                    const float fc = (float) c / (float) (nC - 1) * (float) (e.hist.cols - 1);
                    const int c0 = (int) fc, c1 = std::min (c0 + 1, e.hist.cols - 1); const float fr = fc - (float) c0;
                    float db = 0.f; int cnt = 0;
                    for (int a = std::max (0, age - 1); a <= std::min (e.hist.rows - 1, age + 1); ++a)
                    { db += lerp (e.hist.at (a, c0), e.hist.at (a, c1), fr); ++cnt; }
                    db /= (float) cnt;
                    hv[(size_t) r * nC + c] = shapeV ((db - s.lo) / std::max (1.f, s.hi - s.lo));
                }
            }

            // vertices: position, lighting, colour
            const V3 L = norm (V3 { -0.35f, 0.85f, -0.4f });
            float baseR = e.r, baseG = e.g, baseB = e.b;
            if (e.grey) { const float l = 0.3f * baseR + 0.59f * baseG + 0.11f * baseB; baseR = baseG = baseB = l * 0.8f; }
            for (int r = 0; r < nR; ++r)
                for (int c = 0; c < nC; ++c)
                {
                    const size_t i = (size_t) r * nC + c;
                    const float v = hv[i];
                    const float hl = hv[(size_t) r * nC + std::max (0, c - 1)], hr = hv[(size_t) r * nC + std::min (nC - 1, c + 1)];
                    const float hb = hv[(size_t) std::max (0, r - 1) * nC + c], hf = hv[(size_t) std::min (nR - 1, r + 1) * nC + c];
                    const float gx = (hr - hl) * 0.5f * worldH, gz = (hf - hb) * 0.5f * worldH;
                    const V3 nrm = norm (V3 { -gx * 6.f, 1.f, -gz * 6.f });
                    const float diff = std::max (0.f, dot (nrm, L));

                    float R = baseR, G = baseG, B = baseB;
                    if (! tapColours || e.grey)
                    {
                        if (! tapColours) { rampRGB (s.preset, v, R, G, B); if (e.grey) { const float l = 0.3f * R + 0.59f * G + 0.11f * B; R = G = B = l * 0.85f; } }
                    }
                    // bright, saturated surface (Ozone-style): strong base level, lit ridges, hot white-ish peaks
                    // Colours stay saturated: lighting only scales brightness, a little white is added on the
                    // sharpest lit ridges, and anything over 255 is scaled back (not clipped) so hues never wash to white.
                    const float k = tapColours ? 0.85f + 0.50f * diff : 0.82f + 0.38f * diff, tint = tapColours ? (0.82f + 0.18f * std::sqrt (v)) : 1.f;
                    const float glow = tapColours ? (v * v * v * 0.12f + std::pow (diff, 10.f) * 0.08f) * 255.f : std::pow (diff, 12.f) * 14.f;
                    R = R * k * tint + glow; G = G * k * tint + glow; B = B * k * tint + glow;
                    const float fog = 0.10f * (float) r / (float) (nR - 1);
                    R = lerp (R, 8.f, fog); G = lerp (G, 10.f, fog); B = lerp (B, 14.f, fog);
                    { const float mx = std::max ({ R, G, B }); if (mx > 255.f) { const float sc = 255.f / mx; R *= sc; G *= sc; B *= sc; } }
                    cr[i] = R; cg[i] = G; cb[i] = B;

                    const V3 q { -1.f + 2.f * (float) c / (float) (nC - 1), v * worldH, s.depth * (float) r / (float) (nR - 1) };
                    float x, y, iw;
                    if (project (proj, q, x, y, iw)) { sx[i] = x; sy[i] = y; siw[i] = iw; validV[i] = 1; } else validV[i] = 0;
                }

            auto vtx = [&] (int r, int c) { const size_t i = (size_t) r * nC + c; return Vtx { sx[i], sy[i], siw[i], cr[i], cg[i], cb[i], validV[i] != 0 }; };

            for (int r = 0; r < nR - 1; ++r)
                for (int c = 0; c < nC - 1; ++c)
                {
                    const int id = ((int) ei + 1) << 20 | (r << 10) | c;
                    const Vtx a = vtx (r, c), b = vtx (r, c + 1), d = vtx (r + 1, c), f = vtx (r + 1, c + 1);
                    tri (a, b, d, id); tri (b, f, d, id);
                }

            // skirts (front + both sides) so the slab looks solid: smooth dark wall under the surface
            const float sk = 0.62f;
            auto skirt = [&] (int r0, int c0, int r1, int c1)
            {
                auto top = [&] (int r, int c)
                {
                    Vtx v = vtx (r, c); const float v01 = hv[(size_t) r * nC + c], m = sk * (0.55f + 0.45f * v01);
                    if (tapColours) { v.r = baseR * m; v.g = baseG * m; v.b = baseB * m; }
                    else            { v.r *= 0.62f; v.g *= 0.62f; v.b *= 0.62f; }   // solid wall in the surface colours
                    return v;
                };
                auto base = [&] (int r, int c)
                {
                    const V3 q { -1.f + 2.f * (float) c / (float) (nC - 1), 0.f, s.depth * (float) r / (float) (nR - 1) };
                    Vtx v { 0, 0, 0, tapColours ? baseR * 0.20f : 6.f, tapColours ? baseG * 0.20f : 22.f, tapColours ? baseB * 0.20f : 40.f, false };
                    v.ok = project (proj, q, v.x, v.y, v.iw); return v;
                };
                const Vtx a = top (r0, c0), b = top (r1, c1), ba = base (r0, c0), bb = base (r1, c1);
                tri (a, b, ba, 0); tri (b, bb, ba, 0);
            };
            for (int c = 0; c < nC - 1; ++c) skirt (0, c, 0, c + 1);
            for (int r = 0; r < nR - 1; ++r) { skirt (r, 0, r + 1, 0); skirt (r, nC - 1, r + 1, nC - 1); }

            // contour lines: a lighter wire along frequency every few time rows, like the Ozone surface mesh
            for (int r = 0; r < nR; r += 8)
                for (int c = 0; c < nC - 1; ++c)
                    wire (vtx (r, c), vtx (r, c + 1));
            for (int c = 0; c < nC - 1; ++c) wire (vtx (nR - 1, c), vtx (nR - 1, c + 1));
        }
    }

private:
    struct Vtx { float x, y, iw, r, g, b; bool ok; };
    int W = 0, H = 0; uint32_t* pix = nullptr;
    std::vector<float> zbuf, hv, sx, sy, siw, cr, cg, cb; std::vector<char> validV;

    // 2 px wire that is depth-tested against the surface it lies on (small bias so it isn't z-fought away)
    void wire (const Vtx& a, const Vtx& b)
    {
        if (! (a.ok && b.ok)) return;
        const float dx = b.x - a.x, dy = b.y - a.y;
        const int n = std::max (1, (int) std::ceil (std::max (std::fabs (dx), std::fabs (dy))));
        const float lr = 0.5f * (a.r + b.r) * 1.08f + 10.f, lg = 0.5f * (a.g + b.g) * 1.08f + 10.f, lb = 0.5f * (a.b + b.b) * 1.08f + 10.f;
        const uint32_t col = packRGB (lr, lg, lb);
        for (int i = 0; i <= n; ++i)
        {
            const float t = (float) i / (float) n, iw = a.iw + (b.iw - a.iw) * t;
            const int x = (int) (a.x + dx * t), y0 = (int) (a.y + dy * t);
            if (x < 0 || x >= W) continue;
            for (int y = y0; y <= y0 + 1; ++y)
            {
                if (y < 0 || y >= H) continue;
                const size_t idx = (size_t) y * (size_t) W + (size_t) x;
                if (iw < zbuf[idx] * 0.965f) continue;
                pix[idx] = col;
            }
        }
    }

    void tri (const Vtx& a, const Vtx& b, const Vtx& c, int id)
    {
        if (! (a.ok && b.ok && c.ok)) return;
        const float area = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
        if (std::fabs (area) < 1e-5f) return;
        const float inv = 1.f / area;
        const int x0 = std::max (0, (int) std::floor (std::min ({ a.x, b.x, c.x }))), x1 = std::min (W - 1, (int) std::ceil (std::max ({ a.x, b.x, c.x })));
        const int y0 = std::max (0, (int) std::floor (std::min ({ a.y, b.y, c.y }))), y1 = std::min (H - 1, (int) std::ceil (std::max ({ a.y, b.y, c.y })));
        for (int y = y0; y <= y1; ++y)
            for (int x = x0; x <= x1; ++x)
            {
                const float px = (float) x + 0.5f, py = (float) y + 0.5f;
                const float w0 = ((b.x - px) * (c.y - py) - (b.y - py) * (c.x - px)) * inv;
                const float w1 = ((c.x - px) * (a.y - py) - (c.y - py) * (a.x - px)) * inv;
                const float w2 = 1.f - w0 - w1;
                if (w0 < -0.002f || w1 < -0.002f || w2 < -0.002f) continue;
                const float iw = w0 * a.iw + w1 * b.iw + w2 * c.iw;
                const size_t idx = (size_t) y * (size_t) W + (size_t) x;
                if (iw <= zbuf[idx]) continue;
                zbuf[idx] = iw;
                pix[idx] = packRGB (w0 * a.r + w1 * b.r + w2 * c.r, w0 * a.g + w1 * b.g + w2 * c.g, w0 * a.b + w1 * b.b + w2 * c.b);
                ids[idx] = id;
            }
    }
};

// ───────────── 2D spectrogram (frequency vertical, time horizontal, newest on the right)
struct Pane { int x = 0, y = 0, w = 0, h = 0; std::vector<int> entries; };

inline void render2D (Image& img, const std::vector<Entry>& entries, const std::vector<Pane>& panes, float lo, float hi, int preset)
{
    std::fill (img.px.begin(), img.px.end(), 0xff050709u);   // fully opaque canvas colour, no transparent pixels
    const float span = std::max (1.f, hi - lo);
    for (const auto& pane : panes)
    {
        if (pane.w < 2 || pane.h < 2) continue;
        // per-column time lookup and per-row frequency lookup
        struct TL { int r0, r1; float f; };
        struct FL { int c0, c1; float f; };
        std::vector<std::vector<TL>> tl (pane.entries.size());
        std::vector<std::vector<FL>> fl (pane.entries.size());
        for (size_t k = 0; k < pane.entries.size(); ++k)
        {
            const Hist& h = entries[(size_t) pane.entries[k]].hist;
            tl[k].resize ((size_t) pane.w); fl[k].resize ((size_t) pane.h);
            for (int px = 0; px < pane.w; ++px)
            {
                const float age = (1.f - (float) px / (float) (pane.w - 1)) * (float) (h.rows - 1);
                const int a0 = (int) age, a1 = std::min (a0 + 1, h.rows - 1);
                tl[k][(size_t) px] = { h.ringRow (a0), h.ringRow (a1), age - (float) a0 };
            }
            for (int py = 0; py < pane.h; ++py)
            {
                const float fc = (1.f - (float) py / (float) (pane.h - 1)) * (float) (h.cols - 1);
                const int c0 = (int) fc;
                fl[k][(size_t) py] = { c0, std::min (c0 + 1, h.cols - 1), fc - (float) c0 };
            }
        }
        for (int py = 0; py < pane.h; ++py)
            for (int px = 0; px < pane.w; ++px)
            {
                float R = 6.f, G = 8.f, B = 11.f, tmax = 0.f;
                for (size_t k = 0; k < pane.entries.size(); ++k)
                {
                    const Entry& e = entries[(size_t) pane.entries[k]];
                    const TL& t = tl[k][(size_t) px]; const FL& f = fl[k][(size_t) py];
                    auto row = [&] (int rr) { const float* d = e.hist.data + (size_t) rr * (size_t) e.hist.cols; return d[f.c0] + (d[f.c1] - d[f.c0]) * f.f; };
                    const float db = row (t.r0) + (row (t.r1) - row (t.r0)) * t.f;
                    const float v = shapeV ((db - lo) / span);
                    tmax = std::max (tmax, v);
                    if (preset == 0)
                    {
                        float cr = e.r, cg = e.g, cb = e.b;
                        if (e.grey) { const float l = 0.3f * cr + 0.59f * cg + 0.11f * cb; cr = cg = cb = l * 0.8f; }
                        const float i = clamp01 (std::pow (v, 1.15f) * 1.15f), glow = std::max (0.f, v - 0.82f) / 0.18f * 90.f;
                        // screen blend: overlapping taps mix instead of washing out to white
                        const float sr = std::min (255.f, cr * i + glow), sg = std::min (255.f, cg * i + glow), sb = std::min (255.f, cb * i + glow);
                        R = R + sr - R * sr / 255.f; G = G + sg - G * sg / 255.f; B = B + sb - B * sb / 255.f;
                    }
                }
                if (preset != 0) rampRGB (preset, tmax, R, G, B);
                img.px[(size_t) (pane.y + py) * (size_t) img.w + (size_t) (pane.x + px)] = packRGB (R, G, B);
            }
    }
}
} // namespace bridge
