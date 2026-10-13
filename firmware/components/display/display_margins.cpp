#include "display_margins.h"
#include "sdkconfig.h"

#include <nvs.h>
#include <nvs_flash.h>

static const char *NVS_NS         = "dispmargin";
static const char *NVS_KEY_LEFT   = "left";
static const char *NVS_KEY_RIGHT  = "right";
static const char *NVS_KEY_TOP    = "top";
static const char *NVS_KEY_BOTTOM = "bottom";

/* Frozen for the life of the session -- see the "why frozen" comment
 * in display_margins.h. Set once by display_margins_init(). */
static constexpr int DEFAULT_MARGIN = CONFIG_DRAFTLING_DISPLAY_BEZEL_MARGIN;
static constexpr int DEFAULT_LEFT = CONFIG_DRAFTLING_DISPLAY_BEZEL_MARGIN_LEFT;
static int s_left = DEFAULT_LEFT, s_right = DEFAULT_MARGIN;
static int s_top = DEFAULT_MARGIN, s_bottom = DEFAULT_MARGIN;

static int read_u8(nvs_handle_t h, const char *key, int fallback = DEFAULT_MARGIN)
{
    uint8_t v = 0;
    if (nvs_get_u8(h, key, &v) == ESP_OK) return v > 40 ? 40 : v;
    return fallback;
}

extern "C" void display_margins_init(void)
{
    display_margins_get_pending(&s_left, &s_right, &s_top, &s_bottom);
    /* Upgrade an existing zero-inset install without requiring full
     * erase. Seed only once so later deliberate adjustments survive. */
    if (DEFAULT_MARGIN == 0) return;
    nvs_handle_t h;
    uint8_t seeded = 0;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) == ESP_OK) {
        nvs_get_u8(h, "bezel_v1", &seeded);
        nvs_close(h);
    }
    if (seeded >= 2) return;
    if (s_left < DEFAULT_LEFT) s_left = DEFAULT_LEFT;
    /* Version 2 only adjusts the left edge on an already seeded device. */
    if (seeded == 0) {
        if (s_right < DEFAULT_MARGIN) s_right = DEFAULT_MARGIN;
        if (s_top < DEFAULT_MARGIN) s_top = DEFAULT_MARGIN;
        if (s_bottom < DEFAULT_MARGIN) s_bottom = DEFAULT_MARGIN;
    }
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) == ESP_OK) {
        if (nvs_set_u8(h, NVS_KEY_LEFT, (uint8_t)s_left) == ESP_OK &&
            nvs_set_u8(h, NVS_KEY_RIGHT, (uint8_t)s_right) == ESP_OK &&
            nvs_set_u8(h, NVS_KEY_TOP, (uint8_t)s_top) == ESP_OK &&
            nvs_set_u8(h, NVS_KEY_BOTTOM, (uint8_t)s_bottom) == ESP_OK) {
            nvs_set_u8(h, "bezel_v1", 2);
            nvs_commit(h);
        }
        nvs_close(h);
    }
}

extern "C" int display_margin_left(void)   { return s_left; }
extern "C" int display_margin_right(void)  { return s_right; }
extern "C" int display_margin_top(void)    { return s_top; }
extern "C" int display_margin_bottom(void) { return s_bottom; }

extern "C" void display_margins_set(int left, int right, int top, int bottom)
{
    /* Deliberately does NOT touch s_left/s_right/s_top/s_bottom --
     * see the file comment in display_margins.h. Only the NEXT boot's
     * display_margins_init() picks this up. */
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_u8(h, NVS_KEY_LEFT,   (uint8_t)left);
        nvs_set_u8(h, NVS_KEY_RIGHT,  (uint8_t)right);
        nvs_set_u8(h, NVS_KEY_TOP,    (uint8_t)top);
        nvs_set_u8(h, NVS_KEY_BOTTOM, (uint8_t)bottom);
        nvs_commit(h);
        nvs_close(h);
    }
}

extern "C" void display_margins_get_pending(int *left, int *right, int *top, int *bottom)
{
    int l = DEFAULT_LEFT, r = DEFAULT_MARGIN;
    int t = DEFAULT_MARGIN, b = DEFAULT_MARGIN;
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) == ESP_OK) {
        l = read_u8(h, NVS_KEY_LEFT, DEFAULT_LEFT);
        r = read_u8(h, NVS_KEY_RIGHT);
        t = read_u8(h, NVS_KEY_TOP);
        b = read_u8(h, NVS_KEY_BOTTOM);
        nvs_close(h);
    }
    if (left)   *left   = l;
    if (right)  *right  = r;
    if (top)    *top    = t;
    if (bottom) *bottom = b;
}
