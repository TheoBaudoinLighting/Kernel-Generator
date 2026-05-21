/*
    Kernel Generator
    Author: Theo Baudoin
    Copyright (c) 2026 Theo Baudoin. All rights reserved.
*/

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cmath>
#include <cstring>

static const float PI_F  = 3.14159265358979323846f;
static const float TAU_F = 6.28318530717958647692f;
static const float EPS_F = 1e-6f;

static inline float clampf(float x, float a, float b) { return x < a ? a : (x > b ? b : x); }
static inline float saturate(float x) { return clampf(x, 0.0f, 1.0f); }
static inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
static inline float sqr(float x) { return x * x; }
static inline float absf(float x) { return x < 0.0f ? -x : x; }
static inline float signf(float x) { return x < 0.0f ? -1.0f : 1.0f; }
static inline float luminance(float r, float g, float b) { return r * 0.2126f + g * 0.7152f + b * 0.0722f; }

static inline float smoothstep(float a, float b, float x)
{
    float t = saturate((x - a) / (b - a));
    return t * t * (3.0f - 2.0f * t);
}

static inline float gaussian(float x, float center, float width)
{
    float d = (x - center) / width;
    return std::exp(-0.5f * d * d);
}

static inline int fastfloor(float x)
{
    int i = (int)x;
    return x < (float)i ? i - 1 : i;
}

static uint32_t hash32(uint32_t x)
{
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}

static uint32_t hash2i(int x, int y, uint32_t seed)
{
    uint32_t h = seed ^ 0x9e3779b9u;
    h ^= (uint32_t)x * 0x85ebca6bu;
    h = (h << 13) | (h >> 19);
    h ^= (uint32_t)y * 0xc2b2ae35u;
    return hash32(h);
}

static float hash01(int x, int y, uint32_t seed)
{
    return (hash2i(x, y, seed) >> 8) * (1.0f / 16777216.0f);
}

static float valueNoise(float x, float y, uint32_t seed)
{
    int ix = fastfloor(x);
    int iy = fastfloor(y);
    float fx = x - (float)ix;
    float fy = y - (float)iy;

    float ux = fx * fx * fx * (fx * (fx * 6.0f - 15.0f) + 10.0f);
    float uy = fy * fy * fy * (fy * (fy * 6.0f - 15.0f) + 10.0f);

    float a = hash01(ix,     iy,     seed);
    float b = hash01(ix + 1, iy,     seed);
    float c = hash01(ix,     iy + 1, seed);
    float d = hash01(ix + 1, iy + 1, seed);

    return lerpf(lerpf(a, b, ux), lerpf(c, d, ux), uy);
}

static float fbm(float x, float y, uint32_t seed, int octaves)
{
    float sum = 0.0f;
    float amp = 0.5f;
    float norm = 0.0f;

    for (int i = 0; i < octaves; ++i)
    {
        sum += amp * valueNoise(x, y, seed + (uint32_t)i * 1013u);
        norm += amp;

        float nx = x * 1.72f + y * 1.11f + 17.31f;
        float ny = y * 1.64f - x * 0.97f -  9.17f;
        x = nx;
        y = ny;
        amp *= 0.5f;
    }
    return sum / (norm > EPS_F ? norm : 1.0f);
}

struct Rng
{
    uint32_t s;
    explicit Rng(uint32_t seed) : s(seed ? seed : 1u) {}

    uint32_t next()
    {
        uint32_t x = s;
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        s = x;
        return x;
    }

    float f01() { return (next() >> 8) * (1.0f / 16777216.0f); }
    float range(float a, float b) { return a + (b - a) * f01(); }
    int rangei(int a, int b) { return a + (int)(f01() * (float)(b - a + 1)); }
};

struct Color
{
    float r, g, b, a;
};

struct Bubble
{
    float x, y;
    float radius;
    float width;
    float strength;
    float tint;
    float depth;
    int type;
};

struct Blob
{
    float x, y;
    float rx, ry;
    float ca, sa;
    float strength;
    float phase;
    float depth;
};

struct Scratch
{
    float x0, y0, x1, y1;
    float width;
    float strength;
    float depth;
};

static const int BUBBLE_COUNT  = 96;
static const int BLOB_COUNT    = 28;
static const int SCRATCH_COUNT = 22;

struct LensModel
{
    uint32_t seed;

    int bladeCount;
    float bladeRoundness;
    float bladeRotation;
    float baseCatEye;
    float baseApodization;

    float zDefocus;
    float zSpherical;
    float zComa;
    float zAstig;
    float zTrefoil;

    float longCA;
    float latCA;

    float rimStrength;
    float rimWidth;

    float edgeRoughness;
    float edgeDentAmp;
    float centerShiftBaseX;
    float centerShiftBaseY;

    Bubble bubbles[BUBBLE_COUNT];
    Blob blobs[BLOB_COUNT];
    Scratch scratches[SCRATCH_COUNT];
};

struct BokehQuery
{
    float u, v;
    float fieldX, fieldY;
    float defocus;
    float aperture;
    float focal;
    float frameScale;
};

struct LocalLensState
{
    float bladeScale;
    float catEyeX, catEyeY;
    float centerShiftX, centerShiftY;

    float zDefocus;
    float zSpherical;
    float zComa;
    float zAstig;
    float zTrefoil;

    float longCA;
    float latCA;
    float rimStrength;
    float rimWidth;
    float apodization;
    float edgeRoughness;
    float edgeDentAmp;
};

struct BokehSample
{
    float u, v;
    float pdf;
    Color value;
};

struct BokehKernel
{
    int w, h;
    Color* values;
    float* pmf;
    float* rowCdf;
    float* colCdf;
    float totalEnergy;
    float normScale;
    BokehQuery query;
};

struct KernelBank
{
    int fieldResX, fieldResY;
    int defocusRes;
    int apertureRes;
    int focalRes;

    float fieldMinX, fieldMaxX;
    float fieldMinY, fieldMaxY;
    float defocusMin, defocusMax;
    float apertureMin, apertureMax;
    float focalMin, focalMax;

    int kernelW, kernelH;
    BokehKernel* kernels;
};

static void randomPointInDisk(Rng& rng, float maxR, float& x, float& y)
{
    float a = rng.range(0.0f, TAU_F);
    float r = std::sqrt(rng.f01()) * maxR;
    x = std::cos(a) * r;
    y = std::sin(a) * r;
}

static LensModel makeLensModel(uint32_t seed)
{
    LensModel lens;
    std::memset(&lens, 0, sizeof(LensModel));
    lens.seed = seed;

    Rng rng(seed ^ 0x61c88647u);
    lens.bladeCount      = rng.rangei(7, 13);
    lens.bladeRoundness  = rng.range(0.72f, 0.97f);
    lens.bladeRotation   = rng.range(0.0f, TAU_F);
    lens.baseCatEye      = rng.range(0.08f, 0.22f);
    lens.baseApodization = rng.range(0.06f, 0.28f);

    lens.zDefocus   = rng.range(0.9f, 1.3f);
    lens.zSpherical = rng.range(-0.22f, 0.22f);
    lens.zComa      = rng.range(-0.16f, 0.16f);
    lens.zAstig     = rng.range(-0.14f, 0.14f);
    lens.zTrefoil   = rng.range(-0.10f, 0.10f);

    lens.longCA     = rng.range(0.010f, 0.028f);
    lens.latCA      = rng.range(0.006f, 0.018f);

    lens.rimStrength    = rng.range(1.1f, 1.8f);
    lens.rimWidth       = rng.range(0.014f, 0.032f);
    lens.edgeRoughness  = rng.range(0.010f, 0.030f);
    lens.edgeDentAmp    = rng.range(0.004f, 0.015f);
    lens.centerShiftBaseX = rng.range(-0.020f, 0.020f);
    lens.centerShiftBaseY = rng.range(-0.020f, 0.020f);

    for (int i = 0; i < BUBBLE_COUNT; ++i)
    {
        Bubble& b = lens.bubbles[i];
        randomPointInDisk(rng, rng.range(0.18f, 0.93f), b.x, b.y);
        b.type = rng.f01() < 0.24f ? 1 : 0;
        b.depth = rng.range(-0.85f, 0.85f);
        if (b.type == 1)
        {
            b.radius   = rng.range(0.0020f, 0.0130f);
            b.width    = rng.range(0.0030f, 0.0100f);
            b.strength = rng.range(0.25f, 1.00f);
            b.tint     = rng.range(-1.0f, 1.0f);
        }
        else
        {
            b.radius   = rng.range(0.010f, 0.040f);
            if ((i % 11) == 0) b.radius = rng.range(0.050f, 0.094f);
            b.width    = b.radius * rng.range(0.045f, 0.140f);
            b.strength = rng.range(0.10f, 0.55f);
            b.tint     = rng.range(-1.0f, 1.0f);
        }
    }

    for (int i = 0; i < BLOB_COUNT; ++i)
    {
        Blob& b = lens.blobs[i];
        randomPointInDisk(rng, rng.range(0.30f, 0.98f), b.x, b.y);
        b.rx = rng.range(0.020f, 0.120f);
        b.ry = rng.range(0.006f, 0.038f);
        if ((i % 5) == 0)
        {
            b.rx *= rng.range(1.6f, 2.9f);
            b.ry *= rng.range(0.7f, 1.3f);
        }
        float a = rng.range(0.0f, TAU_F);
        b.ca = std::cos(a);
        b.sa = std::sin(a);
        b.strength = rng.range(0.10f, 0.60f);
        b.phase = rng.range(-20.0f, 20.0f);
        b.depth = rng.range(-0.90f, 0.90f);
    }

    for (int i = 0; i < SCRATCH_COUNT; ++i)
    {
        Scratch& s = lens.scratches[i];
        float cx, cy;
        randomPointInDisk(rng, rng.range(0.25f, 0.96f), cx, cy);
        float a = rng.range(0.0f, TAU_F);
        float len = rng.range(0.05f, 0.32f);
        s.x0 = cx - std::cos(a) * len * 0.5f;
        s.y0 = cy - std::sin(a) * len * 0.5f;
        s.x1 = cx + std::cos(a) * len * 0.5f;
        s.y1 = cy + std::sin(a) * len * 0.5f;
        s.width = rng.range(0.0015f, 0.0070f);
        s.strength = rng.range(0.05f, 0.30f);
        s.depth = rng.range(-1.0f, 1.0f);
    }

    return lens;
}

static LocalLensState buildLocalLensState(const LensModel& lens, const BokehQuery& q)
{
    LocalLensState s;
    std::memset(&s, 0, sizeof(LocalLensState));

    float fieldR = std::sqrt(q.fieldX * q.fieldX + q.fieldY * q.fieldY);
    fieldR = clampf(fieldR, 0.0f, 1.5f);
    float blur = absf(q.defocus);
    float blurSign = signf(q.defocus);
    float aperture = q.aperture > 0.01f ? q.aperture : 0.01f;
    float focal = q.focal > 0.10f ? q.focal : 0.10f;

    s.bladeScale = aperture * (0.95f + 0.10f / focal);

    s.catEyeX = q.fieldX * lens.baseCatEye * fieldR * (1.0f + 0.30f * blur);
    s.catEyeY = q.fieldY * lens.baseCatEye * fieldR * (1.0f + 0.30f * blur);

    s.centerShiftX = lens.centerShiftBaseX + q.fieldX * 0.08f * fieldR;
    s.centerShiftY = lens.centerShiftBaseY + q.fieldY * 0.08f * fieldR;

    s.zDefocus   = lens.zDefocus * blur * blurSign;
    s.zSpherical = lens.zSpherical * (1.0f + 0.20f * blur);
    s.zComa      = lens.zComa + q.fieldX * 0.35f * fieldR + blurSign * q.fieldX * 0.08f;
    s.zAstig     = lens.zAstig + (q.fieldX * q.fieldX - q.fieldY * q.fieldY) * 0.25f;
    s.zTrefoil   = lens.zTrefoil + (q.fieldX * q.fieldX * q.fieldX - 3.0f * q.fieldX * q.fieldY * q.fieldY) * 0.08f;

    s.longCA = lens.longCA * (1.0f + blur * 0.8f + 0.10f / focal);
    s.latCA  = lens.latCA  * (1.0f + fieldR * 1.5f + 0.15f / focal);

    s.rimStrength = lens.rimStrength * (1.0f + blur * 0.3f);
    s.rimWidth    = lens.rimWidth * (1.0f + blur * 0.5f + (1.0f - aperture) * 0.35f);
    s.apodization = lens.baseApodization * (1.0f + fieldR * 0.5f + (1.0f - aperture) * 0.8f);
    s.edgeRoughness = lens.edgeRoughness * (1.0f + blur * 0.15f);
    s.edgeDentAmp   = lens.edgeDentAmp * (1.0f + fieldR * 0.20f);

    return s;
}

static float wrapPi(float a)
{
    while (a < -PI_F) a += TAU_F;
    while (a >  PI_F) a -= TAU_F;
    return a;
}

static float polygonRadiusNormalized(float theta, int blades)
{
    float sector = TAU_F / (float)blades;
    float local = std::fmod(theta + sector * 0.5f, sector);
    if (local < 0.0f) local += sector;
    local -= sector * 0.5f;
    float c = std::cos(local);
    if (absf(c) < 1e-4f) c = c < 0.0f ? -1e-4f : 1e-4f;
    return std::cos(PI_F / (float)blades) / c;
}

static float spectralWeightR(float lambda)
{
    return gaussian(lambda, 610.0f, 34.0f) + 0.22f * gaussian(lambda, 675.0f, 22.0f);
}

static float spectralWeightG(float lambda)
{
    return gaussian(lambda, 545.0f, 30.0f);
}

static float spectralWeightB(float lambda)
{
    return gaussian(lambda, 460.0f, 28.0f) + 0.18f * gaussian(lambda, 500.0f, 22.0f);
}

static float apertureRadiusProcedural(float theta, float spectralT, const LensModel& lens, const LocalLensState& ls)
{
    float t = theta + lens.bladeRotation + spectralT * ls.latCA * 2.1f;
    float poly = polygonRadiusNormalized(t, lens.bladeCount);
    float base = lerpf(poly, 1.0f, lens.bladeRoundness);

    float asym = 1.0f + ls.catEyeX * std::cos(t) + ls.catEyeY * std::sin(t);
    float rough = (fbm(std::cos(t) * 4.7f + 11.2f, std::sin(t) * 4.7f - 8.4f, lens.seed + 100u, 4) - 0.5f) * ls.edgeRoughness;
    rough += (valueNoise(std::cos(t) * 16.0f + 31.0f, std::sin(t) * 16.0f - 12.0f, lens.seed + 101u) - 0.5f) * ls.edgeDentAmp;
    float bladeDent = std::cos(t * (float)lens.bladeCount) * ls.edgeDentAmp * 0.4f;

    float radial = base * asym * (1.0f + rough + bladeDent);
    radial *= (1.0f + spectralT * ls.longCA);
    radial *= ls.bladeScale;
    return clampf(radial, 0.40f, 1.50f);
}

static float estimateFrameScale(const LensModel& lens, const BokehQuery& q)
{
    LocalLensState ls = buildLocalLensState(lens, q);

    float maxRadius = 0.0f;
    const int angleCount = 96;
    const int spectralCount = 5;

    for (int si = 0; si < spectralCount; ++si)
    {
        float lambda = lerpf(430.0f, 680.0f, (float)si / (float)(spectralCount - 1));
        float spectralT = (lambda - 555.0f) / 125.0f;

        for (int i = 0; i < angleCount; ++i)
        {
            float theta = ((float)i / (float)angleCount) * TAU_F;
            float r = apertureRadiusProcedural(theta, spectralT, lens, ls);
            if (r > maxRadius) maxRadius = r;
        }
    }

    float centerShift = std::sqrt(ls.centerShiftX * ls.centerShiftX + ls.centerShiftY * ls.centerShiftY);
    float chromaMargin = absf(ls.latCA) * 0.30f + absf(ls.longCA) * 0.12f;
    float frame = maxRadius * 1.10f + centerShift + chromaMargin + 0.035f;

    return clampf(frame, 1.05f, 1.85f);
}

static float wavefrontPhase(float rho, float theta, const LocalLensState& ls, float spectralT)
{
    float x = rho * std::cos(theta);
    float y = rho * std::sin(theta);

    float defocus = ls.zDefocus * rho * rho;
    float spherical = ls.zSpherical * rho * rho * rho * rho;
    float coma = ls.zComa * (3.0f * rho * rho * x - 2.0f * x * x * x);
    float astig = ls.zAstig * (x * x - y * y);
    float trefoil = ls.zTrefoil * (x * x * x - 3.0f * x * y * y);
    float chroma = spectralT * (0.35f + 0.20f * rho * rho);

    return defocus + spherical + coma + astig + trefoil + chroma;
}

static float distToSegment(float px, float py, const Scratch& s, float& tOut)
{
    float vx = s.x1 - s.x0;
    float vy = s.y1 - s.y0;
    float wx = px - s.x0;
    float wy = py - s.y0;
    float d = vx * vx + vy * vy;
    float t = d > 1e-8f ? (wx * vx + wy * vy) / d : 0.0f;
    t = saturate(t);
    float qx = s.x0 + vx * t;
    float qy = s.y0 + vy * t;
    tOut = t;
    return std::sqrt(sqr(px - qx) + sqr(py - qy));
}

static Color evalBokeh(const BokehQuery& q, const LensModel& lens)
{
    LocalLensState ls = buildLocalLensState(lens, q);

    float frameScale = q.frameScale > 0.01f ? q.frameScale : 1.0f;
    float x = (q.u * 2.0f - 1.0f) * frameScale;
    float y = (q.v * 2.0f - 1.0f) * frameScale;

    x -= ls.centerShiftX;
    y -= ls.centerShiftY;

    float baseR = std::sqrt(x * x + y * y);

    Color c = {0, 0, 0, 0};
    const int spectralCount = 9;
    float rgbNormR = 0.0f, rgbNormG = 0.0f, rgbNormB = 0.0f;

    for (int si = 0; si < spectralCount; ++si)
    {
        float lambda = lerpf(430.0f, 680.0f, (float)si / (float)(spectralCount - 1));
        float spectralT = (lambda - 555.0f) / 125.0f;

        float cx = x - spectralT * ls.latCA * 0.18f;
        float cy = y + spectralT * ls.latCA * 0.12f;
        float theta = std::atan2(cy, cx);
        float rr = std::sqrt(cx * cx + cy * cy);

        float apRadius = apertureRadiusProcedural(theta, spectralT, lens, ls);
        float rho = rr / (apRadius > EPS_F ? apRadius : 1.0f);

        float aperture = 1.0f - smoothstep(0.988f, 1.020f, rho);
        float haloMask = 1.0f - smoothstep(1.015f, 1.080f, rho);
        if (haloMask <= 0.0f) continue;

        float phase = wavefrontPhase(rho, theta, ls, spectralT);
        float n0 = fbm(cx * 3.3f + 100.0f, cy * 3.3f - 30.0f, lens.seed + 21u, 5);
        float n1 = fbm(cx * 12.5f - 13.0f, cy * 12.5f + 74.0f, lens.seed + 22u, 4);
        float n2 = fbm(cx * 58.0f, cy * 58.0f, lens.seed + 23u, 3);
        float edgeNoise = fbm(cx * 34.0f, cy * 34.0f, lens.seed + 24u, 3) - 0.5f;

        float apod = 1.0f - ls.apodization * sqr(rho);
        apod = clampf(apod, 0.20f, 1.0f);

        float centerBias = 0.44f + 0.18f * gaussian(rho, 0.0f, 0.34f) + 0.14f * (1.0f - rho * rho);
        float ringFreq = 7.0f + 10.0f * absf(ls.zDefocus) + 9.0f * absf(ls.zSpherical);
        float ringPhase = ringFreq * rho + phase * (11.0f + 4.0f * rho) + edgeNoise * 1.5f;
        float rings = (0.5f + 0.5f * std::cos(ringPhase * TAU_F)) * (0.18f + 0.18f * absf(q.defocus));
        rings *= std::exp(-rho * 2.8f);

        float cloud = (n0 - 0.5f) * 0.34f + (n1 - 0.5f) * 0.16f + (n2 - 0.5f) * 0.06f;
        float edgeLift = gaussian(rho, 0.997f + spectralT * 0.006f, ls.rimWidth);
        float outerHalo = gaussian(rho, 1.030f + spectralT * 0.010f, ls.rimWidth * 1.20f);

        float baseLum = (centerBias + cloud + rings) * aperture * apod;
        baseLum = baseLum < 0.0f ? 0.0f : baseLum;
        float rim = haloMask * ls.rimStrength * (0.90f * edgeLift + 0.35f * outerHalo);

        float wr = spectralWeightR(lambda);
        float wg = spectralWeightG(lambda);
        float wb = spectralWeightB(lambda);

        c.r += wr * (baseLum + rim * 0.95f);
        c.g += wg * (baseLum + rim * 1.00f);
        c.b += wb * (baseLum + rim * 1.05f);

        rgbNormR += wr;
        rgbNormG += wg;
        rgbNormB += wb;
        if (aperture > c.a) c.a = aperture;
    }

    c.r /= rgbNormR > EPS_F ? rgbNormR : 1.0f;
    c.g /= rgbNormG > EPS_F ? rgbNormG : 1.0f;
    c.b /= rgbNormB > EPS_F ? rgbNormB : 1.0f;

    float defectMask = saturate(c.a + (1.0f - smoothstep(0.98f, 1.05f, baseR)) * 0.25f);

    for (int i = 0; i < BUBBLE_COUNT; ++i)
    {
        const Bubble& b = lens.bubbles[i];
        float px = x * (1.0f + b.depth * 0.06f) - b.x;
        float py = y * (1.0f + b.depth * 0.06f) - b.y;
        float d = std::sqrt(px * px + py * py);
        float active = defectMask * (1.0f - smoothstep(0.96f, 1.03f, baseR));
        if (active <= 0.0f) continue;

        if (b.type == 1)
        {
            float spot = gaussian(d, 0.0f, b.radius);
            float halo = gaussian(d, b.radius * 2.3f, b.width * 2.2f);
            float a = active * b.strength;
            float dark = saturate(spot * a * 0.95f);
            c.r *= 1.0f - dark;
            c.g *= 1.0f - dark;
            c.b *= 1.0f - dark * 0.94f;
            c.r += halo * a * 0.028f;
            c.g += halo * a * 0.033f;
            c.b += halo * a * 0.043f;
        }
        else
        {
            float ring = gaussian(d, b.radius, b.width);
            float inner = 1.0f - smoothstep(b.radius * 0.18f, b.radius * 0.88f, d);
            float off = b.radius * (0.18f + 0.05f * b.depth);
            float d2 = std::sqrt(sqr(px + off) + sqr(py - off * 0.7f));
            float shadow = gaussian(d2, b.radius * 0.82f, b.width * 4.2f);
            float a = active * b.strength;

            c.r += ring * a * (0.42f + 0.20f * b.tint);
            c.g += ring * a * 0.52f;
            c.b += ring * a * (0.70f - 0.15f * b.tint);

            float dark = saturate((inner * 0.10f + shadow * 0.09f) * a);
            c.r *= 1.0f - dark * 0.95f;
            c.g *= 1.0f - dark * 0.90f;
            c.b *= 1.0f - dark * 0.82f;
        }
    }

    for (int i = 0; i < BLOB_COUNT; ++i)
    {
        const Blob& b = lens.blobs[i];
        float px = x * (1.0f + b.depth * 0.08f) - b.x;
        float py = y * (1.0f + b.depth * 0.08f) - b.y;
        float ax =  px * b.ca + py * b.sa;
        float ay = -px * b.sa + py * b.ca;
        float qv = sqr(ax / b.rx) + sqr(ay / b.ry);
        float g = std::exp(-0.5f * qv);
        float ragged = 0.45f + 0.75f * fbm(ax * 42.0f + b.phase, ay * 42.0f - b.phase, lens.seed + 300u + (uint32_t)i, 3);
        float amount = saturate(g * b.strength * ragged * defectMask);
        c.r *= 1.0f - amount * 0.82f;
        c.g *= 1.0f - amount * 0.86f;
        c.b *= 1.0f - amount * 0.90f;
        c.r += g * defectMask * b.strength * 0.012f;
        c.g += g * defectMask * b.strength * 0.016f;
        c.b += g * defectMask * b.strength * 0.022f;
    }

    for (int i = 0; i < SCRATCH_COUNT; ++i)
    {
        Scratch p = lens.scratches[i];
        float scale = 1.0f + p.depth * 0.06f;
        p.x0 *= scale; p.y0 *= scale; p.x1 *= scale; p.y1 *= scale;
        float t;
        float d = distToSegment(x, y, p, t);
        float line = gaussian(d, 0.0f, p.width);
        float breakup = 0.35f + 0.75f * valueNoise(t * 25.0f + (float)i * 9.1f, (float)i * 3.7f, lens.seed + 500u);
        float amount = saturate(line * p.strength * breakup * defectMask);
        c.r *= 1.0f - amount * 0.62f;
        c.g *= 1.0f - amount * 0.66f;
        c.b *= 1.0f - amount * 0.70f;
        c.r += amount * 0.028f;
        c.g += amount * 0.034f;
        c.b += amount * 0.042f;
    }

    float grain = (valueNoise(x * 260.0f, y * 260.0f, lens.seed + 900u) - 0.5f) * 0.025f;
    c.r += grain * defectMask * 0.75f;
    c.g += grain * defectMask * 0.85f;
    c.b += grain * defectMask * 1.00f;

    c.r = c.r < 0.0f ? 0.0f : c.r;
    c.g = c.g < 0.0f ? 0.0f : c.g;
    c.b = c.b < 0.0f ? 0.0f : c.b;
    c.a = saturate(c.a);
    return c;
}

static void initKernel(BokehKernel& k)
{
    k.w = 0; k.h = 0;
    k.values = 0;
    k.pmf = 0;
    k.rowCdf = 0;
    k.colCdf = 0;
    k.totalEnergy = 0.0f;
    k.normScale = 1.0f;
    std::memset(&k.query, 0, sizeof(BokehQuery));
}

static void destroyKernel(BokehKernel& k)
{
    delete[] k.values;  k.values = 0;
    delete[] k.pmf;     k.pmf = 0;
    delete[] k.rowCdf;  k.rowCdf = 0;
    delete[] k.colCdf;  k.colCdf = 0;
    k.w = k.h = 0;
    k.totalEnergy = 0.0f;
    k.normScale = 1.0f;
}

static void buildKernel(const LensModel& lens, const BokehQuery& q, int w, int h, BokehKernel& out)
{
    destroyKernel(out);

    BokehQuery localQ = q;
    if (localQ.frameScale <= 0.01f)
        localQ.frameScale = estimateFrameScale(lens, localQ);

    out.w = w;
    out.h = h;
    out.query = localQ;
    out.values = new Color[(size_t)w * (size_t)h];
    out.pmf = new float[(size_t)w * (size_t)h];
    out.rowCdf = new float[(size_t)h];
    out.colCdf = new float[(size_t)w * (size_t)h];

    double energy = 0.0;
    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            BokehQuery p = localQ;
            p.u = ((float)x + 0.5f) / (float)w;
            p.v = ((float)y + 0.5f) / (float)h;
            Color c = evalBokeh(p, lens);
            out.values[(size_t)y * (size_t)w + (size_t)x] = c;
            float e = luminance(c.r, c.g, c.b);
            out.pmf[(size_t)y * (size_t)w + (size_t)x] = e;
            energy += (double)e;
        }
    }

    out.totalEnergy = (float)energy;
    out.normScale = energy > 1e-12 ? (float)((double)(w * h) / energy) : 1.0f;

    for (int i = 0; i < w * h; ++i)
    {
        out.values[i].r *= out.normScale;
        out.values[i].g *= out.normScale;
        out.values[i].b *= out.normScale;
        out.pmf[i] *= out.normScale;
    }

    energy = 0.0;
    for (int i = 0; i < w * h; ++i) energy += out.pmf[i];
    out.totalEnergy = (float)energy;

    if (out.totalEnergy > 1e-12f)
    {
        for (int i = 0; i < w * h; ++i) out.pmf[i] /= out.totalEnergy;
    }
    else
    {
        float uniform = 1.0f / (float)(w * h);
        for (int i = 0; i < w * h; ++i) out.pmf[i] = uniform;
    }

    float accumRows = 0.0f;
    for (int y = 0; y < h; ++y)
    {
        float rowSum = 0.0f;
        for (int x = 0; x < w; ++x) rowSum += out.pmf[(size_t)y * (size_t)w + (size_t)x];
        accumRows += rowSum;
        out.rowCdf[y] = accumRows;

        float accumCols = 0.0f;
        if (rowSum > 1e-20f)
        {
            for (int x = 0; x < w; ++x)
            {
                accumCols += out.pmf[(size_t)y * (size_t)w + (size_t)x] / rowSum;
                out.colCdf[(size_t)y * (size_t)w + (size_t)x] = accumCols;
            }
        }
        else
        {
            for (int x = 0; x < w; ++x)
                out.colCdf[(size_t)y * (size_t)w + (size_t)x] = (float)(x + 1) / (float)w;
        }
    }

    if (h > 0) out.rowCdf[h - 1] = 1.0f;
    for (int y = 0; y < h; ++y) out.colCdf[(size_t)y * (size_t)w + (size_t)(w - 1)] = 1.0f;
}

static int cdfLowerBound(const float* cdf, int count, float xi)
{
    int lo = 0;
    int hi = count - 1;
    while (lo < hi)
    {
        int mid = lo + (hi - lo) / 2;
        if (cdf[mid] < xi) lo = mid + 1;
        else hi = mid;
    }
    return lo;
}

static float pdfBokeh(float u, float v, const BokehKernel& kernel)
{
    if (!kernel.pmf || kernel.w <= 0 || kernel.h <= 0) return 0.0f;
    int x = (int)(clampf(u, 0.0f, 0.999999f) * (float)kernel.w);
    int y = (int)(clampf(v, 0.0f, 0.999999f) * (float)kernel.h);
    float pmf = kernel.pmf[(size_t)y * (size_t)kernel.w + (size_t)x];
    return pmf * (float)(kernel.w * kernel.h);
}

static BokehSample sampleBokeh(float xi0, float xi1, const BokehKernel& kernel)
{
    BokehSample s;
    s.u = s.v = 0.5f;
    s.pdf = 0.0f;
    s.value = {0, 0, 0, 0};

    if (!kernel.pmf || !kernel.rowCdf || !kernel.colCdf || kernel.w <= 0 || kernel.h <= 0)
        return s;

    xi0 = saturate(xi0);
    xi1 = saturate(xi1);

    int row = cdfLowerBound(kernel.rowCdf, kernel.h, xi1);
    const float* rowCols = kernel.colCdf + (size_t)row * (size_t)kernel.w;
    int col = cdfLowerBound(rowCols, kernel.w, xi0);

    s.u = ((float)col + 0.5f) / (float)kernel.w;
    s.v = ((float)row + 0.5f) / (float)kernel.h;
    s.pdf = pdfBokeh(s.u, s.v, kernel);
    s.value = kernel.values[(size_t)row * (size_t)kernel.w + (size_t)col];
    return s;
}

static size_t kernelBankFlatIndex(const KernelBank& bank, int fx, int fy, int d, int a, int f)
{
    size_t idx = (size_t)fx;
    idx = idx * (size_t)bank.fieldResY + (size_t)fy;
    idx = idx * (size_t)bank.defocusRes + (size_t)d;
    idx = idx * (size_t)bank.apertureRes + (size_t)a;
    idx = idx * (size_t)bank.focalRes + (size_t)f;
    return idx;
}

static void initKernelBank(KernelBank& bank)
{
    std::memset(&bank, 0, sizeof(KernelBank));
}

static void destroyKernelBank(KernelBank& bank)
{
    if (bank.kernels)
    {
        size_t count = (size_t)bank.fieldResX * (size_t)bank.fieldResY * (size_t)bank.defocusRes * (size_t)bank.apertureRes * (size_t)bank.focalRes;
        for (size_t i = 0; i < count; ++i) destroyKernel(bank.kernels[i]);
        delete[] bank.kernels;
        bank.kernels = 0;
    }
    std::memset(&bank, 0, sizeof(KernelBank));
}

static float sampleGridValue(int i, int n, float a, float b)
{
    if (n <= 1) return 0.5f * (a + b);
    return lerpf(a, b, (float)i / (float)(n - 1));
}

static void buildKernelBank(const LensModel& lens, KernelBank& bank)
{
    if (bank.kernels)
    {
        size_t oldCount = (size_t)bank.fieldResX * (size_t)bank.fieldResY * (size_t)bank.defocusRes * (size_t)bank.apertureRes * (size_t)bank.focalRes;
        for (size_t i = 0; i < oldCount; ++i) destroyKernel(bank.kernels[i]);
        delete[] bank.kernels;
        bank.kernels = 0;
    }

    size_t count = (size_t)bank.fieldResX * (size_t)bank.fieldResY * (size_t)bank.defocusRes * (size_t)bank.apertureRes * (size_t)bank.focalRes;
    bank.kernels = new BokehKernel[count];
    for (size_t i = 0; i < count; ++i) initKernel(bank.kernels[i]);

    for (int fx = 0; fx < bank.fieldResX; ++fx)
    for (int fy = 0; fy < bank.fieldResY; ++fy)
    for (int d  = 0; d  < bank.defocusRes; ++d)
    for (int a  = 0; a  < bank.apertureRes; ++a)
    for (int f  = 0; f  < bank.focalRes; ++f)
    {
        BokehQuery q;
        q.u = 0.5f;
        q.v = 0.5f;
        q.fieldX   = sampleGridValue(fx, bank.fieldResX, bank.fieldMinX, bank.fieldMaxX);
        q.fieldY   = sampleGridValue(fy, bank.fieldResY, bank.fieldMinY, bank.fieldMaxY);
        q.defocus  = sampleGridValue(d,  bank.defocusRes, bank.defocusMin, bank.defocusMax);
        q.aperture = sampleGridValue(a,  bank.apertureRes, bank.apertureMin, bank.apertureMax);
        q.focal    = sampleGridValue(f,  bank.focalRes, bank.focalMin, bank.focalMax);
        q.frameScale = 0.0f;

        size_t idx = kernelBankFlatIndex(bank, fx, fy, d, a, f);
        buildKernel(lens, q, bank.kernelW, bank.kernelH, bank.kernels[idx]);
    }
}

static int nearestGridIndex(float x, int n, float a, float b)
{
    if (n <= 1) return 0;
    float t = saturate((x - a) / (b - a));
    int i = (int)(t * (float)(n - 1) + 0.5f);
    if (i < 0) i = 0;
    if (i > n - 1) i = n - 1;
    return i;
}

static const BokehKernel& fetchKernel(const KernelBank& bank, float fieldX, float fieldY, float defocus, float aperture, float focal)
{
    int fx = nearestGridIndex(fieldX, bank.fieldResX, bank.fieldMinX, bank.fieldMaxX);
    int fy = nearestGridIndex(fieldY, bank.fieldResY, bank.fieldMinY, bank.fieldMaxY);
    int d  = nearestGridIndex(defocus, bank.defocusRes, bank.defocusMin, bank.defocusMax);
    int a  = nearestGridIndex(aperture, bank.apertureRes, bank.apertureMin, bank.apertureMax);
    int f  = nearestGridIndex(focal, bank.focalRes, bank.focalMin, bank.focalMax);
    return bank.kernels[kernelBankFlatIndex(bank, fx, fy, d, a, f)];
}

static unsigned char toByte(float linear)
{
    linear = linear < 0.0f ? 0.0f : linear;
    float mapped = linear / (1.0f + linear * 0.65f);
    mapped = std::pow(saturate(mapped), 1.0f / 2.2f);
    int v = (int)(mapped * 255.0f + 0.5f);
    return (unsigned char)(v < 0 ? 0 : (v > 255 ? 255 : v));
}

static int writeTGA_RGBA(const char* path, const unsigned char* bgra, int w, int h)
{
    FILE* f = std::fopen(path, "wb");
    if (!f) return 0;

    unsigned char header[18] = {0};
    header[2]  = 2;
    header[12] = (unsigned char)(w & 255);
    header[13] = (unsigned char)((w >> 8) & 255);
    header[14] = (unsigned char)(h & 255);
    header[15] = (unsigned char)((h >> 8) & 255);
    header[16] = 32;
    header[17] = 0x20 | 8;

    std::fwrite(header, 1, 18, f);
    std::fwrite(bgra, 1, (size_t)w * (size_t)h * 4u, f);
    std::fclose(f);
    return 1;
}

static int saveKernelPreviewTGA(const char* path, const BokehKernel& kernel)
{
    if (!kernel.values || kernel.w <= 0 || kernel.h <= 0) return 0;
    unsigned char* img = new unsigned char[(size_t)kernel.w * (size_t)kernel.h * 4u];
    for (int y = 0; y < kernel.h; ++y)
    {
        for (int x = 0; x < kernel.w; ++x)
        {
            const Color& c = kernel.values[(size_t)y * (size_t)kernel.w + (size_t)x];
            size_t p = ((size_t)y * (size_t)kernel.w + (size_t)x) * 4u;
            img[p + 0] = toByte(c.b);
            img[p + 1] = toByte(c.g);
            img[p + 2] = toByte(c.r);
            img[p + 3] = (unsigned char)(saturate(c.a) * 255.0f + 0.5f);
        }
    }
    int ok = writeTGA_RGBA(path, img, kernel.w, kernel.h);
    delete[] img;
    return ok;
}

int main(int argc, char** argv)
{
    uint32_t seed     = argc > 1 ? (uint32_t)std::strtoul(argv[1], 0, 10) : 1337u;
    int kernelW       = argc > 2 ? std::atoi(argv[2]) : 512;
    int kernelH       = argc > 3 ? std::atoi(argv[3]) : 512;
    const char* outTga= argc > 4 ? argv[4] : "bokeh_kernel_system.tga";

    if (kernelW < 8) kernelW = 8;
    if (kernelH < 8) kernelH = 8;

    LensModel lens = makeLensModel(seed);

    BokehQuery q;
    q.u = 0.5f; q.v = 0.5f;
    q.fieldX = 0.58f;
    q.fieldY = -0.22f;
    q.defocus = 0.75f;
    q.aperture = 0.95f;
    q.focal = 0.85f;
    q.frameScale = 0.0f;

    BokehKernel kernel;
    initKernel(kernel);
    buildKernel(lens, q, kernelW, kernelH, kernel);

    if (!saveKernelPreviewTGA(outTga, kernel))
    {
        std::fprintf(stderr, "Impossible d'ecrire %s\n", outTga);
        destroyKernel(kernel);
        return 1;
    }

    BokehSample s = sampleBokeh(0.37f, 0.81f, kernel);
    float pe = pdfBokeh(0.50f, 0.50f, kernel);

    std::printf("Kernel direct OK -> %s\n", outTga);
    std::printf("Direct query: field=(%.3f, %.3f) defocus=%.3f aperture=%.3f focal=%.3f\n",
                q.fieldX, q.fieldY, q.defocus, q.aperture, q.focal);
    std::printf("Sample: uv=(%.6f, %.6f) pdf=%.6f value=(%.6f, %.6f, %.6f)\n",
                s.u, s.v, s.pdf, s.value.r, s.value.g, s.value.b);
    std::printf("Eval/pdf: pdf(center)=%.6f totalEnergy=%.6f normScale=%.6f\n",
                pe, kernel.totalEnergy, kernel.normScale);

    KernelBank bank;
    initKernelBank(bank);
    bank.fieldResX = 3; bank.fieldResY = 3;
    bank.defocusRes = 3;
    bank.apertureRes = 2;
    bank.focalRes = 2;
    bank.fieldMinX = -1.0f; bank.fieldMaxX = 1.0f;
    bank.fieldMinY = -1.0f; bank.fieldMaxY = 1.0f;
    bank.defocusMin = -1.0f; bank.defocusMax = 1.0f;
    bank.apertureMin = 0.70f; bank.apertureMax = 1.00f;
    bank.focalMin = 0.60f; bank.focalMax = 1.20f;
    bank.kernelW = 128; bank.kernelH = 128;

    buildKernelBank(lens, bank);
    const BokehKernel& cached = fetchKernel(bank, 0.62f, -0.18f, 0.80f, 0.92f, 0.88f);
    std::printf("Bank: fetched kernel %dx%d for nearest params -> field=(%.3f, %.3f) defocus=%.3f aperture=%.3f focal=%.3f\n",
                cached.w, cached.h,
                cached.query.fieldX, cached.query.fieldY, cached.query.defocus,
                cached.query.aperture, cached.query.focal);

    destroyKernelBank(bank);
    destroyKernel(kernel);
    return 0;
}
