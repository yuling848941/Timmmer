#include "timer_render_utils.h"

void BlendPixel(BYTE* pixels, int idx, BYTE r, BYTE g, BYTE b, float alpha) {
    if (alpha <= 0.01f) return;
    if (alpha >= 0.99f) {
        pixels[idx] = b; pixels[idx+1] = g; pixels[idx+2] = r; pixels[idx+3] = 255;
        return;
    }
    BYTE a = (BYTE)(alpha * 255);
    pixels[idx] = (pixels[idx] * (255 - a) + b * a) >> 8;
    pixels[idx+1] = (pixels[idx+1] * (255 - a) + g * a) >> 8;
    pixels[idx+2] = (pixels[idx+2] * (255 - a) + r * a) >> 8;
    pixels[idx+3] = a > pixels[idx+3] ? a : pixels[idx+3];
}

float RoundedRectSDF(float px, float py, float cx, float cy, float hw, float hh, float r) {
    float dx = fabsf(px - cx) - hw + r;
    float dy = fabsf(py - cy) - hh + r;
    float len = sqrtf(fmaxf(dx, 0.0f) * fmaxf(dx, 0.0f) + fmaxf(dy, 0.0f) * fmaxf(dy, 0.0f));
    return len + fminf(fmaxf(dx, dy), 0.0f) - r;
}

void DrawSoftShadowSDF(BYTE* pixels, int bufW, int bufH, int mx, int my, int mw, int mh, int r, int shadowSize, int offsetY, float maxOpacity) {
    float cx = mx + mw / 2.0f;
    float cy = my + mh / 2.0f + offsetY;
    float hw = mw / 2.0f;
    float hh = mh / 2.0f;
    
    for (int y = 0; y < bufH; y++) {
        for (int x = 0; x < bufW; x++) {
            float d = RoundedRectSDF((float)x, (float)y, cx, cy, hw, hh, (float)r);
            if (d < shadowSize) {
                float alpha = 0.0f;
                if (d <= 0.0f) {
                    alpha = maxOpacity;
                } else {
                    float t = 1.0f - (d / shadowSize);
                    alpha = maxOpacity * t * t * (3.0f - 2.0f * t);
                }
                BlendPixel(pixels, (y * bufW + x) * 4, 0, 0, 0, alpha);
            }
        }
    }
}

void FillRoundedRectAA(BYTE* pixels, int bufW, int bufH, int rx, int ry, int x, int y, int w, int h, BYTE r, BYTE g, BYTE b, BYTE a) {
    int x1 = x + w, y1 = y + h;
    for (int py = y; py < y1; py++) {
        if (py < 0 || py >= bufH) continue;
        float dy = (py < y + ry) ? (float)(y + ry - py) : (py >= y1 - ry ? (float)(py - (y1 - ry) + 1) : 0);
        for (int px = x; px < x1; px++) {
            if (px < 0 || px >= bufW) continue;
            float dx = (px < x + rx) ? (float)(x + rx - px) : (px >= x1 - rx ? (float)(px - (x1 - rx) + 1) : 0);
            float alpha = 1.0f;
            if (dx > 0 && dy > 0) {
                float dist = sqrtf(dx * dx + dy * dy);
                if (dist > (float)rx) {
                    float diff = dist - (float)rx;
                    if (diff >= 1.0f) continue;
                    alpha = 1.0f - diff;
                }
            }
            int idx = (py * bufW + px) * 4;
            BlendPixel(pixels, idx, r, g, b, alpha * (a / 255.0f));
        }
    }
}

void FillStripedRoundedRectAA(BYTE* pixels, int bufW, int bufH, int rx, int ry, int x, int y, int w, int h, BYTE r, BYTE g, BYTE b, BYTE a) {
    int x1 = x + w, y1 = y + h;
    for (int py = y; py < y1; py++) {
        if (py < 0 || py >= bufH) continue;
        float dy = (py < y + ry) ? (float)(y + ry - py) : (py >= y1 - ry ? (float)(py - (y1 - ry) + 1) : 0);
        for (int px = x; px < x1; px++) {
            if (px < 0 || px >= bufW) continue;
            float dx = (px < x + rx) ? (float)(x + rx - px) : (px >= x1 - rx ? (float)(px - (x1 - rx) + 1) : 0);
            float alpha = 1.0f;
            if (dx > 0 && dy > 0) {
                float dist = sqrtf(dx * dx + dy * dy);
                if (dist > (float)rx) {
                    float diff = dist - (float)rx;
                    if (diff >= 1.0f) continue;
                    alpha = 1.0f - diff;
                }
            }
            
            BYTE pr = r, pg = g, pb = b;
            int stripeWidth = 6;
            if (((px - x + py - y) / stripeWidth) % 2 == 0) {
                pr = (BYTE)fmaxf(0, pr - 30);
                pg = (BYTE)fmaxf(0, pg - 30);
                pb = (BYTE)fmaxf(0, pb - 30);
            }
            
            int idx = (py * bufW + px) * 4;
            BlendPixel(pixels, idx, pr, pg, pb, alpha * (a / 255.0f));
        }
    }
}

void DrawRoundedRectOutlineAA(BYTE* pixels, int bufW, int bufH, int rx, int ry, int x, int y, int w, int h, int thickness, BYTE r, BYTE g, BYTE b, BYTE a) {
    float cx = x + w / 2.0f;
    float cy = y + h / 2.0f;
    float hw = w / 2.0f;
    float hh = h / 2.0f;
    
    for (int py = y - thickness - 2; py <= y + h + thickness + 2; py++) {
        if (py < 0 || py >= bufH) continue;
        for (int px = x - thickness - 2; px <= x + w + thickness + 2; px++) {
            if (px < 0 || px >= bufW) continue;
            float d = RoundedRectSDF((float)px, (float)py, cx, cy, hw - thickness/2.0f, hh - thickness/2.0f, (float)rx);
            float distToEdge = fabsf(d);
            if (distToEdge < thickness / 2.0f + 1.0f) {
                float alpha = 1.0f;
                float diff = distToEdge - thickness / 2.0f;
                if (diff > 0.0f) alpha = 1.0f - diff;
                
                int idx = (py * bufW + px) * 4;
                BlendPixel(pixels, idx, r, g, b, alpha * (a / 255.0f));
            }
        }
    }
}
