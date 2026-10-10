/*
 * IDLEC v1.5.c — AuroraOS C SDK idle game
 * Controls:
 *   Y / touch core: collect credits
 *   D-pad / circle pad: navigate tabs and items
 *   A: interact with selected UI item (buy/install/ascend)
 *   X: save now; B: back/save
 * Save data: app data directory/save.dat
 */
#include <aurora_app.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define SAVE_MAGIC 0x52464F52u
#define SAVE_VERSION 3u
#define MAX_COUNT 999999u
#define WHITE 0xF1F5F9
#define MUTED 0x91A4BD
#define BG 0x08111F
#define PANEL 0x111F32
#define PANEL2 0x172941
#define CYAN 0x55E6D0
#define BLUE 0x5D9CFF
#define GOLD 0xFFD166
#define PINK 0xFF6B9D
#define GREEN 0x7BE495
#define RED 0xFF647C

#define GEN_COUNT 8
#define UPG_COUNT 8
#define ASC_COUNT 6

typedef struct {
    uint32_t magic, version;
    uint64_t credits, lifetime, clicks, best;
    uint32_t click_power;
    uint32_t generators[GEN_COUNT];
    uint32_t upgrades[UPG_COUNT];
    uint32_t ascensions;
    uint64_t ascension_points;
    uint32_t ascension_upgrades[ASC_COUNT];
    uint64_t play_seconds;
} SaveData;

typedef struct {
    uint32_t magic, version;
    uint64_t credits, lifetime, clicks, best;
    uint32_t click_power;
    uint32_t generators[GEN_COUNT];
    uint32_t upgrades[UPG_COUNT];
    uint32_t ascensions;
    uint64_t ascension_points;
    uint32_t ascension_upgrades[ASC_COUNT];
} SaveDataV2;

typedef struct {
    uint32_t magic, version;
    uint64_t credits, lifetime, clicks, best;
    uint32_t click_power;
    uint32_t generators[4];
    uint32_t click_upgrade, crit_upgrade, production_upgrade;
    uint32_t prestige;
} LegacySaveData;

static SaveData g;
static char message[72] = "Press Y to collect credits.";
static unsigned message_frames = 180;
static unsigned tab = 0, selected = 0;
static uint32_t tick = 0;
static uint64_t run_seconds = 0;

static const char *gen_names[GEN_COUNT] = {
    "DRONE", "WORKSHOP", "REACTOR", "SINGULARITY",
    "NANOFORGE", "DYSON ARRAY", "STAR EATER", "TIME ENGINE"
};
static const char *gen_desc[GEN_COUNT] = {
    "Automated taps", "Industrial output", "Quantum output", "Reality output",
    "Self-replicating bots", "Harvests star energy", "Consumes dead stars", "Extracts time"
};
static const uint64_t base_cost[GEN_COUNT] = {15, 100, 750, 5000, 30000, 180000, 1200000, 9000000};
static const uint32_t base_rate[GEN_COUNT] = {1, 6, 42, 280, 1800, 12000, 85000, 650000};

static const char *upgrade_names[UPG_COUNT] = {
    "TAP AMPLIFIER", "CRITICAL MATRIX", "PRODUCTION CORE", "OVERDRIVE",
    "AUTO-COLLECTOR", "QUANTUM LENS", "EFFICIENT CIRCUITS", "REALITY ENGINE"
};
static const char *upgrade_desc[UPG_COUNT] = {
    "+4 tap power / level", "More frequent x5 criticals", "+25% production / level",
    "+10% tap power / level", "+5% all production / level", "+50% critical reward / level",
    "Generators produce +12% / level", "All production +35% / level"
};
static const uint64_t upgrade_base[UPG_COUNT] = {250, 600, 1200, 2500, 5000, 15000, 30000, 100000};
static const uint32_t upgrade_cap[UPG_COUNT] = {999, 30, 99, 100, 100, 50, 100, 50};

static const char *asc_names[ASC_COUNT] = {
    "PRIMAL TOUCH", "ETERNAL ENGINE", "POINT MAGNET",
    "CRITICAL SOUL", "STARTER CACHE", "DEEP KNOWLEDGE"
};
static const char *asc_desc[ASC_COUNT] = {
    "+25% tap power permanently", "+20% production permanently",
    "+10% ascension points", "+2% critical chance",
    "Start each run with 1,000 credits / level", "+5% all gains permanently"
};
static const uint64_t asc_base[ASC_COUNT] = {1, 1, 2, 2, 3, 4};

static void notify(const char *s) {
    snprintf(message, sizeof(message), "%s", s);
    message_frames = 150;
}
static uint64_t sat_mul(uint64_t a, uint64_t b) {
    if (a && b > UINT64_MAX / a) return UINT64_MAX;
    return a * b;
}
static uint64_t scaled_cost(uint64_t base, uint32_t level, unsigned pct) {
    uint64_t c = base;
    for (uint32_t i = 0; i < level && i < 500; ++i) {
        if (c > UINT64_MAX / (100 + pct)) return UINT64_MAX;
        c = c * (100 + pct) / 100 + 1;
    }
    return c;
}
static uint64_t generator_cost(unsigned i) {
    return scaled_cost(base_cost[i], g.generators[i], 35);
}
static uint64_t upgrade_cost(unsigned i) {
    uint64_t level = g.upgrades[i];
    if (level >= upgrade_cap[i]) return UINT64_MAX;
    if (level + 1 > UINT64_MAX / upgrade_base[i] / (level + 1)) return UINT64_MAX;
    return scaled_cost(upgrade_base[i], (uint32_t)level, 55);
}
static uint64_t asc_cost(unsigned i) {
    uint32_t level = g.ascension_upgrades[i];
    if (level >= 100) return UINT64_MAX;
    return asc_base[i] + (uint64_t)level * (asc_base[i] + 1);
}
static uint64_t click_power(void) {
    uint64_t p = g.click_power + (uint64_t)g.upgrades[0] * 4;
    p = p * (100 + g.upgrades[3] * 10) / 100;
    p = p * (100 + g.ascension_upgrades[0] * 25) / 100;
    p = p * (100 + g.ascension_upgrades[5] * 5) / 100;
    return p ? p : 1;
}
static uint64_t production_per_second(void) {
    uint64_t total = 0;
    for (unsigned i = 0; i < GEN_COUNT; ++i) {
        uint64_t rate = sat_mul(g.generators[i], base_rate[i]);
        for (unsigned u = 0; u < g.upgrades[2] && u < 100; ++u) rate = sat_mul(rate, 125) / 100;
        for (unsigned u = 0; u < g.upgrades[4] && u < 100; ++u) rate = sat_mul(rate, 105) / 100;
        for (unsigned u = 0; u < g.upgrades[6] && u < 100; ++u) rate = sat_mul(rate, 112) / 100;
        for (unsigned u = 0; u < g.upgrades[7] && u < 50; ++u) rate = sat_mul(rate, 135) / 100;
        total = UINT64_MAX - total < rate ? UINT64_MAX : total + rate;
    }
    for (unsigned u = 0; u < g.ascension_upgrades[1] && u < 100; ++u) total = sat_mul(total, 120) / 100;
    for (unsigned u = 0; u < g.ascension_upgrades[5] && u < 100; ++u) total = sat_mul(total, 105) / 100;
    return total;
}
static void add_credits(uint64_t amount) {
    g.credits = UINT64_MAX - g.credits < amount ? UINT64_MAX : g.credits + amount;
    g.lifetime = UINT64_MAX - g.lifetime < amount ? UINT64_MAX : g.lifetime + amount;
    if (g.credits > g.best) g.best = g.credits;
}
static void save_game(void) {
    char path[300];
    snprintf(path, sizeof(path), "%s/save.dat", app_dir());
    FILE *f = fopen(path, "wb");
    if (f) {
        g.magic = SAVE_MAGIC; g.version = SAVE_VERSION;
        fwrite(&g, sizeof(g), 1, f); fclose(f);
        notify("SAVE COMPLETE // progress secured");
    } else notify("SAVE FAILED // storage unavailable");
}
static void load_game(void) {
    char path[300]; snprintf(path, sizeof(path), "%s/save.dat", app_dir());
    FILE *f = fopen(path, "rb");
    if (f) {
        SaveData loaded;
        size_t n = fread(&loaded, 1, sizeof(loaded), f);
        rewind(f);
        if (n == sizeof(loaded) && loaded.magic == SAVE_MAGIC && loaded.version == SAVE_VERSION) {
            g = loaded; fclose(f); if (!g.click_power) g.click_power = 1;
            notify("SAVE RESTORED // welcome back"); return;
        }
        rewind(f);
        SaveDataV2 old_v2;
        n = fread(&old_v2, 1, sizeof(old_v2), f);
        if (n == sizeof(old_v2) && old_v2.magic == SAVE_MAGIC && old_v2.version == 2) {
            memset(&g, 0, sizeof(g));
            g.magic = SAVE_MAGIC; g.version = SAVE_VERSION;
            g.credits = old_v2.credits; g.lifetime = old_v2.lifetime; g.clicks = old_v2.clicks; g.best = old_v2.best;
            g.click_power = old_v2.click_power ? old_v2.click_power : 1;
            memcpy(g.generators, old_v2.generators, sizeof(g.generators));
            memcpy(g.upgrades, old_v2.upgrades, sizeof(g.upgrades));
            g.ascensions = old_v2.ascensions; g.ascension_points = old_v2.ascension_points;
            memcpy(g.ascension_upgrades, old_v2.ascension_upgrades, sizeof(g.ascension_upgrades));
            fclose(f); notify("SAVE UPDATED // playtime tracking enabled"); return;
        }
        rewind(f);
        LegacySaveData old;
        n = fread(&old, 1, sizeof(old), f); fclose(f);
        if (n == sizeof(old) && old.magic == SAVE_MAGIC && old.version == 1) {
            memset(&g, 0, sizeof(g));
            g.magic = SAVE_MAGIC; g.version = SAVE_VERSION;
            g.credits = old.credits; g.lifetime = old.lifetime; g.clicks = old.clicks; g.best = old.best;
            g.click_power = old.click_power ? old.click_power : 1;
            for (unsigned i=0; i<4; ++i) g.generators[i] = old.generators[i];
            g.upgrades[0] = old.click_upgrade; g.upgrades[1] = old.crit_upgrade;
            g.upgrades[2] = old.production_upgrade; g.ascensions = old.prestige;
            notify("OLD SAVE CONVERTED // expanded systems online"); return;
        }
    }
    memset(&g, 0, sizeof(g)); g.magic = SAVE_MAGIC; g.version = SAVE_VERSION; g.click_power = 1;
}
static void collect(void) {
    uint64_t amount = click_power();
    unsigned crit = 5 + g.upgrades[1] * 3 + g.ascension_upgrades[3] * 2;
    if (crit > 80) crit = 80;
    if (g.upgrades[5]) amount = sat_mul(amount, 100 + g.upgrades[5] * 50) / 100;
    if ((unsigned)(rand() % 100) < crit) {
        amount = sat_mul(amount, 5);
        notify("CRITICAL HARVEST // x5 credits!");
    } else notify("CORE HARVESTED // credits collected");
        for (unsigned i=0; i<g.upgrades[4]; ++i) amount = sat_mul(amount, 105) / 100;
        for (unsigned i=0; i<g.ascension_upgrades[5]; ++i) amount = sat_mul(amount, 105) / 100;
        add_credits(amount); g.clicks++;
}
static int buy_generator(unsigned i) {
    if (i >= GEN_COUNT) return 0;
    uint64_t cost = generator_cost(i);
    if (g.credits < cost) { notify("INSUFFICIENT CREDITS"); return 0; }
    if (g.generators[i] >= MAX_COUNT) { notify("GENERATOR LIMIT REACHED"); return 0; }
    g.credits -= cost; g.generators[i]++;
    notify("GENERATOR ONLINE // production increased"); return 1;
}
static int buy_upgrade(unsigned i) {
    if (i >= UPG_COUNT || g.upgrades[i] >= upgrade_cap[i]) { notify("UPGRADE AT MAX LEVEL"); return 0; }
    uint64_t cost = upgrade_cost(i);
    if (g.credits < cost) { notify("NOT ENOUGH CREDITS FOR THIS UPGRADE"); return 0; }
    g.credits -= cost; g.upgrades[i]++;
    notify("UPGRADE INSTALLED // systems improved"); return 1;
}
static void ascend(void) {
    if (g.lifetime < 1000000ULL) { notify("ASCENSION LOCKED // need 1,000,000 lifetime credits"); return; }
        uint64_t points = (g.lifetime / 1000000ULL);
        points = points * (100 + g.ascension_upgrades[2] * 10) / 100;
        if (!points) points = 1;
        g.ascension_points = UINT64_MAX - g.ascension_points < points ? UINT64_MAX : g.ascension_points + points;
    g.ascensions++;
    g.credits = 1000ULL * g.ascension_upgrades[4];
    memset(g.generators, 0, sizeof(g.generators));
    memset(g.upgrades, 0, sizeof(g.upgrades));
    g.click_power = 1 + g.ascensions;
    g.lifetime = 0;
    notify("ASCENSION COMPLETE // permanent points awarded");
    save_game();
}
static void buy_asc_upgrade(unsigned i) {
    if (i >= ASC_COUNT) return;
    uint64_t cost = asc_cost(i);
    if (g.ascension_points < cost) { notify("NOT ENOUGH ASCENSION POINTS"); return; }
    g.ascension_points -= cost; g.ascension_upgrades[i]++;
    notify("PERMANENT UPGRADE UNLOCKED");
}
static void text(int screen, int x, int y, int color, const char *s) { gfx_text(screen, x, y, color, s); }
static void panel(int screen, int x, int y, int w, int h, int color) { gfx_round_rect(screen, x, y, w, h, 7, color); }
static void draw_top(void) {
    gfx_clear(SCREEN_TOP, BG); gfx_gradient(SCREEN_TOP, 0, 0, 400, 4, CYAN, BLUE);
    text(SCREEN_TOP, 16, 12, CYAN, "IDLEC");
    text(SCREEN_TOP, 304, 14, MUTED, "v1.5.c");
    gfx_print(SCREEN_TOP, 145, 13, FONT_SMALL, GOLD, "PLAYTIME %02llu:%02llu:%02llu",
              (unsigned long long)(g.play_seconds / 3600),
              (unsigned long long)((g.play_seconds / 60) % 60),
              (unsigned long long)(g.play_seconds % 60));
    panel(SCREEN_TOP, 14, 36, 372, 57, PANEL);
    text(SCREEN_TOP, 26, 45, MUTED, "AVAILABLE CREDITS");
    gfx_print(SCREEN_TOP, 26, 61, FONT_TITLE, WHITE, "%llu", (unsigned long long)g.credits);
    panel(SCREEN_TOP, 14, 101, 179, 39, PANEL);
    text(SCREEN_TOP, 24, 108, MUTED, "PASSIVE / SEC");
    gfx_print(SCREEN_TOP, 24, 121, FONT_BOLD, GREEN, "%llu", (unsigned long long)production_per_second());
    panel(SCREEN_TOP, 201, 101, 185, 39, PANEL);
    text(SCREEN_TOP, 211, 108, MUTED, "COLLECT POWER");
    gfx_print(SCREEN_TOP, 211, 121, FONT_BOLD, GOLD, "%llu", (unsigned long long)click_power());
    panel(SCREEN_TOP, 14, 151, 372, 70, PANEL2);
    gfx_circle(SCREEN_TOP, 54, 186, 22, CYAN); gfx_circle(SCREEN_TOP, 54, 186, 14, BG); gfx_circle(SCREEN_TOP, 54, 186, 6, CYAN);
    text(SCREEN_TOP, 86, 169, WHITE, "COLLECT CREDITS");
    text(SCREEN_TOP, 86, 184, CYAN, "PRESS Y");
    text(SCREEN_TOP, 86, 198, MUTED, "or tap to collect");
    panel(SCREEN_TOP, 14, 230, 372, 7, PANEL);
    unsigned pct = g.lifetime >= 1000000ULL ? 100 : (unsigned)(g.lifetime / 10000ULL);
    if (pct > 100) pct = 100;
    if (pct) gfx_round_rect(SCREEN_TOP, 14, 230, (372 * (int)pct) / 100, 7, 3, GOLD);
}
static void draw_bottom(void) {
    gfx_clear(SCREEN_BOTTOM, BG); gfx_gradient(SCREEN_BOTTOM, 0, 0, 320, 3, BLUE, CYAN);
    text(SCREEN_BOTTOM, 12, 7, WHITE, "COMMAND DECK");
    const char *tabs[5] = {"SHOP", "UPGRADES", "ASCEND", "STATS", "INFO"};
    for (unsigned i=0; i<5; ++i) {
        int x = 6 + (int)i * 63;
        panel(SCREEN_BOTTOM, x, 23, 59, 22, tab == i ? CYAN : PANEL);
        text(SCREEN_BOTTOM, x + 4, 30, tab == i ? BG : MUTED, tabs[i]);
    }
    if (tab == 0) {
        for (unsigned row=0; row<4; ++row) {
            unsigned i = selected + row;
            if (i >= GEN_COUNT) break;
            int y = 50 + (int)row * 38;
            panel(SCREEN_BOTTOM, 8, y, 304, 34, row == 0 ? PANEL2 : PANEL);
            text(SCREEN_BOTTOM, 15, y+3, row == 0 ? CYAN : WHITE, gen_names[i]);
            gfx_print(SCREEN_BOTTOM, 15, y+16, FONT_SMALL, MUTED, "%s x%u", gen_desc[i], g.generators[i]);
            gfx_print(SCREEN_BOTTOM, 248, y+10, FONT_BOLD, GOLD, "%llu", (unsigned long long)generator_cost(i));
        }
        text(SCREEN_BOTTOM, 8, 207, MUTED, "UP/DOWN ITEM  LEFT/RIGHT TAB");
        text(SCREEN_BOTTOM, 8, 221, MUTED, "A BUY  Y COLLECT");
    } else if (tab == 1) {
        for (unsigned row=0; row<4; ++row) {
            unsigned i = selected + row;
            if (i >= UPG_COUNT) break;
            int y = 50 + (int)row * 38;
            panel(SCREEN_BOTTOM, 8, y, 304, 34, row == 0 ? PANEL2 : PANEL);
            text(SCREEN_BOTTOM, 15, y+3, row == 0 ? CYAN : WHITE, upgrade_names[i]);
            gfx_print(SCREEN_BOTTOM, 15, y+16, FONT_SMALL, MUTED, "LV%u %s", g.upgrades[i], upgrade_desc[i]);
            gfx_print(SCREEN_BOTTOM, 248, y+10, FONT_BOLD, GOLD, "%s", g.upgrades[i] >= upgrade_cap[i] ? "MAX" : "BUY");
        }
        text(SCREEN_BOTTOM, 8, 207, MUTED, "UP/DOWN ITEM  LEFT/RIGHT TAB");
        text(SCREEN_BOTTOM, 8, 221, MUTED, "A INSTALL  Y COLLECT");
    } else if (tab == 2) {
        text(SCREEN_BOTTOM, 12, 51, PINK, "ASCENSION // PERMANENT PROGRESS");
        panel(SCREEN_BOTTOM, 8, 64, 304, 39, PANEL2);
        gfx_print(SCREEN_BOTTOM, 17, 70, FONT_REGULAR, WHITE, "Lifetime earned: %llu", (unsigned long long)g.lifetime);
        gfx_print(SCREEN_BOTTOM, 17, 85, FONT_REGULAR, GOLD, "Available AP: %llu   Ascensions: %u", (unsigned long long)g.ascension_points, g.ascensions);
        panel(SCREEN_BOTTOM, 8, 108, 304, 38, selected == 0 ? PINK : PANEL);
        if (selected == 0) panel(SCREEN_BOTTOM, 11, 111, 298, 32, BG);
        text(SCREEN_BOTTOM, 18, 116, selected == 0 ? PINK : WHITE, selected == 0 ? ">> ASCEND & RESET RUN <<" : "ASCEND & RESET RUN");
        text(SCREEN_BOTTOM, 18, 131, MUTED, "Requires 1,000,000 lifetime credits");
        text(SCREEN_BOTTOM, 12, 151, CYAN, "PERMANENT UPGRADES  [UP/DOWN]");
    } else if (tab == 3) {
        text(SCREEN_BOTTOM, 12, 51, CYAN, "GAME STATISTICS");
        gfx_print(SCREEN_BOTTOM, 12, 68, FONT_REGULAR, WHITE, "Lifetime earned: %llu", (unsigned long long)g.lifetime);
        gfx_print(SCREEN_BOTTOM, 12, 84, FONT_REGULAR, WHITE, "Total collected: %llu", (unsigned long long)g.clicks);
        gfx_print(SCREEN_BOTTOM, 12, 100, FONT_REGULAR, WHITE, "Best credits held: %llu", (unsigned long long)g.best);
        gfx_print(SCREEN_BOTTOM, 12, 116, FONT_REGULAR, WHITE, "Ascensions: %u", g.ascensions);
        gfx_print(SCREEN_BOTTOM, 12, 132, FONT_REGULAR, WHITE, "Ascension points: %llu", (unsigned long long)g.ascension_points);
        gfx_print(SCREEN_BOTTOM, 12, 148, FONT_REGULAR, WHITE, "Production/sec: %llu", (unsigned long long)production_per_second());
        gfx_print(SCREEN_BOTTOM, 12, 164, FONT_REGULAR, WHITE, "Generators owned: %u", g.generators[0]+g.generators[1]+g.generators[2]+g.generators[3]+g.generators[4]+g.generators[5]+g.generators[6]+g.generators[7]);
        text(SCREEN_BOTTOM, 8, 221, MUTED, "X SAVE  A SAVE NOW");
    } else {
        text(SCREEN_BOTTOM, 12, 51, CYAN, "ABOUT THIS GAME");
        text(SCREEN_BOTTOM, 12, 72, WHITE, "IDLEC v1.5.c");
        text(SCREEN_BOTTOM, 12, 90, MUTED, "Developer / Discord:");
        text(SCREEN_BOTTOM, 12, 105, WHITE, "taravask.");
        text(SCREEN_BOTTOM, 12, 126, MUTED, "Source repository:");
        text(SCREEN_BOTTOM, 12, 142, CYAN, "github.com/TaraVasque/");
        text(SCREEN_BOTTOM, 12, 156, CYAN, "TextIdle3ds");
        text(SCREEN_BOTTOM, 12, 184, MUTED, "AuroraOS edition");
        text(SCREEN_BOTTOM, 8, 221, MUTED, "A SAVE  Y COLLECT");
    }
    if (tab == 2 || tab == 3 || tab == 4) {
        /* Ascension upgrades are shown as a compact scrollable list on Ascend tab. */
        if (tab == 2) {
            for (unsigned row=0; row<3; ++row) {
                unsigned i = (selected == 0 ? row : (selected - 1 + row)) % ASC_COUNT;
                int y = 166 + (int)row * 18;
                int active = (selected == i + 1);
                panel(SCREEN_BOTTOM, 8, y - 2, 304, 17, active ? PINK : PANEL);
                if (active) panel(SCREEN_BOTTOM, 11, y, 298, 13, BG);
                gfx_print(SCREEN_BOTTOM, 15, y, FONT_SMALL, active ? GOLD : WHITE,
                          "%c %s Lv%u", active ? '>' : ' ', asc_names[i], g.ascension_upgrades[i]);
                gfx_print(SCREEN_BOTTOM, 230, y, FONT_SMALL, active ? CYAN : MUTED, "AP:%llu", (unsigned long long)asc_cost(i));
            }
            text(SCREEN_BOTTOM, 8, 224, MUTED, "UP/DOWN SELECT  A CONFIRM  Y COLLECT");
        }
    }
    if (message_frames) {
        panel(SCREEN_BOTTOM, 8, 231, 304, 8, PANEL2);
        /* Message intentionally has no combo-related text or permanent bottom hint. */
    }
}
static void handle_touch(void) {
    if (!hid_touch_down()) return;
    int x = hid_touch_x(), y = hid_touch_y();
    if (y < 46 && x >= 6 && x < 321) { tab = (unsigned)((x - 6) / 63); if (tab > 4) tab = 4; selected = 0; return; }
    if (y >= 50 && y < 202) {
        unsigned row = (unsigned)((y - 50) / 38);
        if (tab == 0) { unsigned i = selected + row; if (i < GEN_COUNT) { selected = i; buy_generator(i); } }
        else if (tab == 1) { unsigned i = selected + row; if (i < UPG_COUNT) { selected = i; buy_upgrade(i); } }
        else if (tab == 2 && y < 171) ascend();
        else if (tab == 2) buy_asc_upgrade((selected + row) % ASC_COUNT);
        else if (tab == 3) save_game();
        else if (tab == 4) save_game();
    }
}
static void update(void) {
    uint32_t keys = hid_keys_down();
    if (keys & KEY_Y) collect();
    if (keys & KEY_X) save_game();
    if (keys & KEY_A) {
        if (tab == 0) buy_generator(selected);
        else if (tab == 1) buy_upgrade(selected);
        else if (tab == 2) { if (selected == 0) ascend(); else buy_asc_upgrade((selected - 1) % ASC_COUNT); }
        else save_game();
    }
    if (keys & KEY_LEFT) { tab = tab == 0 ? 4 : tab - 1; selected = 0; }
    if (keys & KEY_RIGHT) { tab = (tab + 1) % 5; selected = 0; }
    if (keys & KEY_UP) {
        if (tab == 0 && selected > 0) selected--;
        else if (tab == 1 && selected > 0) selected--;
        else if (tab == 2 && selected > 0) selected--;
    }
    if (keys & KEY_DOWN) {
        if (tab == 0 && selected < GEN_COUNT - 1) selected++;
        else if (tab == 1 && selected < UPG_COUNT - 1) selected++;
        else if (tab == 2 && selected < ASC_COUNT) selected++;
    }
    if (keys & KEY_B) save_game();
    if (keys & KEY_L) save_game();
    if (keys & KEY_R) { tab = (tab + 1) % 5; selected = 0; }
    handle_touch();
    if (++tick >= 60) {
        tick = 0; run_seconds++; g.play_seconds++;
        add_credits(production_per_second());
        if (message_frames) message_frames--;
        if (g.play_seconds % 30 == 0) save_game();
    }
}
int main(void) {
    srand((unsigned)sys_millis()); load_game(); atexit(save_game);
    while (app_loop()) { update(); draw_top(); draw_bottom(); }
    return 0;
}
