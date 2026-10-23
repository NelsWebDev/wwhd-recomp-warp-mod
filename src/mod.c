/* Map warp: hold ZL+ZR and press Minus to open a destination list (Great Sea islands and a few
 * dungeons). D-pad up/down chooses, left/right jumps a page, A warps, B closes.
 * Calls the game's own dComIfGp_setNextStage, so the game does the fade and scene change. */
#include "wwhd_guest.h"
#include "wwhd/bindings.h"

#define BTN_MINUS 0x0004u
#define BTN_X     0x2000u
#define BTN_A     0x8000u
#define BTN_B     0x4000u
#define BTN_ZR    0x0040u
#define BTN_ZL    0x0080u
#define BTN_DOWN  0x0100u
#define BTN_UP    0x0200u
#define BTN_RIGHT 0x0400u
#define BTN_LEFT  0x0800u

typedef struct {
    const char* name;
    const char* stage;
    u32 room;
} dest;

#define SEA(n, label) { label, "sea", n }
static const dest dests[] = {
    SEA(44, "Outset Island"),
    SEA(11, "Windfall Island"),
    SEA(13, "Dragon Roost Island"),
    SEA(41, "Forest Haven"),
    SEA(1,  "Forsaken Fortress"),
    SEA(2,  "Star Island"),
    SEA(3,  "Northern Fairy Island"),
    SEA(4,  "Gale Isle"),
    SEA(5,  "Crescent Moon Island"),
    SEA(6,  "Seven-Star Isles"),
    SEA(7,  "Overlook Island"),
    SEA(8,  "Four-Eye Reef"),
    SEA(9,  "Mother & Child Isles"),
    SEA(10, "Spectacle Island"),
    SEA(12, "Pawprint Isle"),
    SEA(14, "Flight Control Platform"),
    SEA(15, "Western Fairy Island"),
    SEA(16, "Rock Spire Isle"),
    SEA(17, "Tingle Island"),
    SEA(18, "Northern Triangle Island"),
    SEA(19, "Eastern Fairy Island"),
    SEA(20, "Fire Mountain"),
    SEA(21, "Star Belt Archipelago"),
    SEA(22, "Three-Eye Reef"),
    SEA(23, "Greatfish Isle"),
    SEA(24, "Cyclops Reef"),
    SEA(25, "Six-Eye Reef"),
    SEA(26, "Tower of the Gods (sea)"),
    SEA(27, "Eastern Triangle Island"),
    SEA(28, "Thorned Fairy Island"),
    SEA(29, "Needle Rock Isle"),
    SEA(30, "Islet of Steel"),
    SEA(31, "Stone Watcher Island"),
    SEA(32, "Southern Triangle Island"),
    SEA(33, "Private Oasis"),
    SEA(34, "Bomb Island"),
    SEA(35, "Bird's Peak Rock"),
    SEA(36, "Diamond Steppe Island"),
    SEA(37, "Five-Eye Reef"),
    SEA(38, "Shark Island"),
    SEA(39, "Southern Fairy Island"),
    SEA(40, "Ice Ring Isle"),
    SEA(42, "Cliff Plateau Isles"),
    SEA(43, "Horseshoe Island"),
    SEA(45, "Headstone Island"),
    SEA(46, "Two-Eye Reef"),
    SEA(47, "Angular Isles"),
    SEA(48, "Boating Course"),
    SEA(49, "Five-Star Isles"),
    { "Dragon Roost Cavern", "M_NewD2", 0 },
    { "Forbidden Woods",     "kindan",  0 },
    { "Tower of the Gods",   "Siren",   0 },
    { "Earth Temple",        "M_Dai",   0 },
    { "Wind Temple",         "kaze",    0 },
};
#define COUNT ((int)(sizeof(dests) / sizeof(dests[0])))
#define ROWS 12

static int open, cursor, scroll, prev_buttons, pending;
static int held_frames, blocking;
static int grid_mode, gcol, grow;  /* square picker: column A-G, row 1-7 */
static dest target;
static char square_text[64];
static char row_text[ROWS][48];

/* Sea rooms are numbered along the chart grid: room 1 = A1, 7 = G1, 8 = A2 ... 49 = G7. */
static int is_sea(const dest* d) { return d->stage[0] == 's' && d->stage[1] == 'e' && d->stage[2] == 'a' && !d->stage[3]; }
static void square_label(char* out, u32 room) {
    out[0] = (char)('A' + (room - 1) % 7);
    out[1] = (char)('1' + (room - 1) / 7);
    out[2] = 0;
}
static void copy(char* out, u32 cap, const char* a, const char* b, const char* c) {
    u32 n = 0;
    const char* parts[3] = { a, b, c };
    for (int p = 0; p < 3; p++)
        for (const char* q = parts[p]; *q && n + 1 < cap; q++) out[n++] = *q;
    out[n] = 0;
}
static const dest* find_square(u32 room) {
    for (int i = 0; i < COUNT; i++)
        if (is_sea(&dests[i]) && dests[i].room == room) return &dests[i];
    return 0;
}

/* Press fires once, then repeats after a delay while held (pad reads come about 30 per second). */
#define REPEAT_DELAY 12
#define REPEAT_EVERY 2
static int repeats(u32 buttons, u32 pressed, u32 mask) {
    if (pressed & mask) return 1;
    if (!(buttons & mask)) return 0;
    return held_frames >= REPEAT_DELAY && (held_frames - REPEAT_DELAY) % REPEAT_EVERY == 0;
}
static wwhd_hud_element el;

static void clamp_view(void) {
    if (cursor < 0) cursor = COUNT - 1;
    if (cursor >= COUNT) cursor = 0;
    if (cursor < scroll) scroll = cursor;
    if (cursor >= scroll + ROWS) scroll = cursor - ROWS + 1;
}

static void text(u32 list, f32 x, f32 y, f32 size, u32 rgba, const char* s) {
    u32 n = 0;
    while (s[n]) n++;
    el = (wwhd_hud_element){ .kind = WWHD_HUD_TEXT, .anchor = WWHD_HUD_TOP_LEFT,
        .x = x, .y = y, .size = size, .thickness = 1, .u1 = 1, .v1 = 1,
        .rgba = rgba, .text = s, .text_bytes = n };
    wwhd_hud_emit(list, &el);
}

static void draw(u32 list) {
    if (!open) return;
    el = (wwhd_hud_element){ .kind = WWHD_HUD_RECT, .anchor = WWHD_HUD_TOP_LEFT,
        .x = 40, .y = 60, .w = 460, .h = 52 + ROWS * 34,
        .thickness = 1, .u1 = 1, .v1 = 1, .rgba = 0x000000B0 };
    wwhd_hud_emit(list, &el);
    if (grid_mode) {
        char sq[3];
        u32 room = (u32)(grow * 7 + gcol + 1);
        square_label(sq, room);
        const dest* known = find_square(room);
        text(list, 56, 68, 26, 0xFFE070FF, "Go to square  (D-pad, A: go, X: list, B: close)");
        copy(square_text, sizeof square_text, "Square ", sq, "");
        text(list, 64, 130, 60, 0xFFFFFFFF, square_text);
        text(list, 64, 220, 26, 0xA0A0A0FF, known ? known->name : "(open sea)");
        text(list, 64, 270, 22, 0xA0A0A0FF, "Left/right: letter    Up/down: number");
        return;
    }
    text(list, 56, 68, 26, 0xFFE070FF, "Map warp  (D-pad, A: go, X: square, B: close)");
    for (int i = 0; i < ROWS && scroll + i < COUNT; i++) {
        int idx = scroll + i;
        char sq[3] = "";
        if (is_sea(&dests[idx])) square_label(sq, dests[idx].room);
        copy(row_text[i], sizeof row_text[i], sq, sq[0] ? "  " : "", dests[idx].name);
        text(list, 64, 106 + i * 34, 26, idx == cursor ? 0xFFFFFFFF : 0xA0A0A0FF, row_text[i]);
        if (idx == cursor) text(list, 56, 106 + i * 34, 26, 0xFFE070FF, ">");
    }
}

static void warp(const dest* d) {
    wwhd_log(d->name);
    /* layer -1 = default; speed 0; mode 1; use the stage's default spawn point */
    wwhd_dComIfGp_setNextStage((u32)d->stage, 0, d->room, (u32)-1, 0.0f, 1, 1, 0);
}

/* The game's own controller read (not in the public SDK): VPADRead(0, this + 0x14, 16, ...). Runs
 * before the game looks at the pad, so the menu is handled here and the game is given an empty
 * pad while it is open (otherwise D-pad up opens the Wind Waker, Plus the pause menu, etc). */
#define WWHD_ADDR_pad_read 0x0273E3FC
#define VPAD_STATUS_SIZE 0xACu
#define VPAD_SAMPLES 16

WWHD_HOOK_RETURN(WWHD_ADDR_pad_read, on_pad_read, (void* self)) {
    wwhd_input_state in;
    wwhd_input_read(&in);
    u32 pressed = in.buttons & ~(u32)prev_buttons;
    prev_buttons = (int)in.buttons;
    int swallow = open;

    if ((in.buttons & (BTN_ZL | BTN_ZR)) == (BTN_ZL | BTN_ZR) && (in.buttons & BTN_MINUS)) {
        if (pressed & BTN_MINUS) open = !open;
        swallow = 1;
    } else if (open) {
        u32 nav = BTN_UP | BTN_DOWN | BTN_LEFT | BTN_RIGHT;
        if (pressed & BTN_X) grid_mode = !grid_mode;
        if (pressed & nav) held_frames = 0;
        else if (in.buttons & nav) held_frames++;
        else held_frames = 0;
        if (grid_mode) {
            if (repeats(in.buttons, pressed, BTN_LEFT)) gcol = (gcol + 6) % 7;
            if (repeats(in.buttons, pressed, BTN_RIGHT)) gcol = (gcol + 1) % 7;
            if (repeats(in.buttons, pressed, BTN_UP)) grow = (grow + 6) % 7;
            if (repeats(in.buttons, pressed, BTN_DOWN)) grow = (grow + 1) % 7;
            if (pressed & BTN_A) {
                target = (dest){ "square", "sea", (u32)(grow * 7 + gcol + 1) };
                open = 0;
                pending = 1;
            }
        } else {
            if (repeats(in.buttons, pressed, BTN_UP)) cursor--;
            if (repeats(in.buttons, pressed, BTN_DOWN)) cursor++;
            if (repeats(in.buttons, pressed, BTN_LEFT)) { cursor -= ROWS; if (cursor < 0) cursor = 0; }
            if (repeats(in.buttons, pressed, BTN_RIGHT)) { cursor += ROWS; if (cursor >= COUNT) cursor = COUNT - 1; }
            clamp_view();
            if (pressed & BTN_A) { target = dests[cursor]; open = 0; pending = 1; }
        }
        if (pressed & BTN_B) open = 0;
    }
    blocking = swallow;
    if (!swallow) return;
    /* hold, trigger and release words of every sample */
    for (u32 i = 0; i < VPAD_SAMPLES; i++) {
        u32* sample = (u32*)((u8*)self + 0x14 + i * VPAD_STATUS_SIZE);
        sample[0] = sample[1] = sample[2] = 0;
    }
}

WWHD_GAME_ORIGINAL(WWHD_ADDR_daPy_lk_c__procTactWait_init, s32, tact_wait_init_original, (void* self, int arg));

/* Starting the Wind Waker is what the D-pad does in the game: refuse while the menu is using the pad. */
WWHD_REPLACE(WWHD_ADDR_daPy_lk_c__procTactWait_init, s32, on_tact_wait_init, (void* self, int arg)) {
    if (blocking) return 0;
    return tact_wait_init_original(self, arg);
}

WWHD_HOOK(WWHD_ADDR_daPy_Execute, on_link_execute, (void* link)) {
    (void)link;
    wwhd_hud_register(draw, WWHD_HUD_TV);
    if (pending) {
        pending = 0;
        warp(&target);
    }
}
