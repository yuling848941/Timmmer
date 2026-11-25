#include "timer_dialog_glass.h"
#include "timer_theme.h"

void DialogGlass_Init(DialogGlass* g, int bufW, int bufH, int shadow, int radius) {
    DialogGlass_Destroy(g);
    memset(g, 0, sizeof(*g));
    g->bufW = bufW;
    g->bufH = bufH;
    g->shadow = shadow;
    g->radius = radius;
    DialogGlass_Rebuild(g);
}

void DialogGlass_Destroy(DialogGlass* g) {
    GlassCache_Free(&g->cache);
}

void DialogGlass_Rebuild(DialogGlass* g) {
    GlassCache_Build(&g->cache, g->bufW, g->bufH,
                     g->shadow, g->shadow, g->bufW - g->shadow * 2, g->bufH - g->shadow * 2,
                     g->radius, g->shadow, Ui_GetTheme());
}

void DialogGlass_Blit(DialogGlass* g, BYTE* pixels, int bufW, int bufH) {
    GlassCache_Blit(&g->cache, pixels, bufW, bufH);
}
