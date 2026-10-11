/*
 * IDLEC — AuroraOS C SDK idle game
 *
 * Controls:
 *   Y            collect credits
 *   D-pad        UP/DOWN select item, LEFT/RIGHT (or L/R) change tab
 *   A            buy / install / ascend (ascend asks for confirmation)
 *   X, B         save now
 * Save data: <app dir>/save.dat (v1, v2, v3 and v4 saves are all loaded)
 */
#include <aurora_app.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define SAVE_MAGIC     0x52464F52u
#define APP_VERSION    "1.8.c.b"
#define SAVE_VERSION   4u
#define MAX_COUNT      999999u
#define ASC_LEVEL_CAP  100u
#define ASC_THRESHOLD  1000000ULL
#define AUTOSAVE_SECS  30u
#define MSG_FRAMES     150u
#define CONFIRM_FRAMES 180u

#define WHITE 0xF1F5F9
#define MUTED 0x91A4BD
#define BG    0x08111F
#define PANEL 0x111F32
#define PANEL2 0x172941
#define CYAN  0x55E6D0
#define BLUE  0x5D9CFF
#define GOLD  0xFFD166
#define PINK  0xFF6B9D
#define GREEN 0x7BE495
#define RED   0xFF647C

#define GEN_COUNT 12
#define UPG_COUNT 10
#define ASC_COUNT 8
#define ACH_COUNT 12
#define TAB_COUNT 6
#define TAB_W     52

/* Array sizes used by older save formats (v2 / v3). Never change these. */
#define OLD_GEN_COUNT 8
#define OLD_UPG_COUNT 8
#define OLD_ASC_COUNT 6

#define ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))

enum { TAB_SHOP, TAB_UPGRADES, TAB_ASCEND, TAB_STATS, TAB_GOALS, TAB_INFO };


enum {
    UPG_TAP, UPG_CRIT, UPG_PRODUCTION, UPG_OVERDRIVE,
    UPG_AUTO, UPG_LENS, UPG_CIRCUITS, UPG_REALITY,
    UPG_SYNERGY, UPG_RESONANCE
};

enum {
    ASC_TOUCH, ASC_ENGINE, ASC_MAGNET, ASC_CRIT, ASC_CACHE, ASC_KNOWLEDGE,
    ASC_FRUGAL, ASC_FORTUNE
};

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
    uint32_t achievements;          /* bitmask, bit i = goal i unlocked */
} SaveData;

typedef struct {
    uint32_t magic, version;
    uint64_t credits, lifetime, clicks, best;
    uint32_t click_power;
    uint32_t generators[OLD_GEN_COUNT];
    uint32_t upgrades[OLD_UPG_COUNT];
    uint32_t ascensions;
    uint64_t ascension_points;
    uint32_t ascension_upgrades[OLD_ASC_COUNT];
    uint64_t play_seconds;
} SaveDataV3;

typedef struct {
    uint32_t magic, version;
    uint64_t credits, lifetime, clicks, best;
    uint32_t click_power;
    uint32_t generators[OLD_GEN_COUNT];
    uint32_t upgrades[OLD_UPG_COUNT];
    uint32_t ascensions;
    uint64_t ascension_points;
    uint32_t ascension_upgrades[OLD_ASC_COUNT];
} SaveDataV2;

typedef struct {
    uint32_t magic, version;
    uint64_t credits, lifetime, clicks, best;
    uint32_t click_power;
    uint32_t generators[4];
    uint32_t click_upgrade, crit_upgrade, production_upgrade;
    uint32_t prestige;
} LegacySaveData;

static const char *const gen_names[GEN_COUNT] = {
    "DRONE", "WORKSHOP", "REACTOR", "SINGULARITY",
    "NANOFORGE", "DYSON ARRAY", "STAR EATER", "TIME ENGINE",
    "VOID HARVESTER", "GALAXY WEAVER", "MULTIVERSE GATE", "OMEGA CORE"
};
static const char *const gen_desc[GEN_COUNT] = {
    "Automated taps", "Industrial output", "Quantum output", "Reality output",
    "Self-replicating bots", "Harvests star energy", "Consumes dead stars", "Extracts time",
    "Mines the dark between", "Spins galaxies to cash", "Trades with other worlds", "The final engine"
};
static const uint64_t base_cost[GEN_COUNT] = {
    15, 100, 750, 5000, 30000, 180000, 1200000, 9000000,
    75000000ULL, 600000000ULL, 5000000000ULL, 45000000000ULL
};
static const uint32_t base_rate[GEN_COUNT] = {
    1, 6, 42, 280, 1800, 12000, 85000, 650000,
    5500000u, 48000000u, 420000000u, 3800000000u
};

static const char *const upgrade_names[UPG_COUNT] = {
    "TAP AMPLIFIER", "CRITICAL MATRIX", "PRODUCTION CORE", "OVERDRIVE",
    "AUTO-COLLECTOR", "QUANTUM LENS", "EFFICIENT CIRCUITS", "REALITY ENGINE",
    "SYNERGY LINK", "RESONANCE"
};
static const char *const upgrade_desc[UPG_COUNT] = {
    "+4 tap power / level", "+3% critical chance / level", "+25% production / level",
    "+10% tap power / level", "+5% all gains / level", "+50% critical reward / level",
    "Generators produce +12% / level", "All production +35% / level",
    "+15% production / level", "Collect adds +1% PPS / level"
};
static const uint64_t upgrade_base[UPG_COUNT] = {250, 600, 1200, 2500, 5000, 15000, 30000, 100000, 250000, 500000};
static const uint32_t upgrade_cap[UPG_COUNT]  = {999, 30, 99, 100, 100, 50, 100, 50, 50, 50};

static const char *const asc_names[ASC_COUNT] = {
    "PRIMAL TOUCH", "ETERNAL ENGINE", "POINT MAGNET",
    "CRITICAL SOUL", "STARTER CACHE", "DEEP KNOWLEDGE",
    "FRUGAL MIND", "FORTUNE'S EDGE"
};
/* PRIMAL TOUCH +25% tap, ETERNAL ENGINE +20% prod, POINT MAGNET +10% AP,
 * CRITICAL SOUL +2% crit, STARTER CACHE +1000 start credits,
 * DEEP KNOWLEDGE +5% tap & prod, FRUGAL MIND -1% generator cost (max -50%),
 * FORTUNE'S EDGE +10% critical reward. */
static const uint64_t asc_base[ASC_COUNT] = {1, 1, 2, 2, 3, 4, 3, 5};

static const char *const asc_desc[ASC_COUNT] = {
    "Boosts collect power by 25% per level.",
    "Multiplies all passive income x1.20 per level.",
    "Each ascension awards 10% more AP per level.",
    "+2% critical chance per level (cap 80% total).",
    "Start every run with +1,000 credits per level.",
    "+5% collect power and x1.05 income per level.",
    "Generators cost 1% less per level (max 50%).",
    "Critical hits pay +10% more per level."
};

static const char *const ach_names[ACH_COUNT] = {
    "FIRST STEPS", "BUSY FINGERS", "TAP LEGEND", "MILLIONAIRE",
    "BILLIONAIRE", "TRILLIONAIRE", "REBORN", "SERIAL ASCENDER",
    "FACTORY", "MEGAFACTORY", "TIME LORD", "TOP TIER"
};
static const char *const ach_desc[ACH_COUNT] = {
    "Collect 100 times", "Collect 5,000 times", "Collect 50,000 times", "Hold 1M credits at once",
    "Hold 1B credits at once", "Hold 1T credits at once", "Ascend once", "Ascend 10 times",
    "Own 100 generators", "Own 1,000 generators", "Play for 1 hour", "Own an OMEGA CORE"
};

static const char *const tab_names[TAB_COUNT] = {"SHOP", "UPGR", "ASC", "STATS", "GOALS", "INFO"};

static SaveData g;
static char message[72] = "Press Y to collect credits.";
static unsigned message_frames = MSG_FRAMES;
static unsigned tab = TAB_SHOP, selected = 0;
static unsigned tick = 0;
static unsigned ascend_armed = 0;
static uint64_t cur_pps = 0;
static uint64_t cur_click = 1;

static void notify(const char *s) {
    snprintf(message, sizeof(message), "%s", s);
    message_frames = MSG_FRAMES;
}

static uint64_t sat_mul(uint64_t a, uint64_t b) {
    if (a && b > UINT64_MAX / a) return UINT64_MAX;
    return a * b;
}
static uint64_t sat_add(uint64_t a, uint64_t b) {
    return UINT64_MAX - a < b ? UINT64_MAX : a + b;
}
/* value * (pct/100) applied `levels` times, saturating. */
/* Numbers go in, numbers get progressively more suspicious. */
static uint64_t compound(uint64_t v, unsigned pct, unsigned levels) {
    for (unsigned i = 0; i < levels && v < UINT64_MAX; ++i) v = sat_mul(v, pct) / 100;
    return v;
}

/* 1234567 -> "1.23M". Up to four results can be alive at once. */
static const char *fmt(uint64_t v) {
    static char buf[4][24];
    static unsigned n;
    static const char *const suffix[] = {"K", "M", "B", "T", "Qa", "Qi"};
    char *out = buf[n++ & 3u];
    if (v < 1000000ULL) {
        snprintf(out, 24, "%llu", (unsigned long long)v);
        return out;
    }
    uint64_t div = 1000;
    unsigned s = 0;
    while (s < ARRAY_LEN(suffix) - 1 && v / div >= 1000) { div *= 1000; s++; }
    snprintf(out, 24, "%llu.%02llu%s", (unsigned long long)(v / div),
             (unsigned long long)((v % div) / (div / 100)), suffix[s]);
    return out;
}

/* First visible item of a scrolling list so that `sel` stays on screen. */
static unsigned list_top(unsigned sel, unsigned count, unsigned rows) {
    if (count <= rows || sel < rows) return 0;
    unsigned top = sel - rows + 1;
    return top > count - rows ? count - rows : top;
}

static unsigned total_generators(void) {
    unsigned long long owned = 0;
    for (unsigned i = 0; i < GEN_COUNT; ++i) owned += g.generators[i];
    return owned > 0xFFFFFFFFULL ? 0xFFFFFFFFu : (unsigned)owned;
}

/* ------------------------------------------------------------------ */
/* Goals (achievements). Each unlocked goal gives +1% production and  */
/* +1% collect power.                                                 */
/* ------------------------------------------------------------------ */
static int ach_met(unsigned i) {
    switch (i) {
        case 0:  return g.clicks >= 100;
        case 1:  return g.clicks >= 5000;
        case 2:  return g.clicks >= 50000;
        case 3:  return g.best >= 1000000ULL;
        case 4:  return g.best >= 1000000000ULL;
        case 5:  return g.best >= 1000000000000ULL;
        case 6:  return g.ascensions >= 1;
        case 7:  return g.ascensions >= 10;
        case 8:  return total_generators() >= 100;
        case 9:  return total_generators() >= 1000;
        case 10: return g.play_seconds >= 3600;
        case 11: return g.generators[GEN_COUNT - 1] >= 1;
        default: return 0;
    }
}
static unsigned ach_count(void) {
    unsigned n = 0;
    for (unsigned i = 0; i < ACH_COUNT; ++i) if (g.achievements & (1u << i)) n++;
    return n;
}
static void check_achievements(void) {
    for (unsigned i = 0; i < ACH_COUNT; ++i) {
        if (!(g.achievements & (1u << i)) && ach_met(i)) {
            char buf[72];
            g.achievements |= 1u << i;
            snprintf(buf, sizeof(buf), "GOAL REACHED // %s", ach_names[i]);
            notify(buf);
        }
    }
}

static uint64_t scaled_cost(uint64_t base, uint32_t level, unsigned pct) {
    uint64_t c = base;
    for (uint32_t i = 0; i < level; ++i) {
        if (c > UINT64_MAX / (100 + pct)) return UINT64_MAX;
        c = c * (100 + pct) / 100 + 1;
    }
    return c;
}
static uint64_t generator_cost(unsigned i) {
    uint64_t c = scaled_cost(base_cost[i], g.generators[i], 35);
    unsigned off = g.ascension_upgrades[ASC_FRUGAL];
    if (off > 50) off = 50;
    if (c == UINT64_MAX || !off) return c;
    c = c > UINT64_MAX / 100 ? c / 100 * (100 - off) : c * (100 - off) / 100;
    return c ? c : 1;
}
static uint64_t upgrade_cost(unsigned i) {
    if (g.upgrades[i] >= upgrade_cap[i]) return UINT64_MAX;
    return scaled_cost(upgrade_base[i], g.upgrades[i], 55);
}
static uint64_t asc_cost(unsigned i) {
    uint32_t level = g.ascension_upgrades[i];
    if (level >= ASC_LEVEL_CAP) return UINT64_MAX;
    return asc_base[i] + (uint64_t)level * (asc_base[i] + 1);
}
static uint64_t ascend_gain(void) {
    uint64_t pts = sat_mul(g.lifetime / ASC_THRESHOLD, 100 + g.ascension_upgrades[ASC_MAGNET] * 10) / 100;
    return pts ? pts : 1;
}

static uint64_t calc_click_power(void) {
    uint64_t p = g.click_power + (uint64_t)g.upgrades[UPG_TAP] * 4;
    p = p * (100 + g.upgrades[UPG_OVERDRIVE] * 10) / 100;
    p = p * (100 + g.ascension_upgrades[ASC_TOUCH] * 25) / 100;
    p = p * (100 + g.ascension_upgrades[ASC_KNOWLEDGE] * 5) / 100;
    p = p * (100 + ach_count()) / 100;
    return p ? p : 1;
}
static uint64_t calc_production(void) {
    uint64_t total = 0;
    for (unsigned i = 0; i < GEN_COUNT; ++i) {
        uint64_t rate = sat_mul(g.generators[i], base_rate[i]);
        rate = compound(rate, 125, g.upgrades[UPG_PRODUCTION]);
        rate = compound(rate, 105, g.upgrades[UPG_AUTO]);
        rate = compound(rate, 112, g.upgrades[UPG_CIRCUITS]);
        rate = compound(rate, 135, g.upgrades[UPG_REALITY]);
        rate = compound(rate, 115, g.upgrades[UPG_SYNERGY]);
        total = sat_add(total, rate);
    }
    total = compound(total, 120, g.ascension_upgrades[ASC_ENGINE]);
    total = compound(total, 105, g.ascension_upgrades[ASC_KNOWLEDGE]);
    total = sat_mul(total, 100 + ach_count()) / 100;
    return total;
}
/* Recount the space nonsense before the screens start lying. */
static void refresh_stats(void) {
    cur_pps = calc_production();
    cur_click = calc_click_power();
}

/* The money machine yearns for more money. */
static void add_credits(uint64_t amount) {
    g.credits = sat_add(g.credits, amount);
    g.lifetime = sat_add(g.lifetime, amount);
    if (g.credits > g.best) g.best = g.credits;
}

static void save_path(char *out, size_t n) { snprintf(out, n, "%s/save.dat", app_dir()); }

static int save_game(int quiet) {
    char path[300];
    save_path(path, sizeof(path));
    FILE *f = fopen(path, "wb");
    int ok = 0;
    if (f) {
        g.magic = SAVE_MAGIC;
        g.version = SAVE_VERSION;
        ok = fwrite(&g, sizeof(g), 1, f) == 1;
        if (fclose(f) != 0) ok = 0;
    }
    if (!quiet) notify(ok ? "SAVE COMPLETE // progress secured" : "SAVE FAILED // storage unavailable");
        return ok;
}
static void save_on_exit(void) { save_game(1); }

static void reset_game(void) {
    memset(&g, 0, sizeof(g));
    g.magic = SAVE_MAGIC;
    g.version = SAVE_VERSION;
    g.click_power = 1;
}

static void sanitize(void) {
    if (!g.click_power) g.click_power = 1;
    for (unsigned i = 0; i < GEN_COUNT; ++i) if (g.generators[i] > MAX_COUNT) g.generators[i] = MAX_COUNT;
    for (unsigned i = 0; i < UPG_COUNT; ++i) if (g.upgrades[i] > upgrade_cap[i]) g.upgrades[i] = upgrade_cap[i];
    for (unsigned i = 0; i < ASC_COUNT; ++i) if (g.ascension_upgrades[i] > ASC_LEVEL_CAP) g.ascension_upgrades[i] = ASC_LEVEL_CAP;
    g.achievements &= (1u << ACH_COUNT) - 1u;
}
static int try_read(FILE *f, void *buf, size_t size) {
    rewind(f);
    return fread(buf, 1, size, f) == size;
}

static void load_game(void) {
    char path[300];
    save_path(path, sizeof(path));
    FILE *f = fopen(path, "rb");
    if (!f) { reset_game(); return; }

    SaveData cur;
    SaveDataV3 v3;
    SaveDataV2 v2;
    LegacySaveData v1;

    if (try_read(f, &cur, sizeof(cur)) && cur.magic == SAVE_MAGIC && cur.version == SAVE_VERSION) {
        g = cur;
        notify("SAVE RESTORED // welcome back");
    } else if (try_read(f, &v3, sizeof(v3)) && v3.magic == SAVE_MAGIC && v3.version == 3) {
        reset_game();
        g.credits = v3.credits; g.lifetime = v3.lifetime; g.clicks = v3.clicks; g.best = v3.best;
        g.click_power = v3.click_power;
        memcpy(g.generators, v3.generators, sizeof(v3.generators));
        memcpy(g.upgrades, v3.upgrades, sizeof(v3.upgrades));
        g.ascensions = v3.ascensions;
        g.ascension_points = v3.ascension_points;
        memcpy(g.ascension_upgrades, v3.ascension_upgrades, sizeof(v3.ascension_upgrades));
        g.play_seconds = v3.play_seconds;
        notify("SAVE UPDATED // new content unlocked");
    } else if (try_read(f, &v2, sizeof(v2)) && v2.magic == SAVE_MAGIC && v2.version == 2) {
        reset_game();
        g.credits = v2.credits; g.lifetime = v2.lifetime; g.clicks = v2.clicks; g.best = v2.best;
        g.click_power = v2.click_power;
        memcpy(g.generators, v2.generators, sizeof(v2.generators));
        memcpy(g.upgrades, v2.upgrades, sizeof(v2.upgrades));
        g.ascensions = v2.ascensions;
        g.ascension_points = v2.ascension_points;
        memcpy(g.ascension_upgrades, v2.ascension_upgrades, sizeof(v2.ascension_upgrades));
        notify("SAVE UPDATED // playtime tracking enabled");
    } else if (try_read(f, &v1, sizeof(v1)) && v1.magic == SAVE_MAGIC && v1.version == 1) {
        reset_game();
        g.credits = v1.credits; g.lifetime = v1.lifetime; g.clicks = v1.clicks; g.best = v1.best;
        g.click_power = v1.click_power;
        memcpy(g.generators, v1.generators, sizeof(v1.generators));
        g.upgrades[UPG_TAP] = v1.click_upgrade;
        g.upgrades[UPG_CRIT] = v1.crit_upgrade;
        g.upgrades[UPG_PRODUCTION] = v1.production_upgrade;
        g.ascensions = v1.prestige;
        notify("OLD SAVE CONVERTED // expanded systems online");
    } else {
        reset_game();
    }
    fclose(f);
    sanitize();
}

static unsigned crit_chance(void) {
    unsigned c = 5 + g.upgrades[UPG_CRIT] * 3 + g.ascension_upgrades[ASC_CRIT] * 2;
    return c > 80 ? 80 : c;
}

static void collect(void) {
    uint64_t amount = compound(cur_click, 105, g.upgrades[UPG_AUTO]);
    /* Resonance: each collect also carries a slice of passive income. */
    amount = sat_add(amount, sat_mul(cur_pps, g.upgrades[UPG_RESONANCE]) / 100);
    if ((unsigned)(rand() % 100) < crit_chance()) {
        /* x5 base, +50% of that per Quantum Lens level, +10% per Fortune's Edge level */
        unsigned bonus = g.upgrades[UPG_LENS] * 50 + g.ascension_upgrades[ASC_FORTUNE] * 10;
        amount = sat_mul(amount, 5ULL * (100 + bonus)) / 100;
        notify("CRITICAL HARVEST // big credits!");
    } else {
        notify("CORE HARVESTED // credits collected");
    }
    add_credits(amount);
    g.clicks++;
}

static void buy_generator(unsigned i) {
    if (i >= GEN_COUNT) return;
    if (g.generators[i] >= MAX_COUNT) { notify("GENERATOR LIMIT REACHED"); return; }
    uint64_t cost = generator_cost(i);
    if (g.credits < cost) { notify("INSUFFICIENT CREDITS"); return; }
    g.credits -= cost;
    g.generators[i]++;
    notify("GENERATOR ONLINE // production increased");
}
static void buy_upgrade(unsigned i) {
    if (i >= UPG_COUNT || g.upgrades[i] >= upgrade_cap[i]) { notify("UPGRADE AT MAX LEVEL"); return; }
    uint64_t cost = upgrade_cost(i);
    if (g.credits < cost) { notify("NOT ENOUGH CREDITS FOR THIS UPGRADE"); return; }
    g.credits -= cost;
    g.upgrades[i]++;
    notify("UPGRADE INSTALLED // systems improved");
}
static void buy_asc_upgrade(unsigned i) {
    if (i >= ASC_COUNT) return;
    uint64_t cost = asc_cost(i);
    if (g.ascension_upgrades[i] >= ASC_LEVEL_CAP) { notify("UPGRADE AT MAX LEVEL"); return; }
    if (g.ascension_points < cost) { notify("NOT ENOUGH ASCENSION POINTS"); return; }
    g.ascension_points -= cost;
    g.ascension_upgrades[i]++;
    notify("PERMANENT UPGRADE UNLOCKED");
}

static void do_ascend(void) {
    g.ascension_points = sat_add(g.ascension_points, ascend_gain());
    g.ascensions++;
    g.credits = 1000ULL * g.ascension_upgrades[ASC_CACHE];
    memset(g.generators, 0, sizeof(g.generators));
    memset(g.upgrades, 0, sizeof(g.upgrades));
    g.click_power = 1 + g.ascensions;
    g.lifetime = 0;
    ascend_armed = 0;
    save_game(1);
    notify("ASCENSION COMPLETE // permanent points awarded");
}

static void request_ascend(void) {
    if (g.lifetime < ASC_THRESHOLD) { notify("ASCENSION LOCKED // need 1,000,000 lifetime credits"); return; }
        if (!ascend_armed) {
            ascend_armed = CONFIRM_FRAMES;
            notify("PRESS AGAIN TO CONFIRM // this resets your run");
            return;
        }
        do_ascend();
}

static void panel(int screen, int x, int y, int w, int h, int color) {
    gfx_round_rect(screen, x, y, w, h, 7, color);
}

static void outline(int screen, int x, int y, int w, int h, int color) {
    panel(screen, x, y, w, h, color);
    panel(screen, x + 3, y + 3, w - 6, h - 6, BG);
}

static void draw_top(void) {
    gfx_clear(SCREEN_TOP, BG);
    gfx_gradient(SCREEN_TOP, 0, 0, 400, 4, CYAN, BLUE);
    gfx_text(SCREEN_TOP, 16, 12, CYAN, "IDLEC");
    gfx_text(SCREEN_TOP, 304, 14, MUTED, "v" APP_VERSION);
    gfx_print(SCREEN_TOP, 145, 13, FONT_SMALL, GOLD, "PLAYTIME %02llu:%02llu:%02llu",
              (unsigned long long)(g.play_seconds / 3600),
              (unsigned long long)((g.play_seconds / 60) % 60),
              (unsigned long long)(g.play_seconds % 60));

    panel(SCREEN_TOP, 14, 36, 372, 57, PANEL);
    gfx_text(SCREEN_TOP, 26, 45, MUTED, "AVAILABLE CREDITS");
    gfx_print(SCREEN_TOP, 26, 61, FONT_TITLE, WHITE, "%s", fmt(g.credits));

    panel(SCREEN_TOP, 14, 101, 179, 39, PANEL);
    gfx_text(SCREEN_TOP, 24, 108, MUTED, "PASSIVE / SEC");
    gfx_print(SCREEN_TOP, 24, 121, FONT_BOLD, GREEN, "%s", fmt(cur_pps));

    panel(SCREEN_TOP, 201, 101, 185, 39, PANEL);
    gfx_text(SCREEN_TOP, 211, 108, MUTED, "COLLECT POWER");
    gfx_print(SCREEN_TOP, 211, 121, FONT_BOLD, GOLD, "%s", fmt(cur_click));

    panel(SCREEN_TOP, 14, 151, 372, 70, PANEL2);
    gfx_circle(SCREEN_TOP, 54, 186, 22, CYAN);
    gfx_circle(SCREEN_TOP, 54, 186, 14, BG);
    gfx_circle(SCREEN_TOP, 54, 186, 6, CYAN);
    gfx_text(SCREEN_TOP, 86, 169, WHITE, "COLLECT CREDITS");
    gfx_text(SCREEN_TOP, 86, 184, CYAN, "PRESS Y");
    gfx_print(SCREEN_TOP, 86, 198, FONT_SMALL, MUTED, "Crit chance: %u%%", crit_chance());


    panel(SCREEN_TOP, 14, 230, 372, 7, PANEL);
    unsigned pct = g.lifetime >= ASC_THRESHOLD ? 100 : (unsigned)(g.lifetime / (ASC_THRESHOLD / 100));
    if (pct) gfx_round_rect(SCREEN_TOP, 14, 230, (372 * (int)pct) / 100, 7, 3, GOLD);
}

static void draw_tabs(void) {
    for (unsigned i = 0; i < TAB_COUNT; ++i) {
        int x = 4 + (int)i * TAB_W;
        panel(SCREEN_BOTTOM, x, 23, TAB_W - 3, 22, tab == i ? CYAN : PANEL);
        gfx_text(SCREEN_BOTTOM, x + 4, 30, tab == i ? BG : MUTED, tab_names[i]);
    }
}

#define LIST_ROWS 4
static void draw_shop(void) {
    unsigned top = list_top(selected, GEN_COUNT, LIST_ROWS);
    for (unsigned row = 0; row < LIST_ROWS && top + row < GEN_COUNT; ++row) {
        unsigned i = top + row;
        int y = 50 + (int)row * 38;
        int active = (i == selected);
        uint64_t cost = generator_cost(i);
        panel(SCREEN_BOTTOM, 8, y, 304, 34, active ? PANEL2 : PANEL);
        gfx_text(SCREEN_BOTTOM, 15, y + 3, active ? CYAN : WHITE, gen_names[i]);
        gfx_print(SCREEN_BOTTOM, 15, y + 16, FONT_SMALL, MUTED, "%s x%u", gen_desc[i], g.generators[i]);
        gfx_print(SCREEN_BOTTOM, 236, y + 10, FONT_BOLD, g.credits >= cost ? GOLD : RED, "%s", fmt(cost));
    }
    gfx_text(SCREEN_BOTTOM, 8, 207, MUTED, "UP/DOWN ITEM  LEFT/RIGHT TAB");
    gfx_text(SCREEN_BOTTOM, 8, 219, MUTED, "A BUY  Y COLLECT");
}

static void draw_upgrades(void) {
    unsigned top = list_top(selected, UPG_COUNT, LIST_ROWS);
    for (unsigned row = 0; row < LIST_ROWS && top + row < UPG_COUNT; ++row) {
        unsigned i = top + row;
        int y = 50 + (int)row * 38;
        int active = (i == selected);
        int maxed = g.upgrades[i] >= upgrade_cap[i];
        uint64_t cost = upgrade_cost(i);
        panel(SCREEN_BOTTOM, 8, y, 304, 34, active ? PANEL2 : PANEL);
        gfx_text(SCREEN_BOTTOM, 15, y + 3, active ? CYAN : WHITE, upgrade_names[i]);
        gfx_print(SCREEN_BOTTOM, 15, y + 16, FONT_SMALL, MUTED, "LV%u %s", g.upgrades[i], upgrade_desc[i]);
        gfx_print(SCREEN_BOTTOM, 236, y + 10, FONT_BOLD,
                  maxed ? GREEN : (g.credits >= cost ? GOLD : RED), "%s", maxed ? "MAX" : fmt(cost));
    }
    gfx_text(SCREEN_BOTTOM, 8, 207, MUTED, "UP/DOWN ITEM  LEFT/RIGHT TAB");
    gfx_text(SCREEN_BOTTOM, 8, 219, MUTED, "A INSTALL  Y COLLECT");
}

/* Describes what an ascension upgrade gives at a given level. */
static void asc_effect(unsigned i, unsigned lv, char *out, size_t n) {
    if (lv > ASC_LEVEL_CAP) lv = ASC_LEVEL_CAP;
    switch (i) {
        case ASC_TOUCH:     snprintf(out, n, "+%u%% collect power", lv * 25); break;
        case ASC_ENGINE:    snprintf(out, n, "+%s%% income", fmt((compound(10000, 120, lv) - 10000) / 100)); break;
        case ASC_MAGNET:    snprintf(out, n, "+%u%% AP per ascension", lv * 10); break;
        case ASC_CRIT:      snprintf(out, n, "+%u%% crit chance", lv * 2); break;
        case ASC_CACHE:     snprintf(out, n, "%s starting credits", fmt(1000ULL * lv)); break;
        case ASC_KNOWLEDGE: snprintf(out, n, "+%u%% tap, +%s%% income", lv * 5,
            fmt((compound(10000, 105, lv) - 10000) / 100)); break;
        case ASC_FRUGAL:    snprintf(out, n, "-%u%% generator cost", lv > 50 ? 50 : lv); break;
        case ASC_FORTUNE:   snprintf(out, n, "+%u%% crit reward", lv * 10); break;
        default:            snprintf(out, n, "-"); break;
    }
}

/* Layout of the ASCEND tab (bottom screen). */
#define ASC_BTN_Y   75
#define ASC_BTN_H   27
#define ASC_ROW_Y   118
#define ASC_ROW_STEP 23
#define ASC_ROW_H   21
#define ASC_ROWS    3

static void draw_ascend(void) {
    int ready = g.lifetime >= ASC_THRESHOLD;

    /* Summary + progress toward the next ascension */
    panel(SCREEN_BOTTOM, 8, 50, 304, 22, PANEL2);
    gfx_print(SCREEN_BOTTOM, 15, 54, FONT_SMALL, GOLD, "AP: %s", fmt(g.ascension_points));
    gfx_print(SCREEN_BOTTOM, 150, 54, FONT_SMALL, WHITE, "Ascensions: %u", g.ascensions);
    panel(SCREEN_BOTTOM, 15, 65, 290, 4, BG);
    unsigned pct = ready ? 100 : (unsigned)(g.lifetime / (ASC_THRESHOLD / 100));
    if (pct) gfx_round_rect(SCREEN_BOTTOM, 15, 65, (290 * (int)pct) / 100, 4, 2, ready ? GREEN : CYAN);

    /* Ascend button */
    if (selected == 0) outline(SCREEN_BOTTOM, 8, ASC_BTN_Y, 304, ASC_BTN_H, PINK);
    else panel(SCREEN_BOTTOM, 8, ASC_BTN_Y, 304, ASC_BTN_H, PANEL);
    gfx_text(SCREEN_BOTTOM, 18, ASC_BTN_Y + 5, selected == 0 ? PINK : WHITE,
             ascend_armed ? "!! PRESS AGAIN TO CONFIRM !!"
             : (selected == 0 ? ">> ASCEND & RESET RUN <<" : "ASCEND & RESET RUN"));
    if (ready) gfx_print(SCREEN_BOTTOM, 18, ASC_BTN_Y + 16, FONT_SMALL, GREEN, "Ready: gain +%s AP", fmt(ascend_gain()));
    else gfx_print(SCREEN_BOTTOM, 18, ASC_BTN_Y + 16, FONT_SMALL, MUTED, "Need %s lifetime credits (%s)",
        fmt(ASC_THRESHOLD), fmt(g.lifetime));

    /* Permanent upgrade list */
    gfx_text(SCREEN_BOTTOM, 12, 106, CYAN, "PERMANENT UPGRADES");
    if (selected > 0) gfx_print(SCREEN_BOTTOM, 262, 107, FONT_SMALL, MUTED, "%u/%u", selected, (unsigned)ASC_COUNT);

    unsigned sel = selected ? selected - 1 : 0;
    unsigned top = list_top(sel, ASC_COUNT, ASC_ROWS);
    for (unsigned row = 0; row < ASC_ROWS && top + row < ASC_COUNT; ++row) {
        unsigned i = top + row;
        int y = ASC_ROW_Y + (int)row * ASC_ROW_STEP;
        int active = (selected == i + 1);
        int maxed = g.ascension_upgrades[i] >= ASC_LEVEL_CAP;
        uint64_t cost = asc_cost(i);
        if (active) outline(SCREEN_BOTTOM, 8, y, 304, ASC_ROW_H, PINK);
        else panel(SCREEN_BOTTOM, 8, y, 304, ASC_ROW_H, PANEL);
        gfx_print(SCREEN_BOTTOM, 15, y + 7, FONT_SMALL, active ? GOLD : WHITE,
                  "%s  Lv%u", asc_names[i], g.ascension_upgrades[i]);
        if (maxed) gfx_text(SCREEN_BOTTOM, 250, y + 7, GREEN, "MAX");
        else gfx_print(SCREEN_BOTTOM, 238, y + 7, FONT_SMALL,
            g.ascension_points >= cost ? GREEN : RED, "AP: %s", fmt(cost));
    }

    /* Detail box: explains whatever is selected */
    panel(SCREEN_BOTTOM, 8, 188, 304, 39, PANEL2);
    if (selected == 0) {
        gfx_text(SCREEN_BOTTOM, 15, 191, WHITE, "Ascending resets your run.");
        gfx_print(SCREEN_BOTTOM, 15, 202, FONT_SMALL, RED, "Lost: credits, generators, upgrades");
        gfx_print(SCREEN_BOTTOM, 15, 213, FONT_SMALL, GREEN, "Kept: AP, permanent upgrades, goals");
    } else {
        unsigned i = selected - 1;
        unsigned lv = g.ascension_upgrades[i];
        char now[48], next[48];
        asc_effect(i, lv, now, sizeof(now));
        if (lv >= ASC_LEVEL_CAP) snprintf(next, sizeof(next), "MAX LEVEL");
        else asc_effect(i, lv + 1, next, sizeof(next));
        gfx_print(SCREEN_BOTTOM, 15, 191, FONT_SMALL, WHITE, "%s", asc_desc[i]);
        gfx_print(SCREEN_BOTTOM, 15, 202, FONT_SMALL, MUTED, "Now:  %s", lv ? now : "none");
        gfx_print(SCREEN_BOTTOM, 15, 213, FONT_SMALL, GOLD, "Next: %s", next);
    }
}

static void draw_stats(void) {
    gfx_text(SCREEN_BOTTOM, 12, 51, CYAN, "GAME STATISTICS");
    gfx_print(SCREEN_BOTTOM, 12, 68,  FONT_REGULAR, WHITE, "Lifetime earned: %s", fmt(g.lifetime));
    gfx_print(SCREEN_BOTTOM, 12, 84,  FONT_REGULAR, WHITE, "Total collected: %s", fmt(g.clicks));
    gfx_print(SCREEN_BOTTOM, 12, 100, FONT_REGULAR, WHITE, "Best credits held: %s", fmt(g.best));
    gfx_print(SCREEN_BOTTOM, 12, 116, FONT_REGULAR, WHITE, "Ascensions: %u", g.ascensions);
    gfx_print(SCREEN_BOTTOM, 12, 132, FONT_REGULAR, WHITE, "Ascension points: %s", fmt(g.ascension_points));
    gfx_print(SCREEN_BOTTOM, 12, 148, FONT_REGULAR, WHITE, "Production/sec: %s", fmt(cur_pps));
    gfx_print(SCREEN_BOTTOM, 12, 164, FONT_REGULAR, WHITE, "Generators owned: %u", total_generators());
    gfx_print(SCREEN_BOTTOM, 12, 180, FONT_REGULAR, WHITE, "Goals: %u/%u", ach_count(), (unsigned)ACH_COUNT);
    gfx_text(SCREEN_BOTTOM, 8, 219, MUTED, "A SAVE NOW  Y COLLECT");
}

static void draw_goals(void) {
    gfx_print(SCREEN_BOTTOM, 12, 51, FONT_REGULAR, GOLD, "GOALS %u/%u  BONUS +%u%%",
              ach_count(), (unsigned)ACH_COUNT, ach_count());
    unsigned top = list_top(selected, ACH_COUNT, LIST_ROWS);
    for (unsigned row = 0; row < LIST_ROWS && top + row < ACH_COUNT; ++row) {
        unsigned i = top + row;
        int y = 64 + (int)row * 38;
        int done = (g.achievements & (1u << i)) != 0;
        panel(SCREEN_BOTTOM, 8, y, 304, 34, i == selected ? PANEL2 : PANEL);
        gfx_text(SCREEN_BOTTOM, 15, y + 3, done ? GREEN : WHITE, ach_names[i]);
        gfx_text(SCREEN_BOTTOM, 15, y + 18, MUTED, ach_desc[i]);
        gfx_text(SCREEN_BOTTOM, 252, y + 10, done ? GREEN : MUTED, done ? "DONE" : "LOCKED");
    }
    gfx_text(SCREEN_BOTTOM, 8, 219, MUTED, "UP/DOWN BROWSE  +1% ALL / GOAL");
}

static void draw_info(void) {
    gfx_text(SCREEN_BOTTOM, 12, 51, CYAN, "ABOUT THIS GAME");
    gfx_text(SCREEN_BOTTOM, 12, 72, WHITE, "IDLEC v" APP_VERSION);
    gfx_text(SCREEN_BOTTOM, 12, 90, MUTED, "Developer / Discord:");
    gfx_text(SCREEN_BOTTOM, 12, 105, WHITE, "taravask.");
    gfx_text(SCREEN_BOTTOM, 12, 126, MUTED, "Source repository:");
    gfx_text(SCREEN_BOTTOM, 12, 142, CYAN, "github.com/TaraVasque/");
    gfx_text(SCREEN_BOTTOM, 12, 156, CYAN, "TextIdle3ds");
    gfx_text(SCREEN_BOTTOM, 12, 184, MUTED, "AuroraOS edition");
}

static void draw_bottom(void) {
    gfx_clear(SCREEN_BOTTOM, BG);
    gfx_gradient(SCREEN_BOTTOM, 0, 0, 320, 3, BLUE, CYAN);
    gfx_text(SCREEN_BOTTOM, 12, 7, WHITE, "COMMAND DECK");
    draw_tabs();

    switch (tab) {
        case TAB_SHOP:     draw_shop();     break;
        case TAB_UPGRADES: draw_upgrades(); break;
        case TAB_ASCEND:   draw_ascend();   break;
        case TAB_STATS:    draw_stats();    break;
        case TAB_GOALS:    draw_goals();    break;
        default:           draw_info();     break;
    }

    if (message_frames) {
        panel(SCREEN_BOTTOM, 8, 229, 304, 11, PANEL2);
        gfx_print(SCREEN_BOTTOM, 14, 231, FONT_SMALL, GOLD, "%s", message);
    }
}

static unsigned item_count(void) {
    switch (tab) {
        case TAB_SHOP:     return GEN_COUNT;
        case TAB_UPGRADES: return UPG_COUNT;
        case TAB_ASCEND:   return ASC_COUNT + 1;   /* ascend button + upgrades */
        case TAB_GOALS:    return ACH_COUNT;
        default:           return 1;
    }
}
static void set_tab(unsigned t) {
    tab = t % TAB_COUNT;
    selected = 0;
    ascend_armed = 0;
}
static void activate(void) {
    switch (tab) {
        case TAB_SHOP:     buy_generator(selected); break;
        case TAB_UPGRADES: buy_upgrade(selected);   break;
        case TAB_ASCEND:   if (selected == 0) request_ascend(); else buy_asc_upgrade(selected - 1); break;
        default:           save_game(0); break;
    }
}

static void handle_touch(void) {
    if (!hid_touch_down()) return;
    int x = hid_touch_x(), y = hid_touch_y();

    if (y < 46) {
        if (x >= 4 && x < 4 + TAB_COUNT * TAB_W) set_tab((unsigned)((x - 4) / TAB_W));
        return;
    }
    if (tab == TAB_SHOP || tab == TAB_UPGRADES) {
        if (y < 50 || y >= 202) return;
        unsigned count = item_count();
        unsigned i = list_top(selected, count, LIST_ROWS) + (unsigned)((y - 50) / 38);
        if (i >= count) return;
        selected = i;
        activate();
    } else if (tab == TAB_GOALS) {
        if (y < 64 || y >= 216) return;
        unsigned i = list_top(selected, ACH_COUNT, LIST_ROWS) + (unsigned)((y - 64) / 38);
        if (i < ACH_COUNT) selected = i;
    } else if (tab == TAB_ASCEND) {
        if (y >= ASC_BTN_Y && y < ASC_BTN_Y + ASC_BTN_H) {
            selected = 0;
            request_ascend();
        } else if (y >= ASC_ROW_Y && y < ASC_ROW_Y + ASC_ROWS * ASC_ROW_STEP) {
            unsigned sel = selected ? selected - 1 : 0;
            unsigned i = list_top(sel, ASC_COUNT, ASC_ROWS) + (unsigned)((y - ASC_ROW_Y) / ASC_ROW_STEP);
            if (i < ASC_COUNT) { selected = i + 1; buy_asc_upgrade(i); }
        }
    } else if (y >= 50) {
        save_game(0);
    }
}

/* Ask the tiny buttons what kind of chaos they want today. */
static void handle_keys(void) {
    uint32_t keys = hid_keys_down();
    if (keys & KEY_Y) collect();
    if (keys & KEY_A) activate();
    if (keys & (KEY_X | KEY_B )) save_game(0);

    /* Bumpers first. Two directions at once is how the tabs enter orbit. */
    if (keys & KEY_R) {
        set_tab(tab + 1);
    } else if (keys & KEY_L) {
        set_tab(tab + TAB_COUNT - 1);
    } else if (keys & KEY_RIGHT) {
        set_tab(tab + 1);
    } else if (keys & KEY_LEFT) {
        set_tab(tab + TAB_COUNT - 1);
    }
    if ((keys & KEY_UP) && selected > 0) { selected--; ascend_armed = 0; }
    if ((keys & KEY_DOWN) && selected + 1 < item_count()) { selected++; ascend_armed = 0; }
}

static void update(void) {
    handle_keys();
    handle_touch();
    refresh_stats();


    if (message_frames) message_frames--;
    if (ascend_armed && --ascend_armed == 0) notify("ASCENSION CANCELLED");


    if (++tick >= 60) {
        tick = 0;
        g.play_seconds++;
        add_credits(cur_pps);
        check_achievements();
        if (g.play_seconds % AUTOSAVE_SECS == 0) save_game(1);
    }
}

int main(void) {
    srand((unsigned)sys_millis());
    load_game();
    atexit(save_on_exit);
    refresh_stats();
    while (app_loop()) {
        update();
        draw_top();
        draw_bottom();
    }
    return 0;
}
