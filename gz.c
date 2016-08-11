#include <stdio.h>
#include <string.h>
#include <startup.h>
#include <n64.h>
#include <z64.h>
#include <mips.h>
#include "console.h"
#include "menu.h"

struct switch_info
{
  const char *name;
  uint8_t    *address;
  uint8_t     mask;
};

struct warp_info
{
  struct menu_item *entrance;
  struct menu_item *age;
};


void *g_text_ptr;
static int g_ready = 0;

static int g_menu_active = 1;
static struct menu g_menu_main;
static struct menu g_menu_watches;
static struct menu g_menu_equipment;
static struct menu g_menu_equips;
static struct menu g_menu_items;
static struct menu g_menu_variable_items;
static struct menu g_menu_misc;
static struct menu g_menu_cheats;
static struct menu g_menu_warps;

static int g_cheats_energy    = 0;
static int g_cheats_magic     = 0;
static int g_cheats_sticks    = 0;
static int g_cheats_nuts      = 0;
static int g_cheats_bombs     = 0;
static int g_cheats_arrows    = 0;
static int g_cheats_seeds     = 0;
static int g_cheats_bombchus  = 0;
static int g_cheats_beans     = 0;
static int g_cheats_keys      = 0;
static int g_cheats_rupees    = 0;
static int g_cheats_nayru     = 0;
static int g_cheats_time      = 0;


static int tp_slot = 0;

static z64_controller_t *input_ptr = (void*)0x8011D730;

static int frames_queued = -1;

static struct switch_info equipment_list[] =
{
  {"kokiri sword",          (void*)0x8011A66D, 0b00000001},
  {"master sword",          (void*)0x8011A66D, 0b00000010},
  {"giant's knife",         (void*)0x8011A66D, 0b00000100},
  {"deku shield",           (void*)0x8011A66D, 0b00010000},
  {"hylian shield",         (void*)0x8011A66D, 0b00100000},
  {"mirror shield",         (void*)0x8011A66D, 0b01000000},
  {"kokiri tunic",          (void*)0x8011A66C, 0b00000001},
  {"goron tunic",           (void*)0x8011A66C, 0b00000010},
  {"zora tunic",            (void*)0x8011A66C, 0b00000100},
  {"kokiri boots",          (void*)0x8011A66C, 0b00010000},
  {"iron boots",            (void*)0x8011A66C, 0b00100000},
  {"hover boots",           (void*)0x8011A66C, 0b01000000},
  {"broken giant's knife",  (void*)0x8011A66D, 0b00001000},
  {"biggoron's sword",      (void*)0x8011A60E, 0b00000001},
};

static struct switch_info item_list[] =
{
  {"deku stick",      (void*)0x8011a644, 0x00},
  {"deku nut",        (void*)0x8011a645, 0x01},
  {"bomb",            (void*)0x8011a646, 0x02},
  {"bow",             (void*)0x8011a647, 0x03},
  {"fire arrow",      (void*)0x8011A648, 0x04},
  {"ice arrow",       (void*)0x8011A64E, 0x0C},
  {"light arrow",     (void*)0x8011A654, 0x12},
  {"din's fire",      (void*)0x8011A649, 0x05},
  {"farore's wind",   (void*)0x8011A64F, 0x0D},
  {"nayru's love",    (void*)0x8011A655, 0x13},
  {"slingshot",       (void*)0x8011a64a, 0x06},
  {"fairy ocarina",   (void*)0x8011A64B, 0x07},
  {"ocarina of time", (void*)0x8011A64B, 0x08},
  {"bombchu",         (void*)0x8011a64c, 0x09},
  {"hookshot",        (void*)0x8011A64D, 0x0A},
  {"longshot",        (void*)0x8011A64D, 0x0B},
  {"boomerang",       (void*)0x8011A650, 0x0E},
  {"lens of truth",   (void*)0x8011A651, 0x0F},
  {"magic bean",      (void*)0x8011A652, 0x10},
  {"megaton hammer",  (void*)0x8011A653, 0x11},
};

static int generic_switch_proc(struct menu_item *item,
                               enum menu_callback_reason reason,
                               void *data)
{
  int *state = data;
  if (reason == MENU_CALLBACK_SWITCH_ON)
    *state = 1;
  else if (reason == MENU_CALLBACK_SWITCH_OFF)
    *state = 0;
  else if (reason == MENU_CALLBACK_THINK)
    menu_switch_set(item, *state);
  return 0;
}

static void main_return_proc(struct menu_item *item, void *data)
{
  g_menu_active = 0;
}

static int equipment_switch_proc(struct menu_item *item,
                                 enum menu_callback_reason reason,
                                 void *data)
{
  struct switch_info *e = data;
  if (reason == MENU_CALLBACK_SWITCH_ON)
    *e->address |= e->mask;
  else if (reason == MENU_CALLBACK_SWITCH_OFF)
    *e->address &= ~e->mask;
  else if (reason == MENU_CALLBACK_THINK)
    menu_switch_set(item, (*e->address & e->mask) == e->mask);
  return 0;
}

static int bottle_option_proc(struct menu_item *item,
                              enum menu_callback_reason reason,
                              void *data)
{
  uint8_t *slot_ptr = data;
  uint8_t item_ids[] =
  {
    0xFF, 0x14, 0x15, 0x16,
    0x17, 0x18, 0x19, 0x1A,
    0x1C, 0x1D, 0x1E, 0x1F,
    0x20,
  };
  int num_items = sizeof(item_ids) / sizeof(*item_ids);
  if (reason == MENU_CALLBACK_THINK_INACTIVE) {
    int item_index = 0;
    for (int i = 0; i < num_items; ++i)
      if (*slot_ptr ==item_ids[i]) {
        item_index = i;
        break;
      }
    if (menu_option_get(item) != item_index)
      menu_option_set(item, item_index);
  }
  else if (reason == MENU_CALLBACK_DEACTIVATE)
    *slot_ptr = item_ids[menu_option_get(item)];
  return 0;
}

static int child_trade_option_proc(struct menu_item *item,
                                   enum menu_callback_reason reason,
                                   void *data)
{
  uint8_t *slot_ptr = (void*)0x8011A65A;
  uint8_t item_ids[] =
  {
    0xFF, 0x21, 0x22, 0x23,
    0x24, 0x25, 0x26, 0x27,
    0x28, 0x29, 0x2A, 0x2B,
    0x2C,
  };
  int num_items = sizeof(item_ids) / sizeof(*item_ids);
  if (reason == MENU_CALLBACK_THINK_INACTIVE) {
    int item_index = 0;
    for (int i = 0; i < num_items; ++i)
      if (*slot_ptr ==item_ids[i]) {
        item_index = i;
        break;
      }
    if (menu_option_get(item) != item_index)
      menu_option_set(item, item_index);
  }
  else if (reason == MENU_CALLBACK_DEACTIVATE)
    *slot_ptr = item_ids[menu_option_get(item)];
  return 0;
}

static int adult_trade_option_proc(struct menu_item *item,
                                   enum menu_callback_reason reason,
                                   void *data)
{
  uint8_t *slot_ptr = (void*)0x8011A65B;
  uint8_t item_ids[] =
  {
    0xFF, 0x2D, 0x2E, 0x2F,
    0x30, 0x31, 0x32, 0x33,
    0x34, 0x35, 0x36, 0x37,
  };
  int num_items = sizeof(item_ids) / sizeof(*item_ids);
  if (reason == MENU_CALLBACK_THINK_INACTIVE) {
    int item_index = 0;
    for (int i = 0; i < num_items; ++i)
      if (*slot_ptr ==item_ids[i]) {
        item_index = i;
        break;
      }
    if (menu_option_get(item) != item_index)
      menu_option_set(item, item_index);
  }
  else if (reason == MENU_CALLBACK_DEACTIVATE)
    *slot_ptr = item_ids[menu_option_get(item)];
  return 0;
}

static int equip_option_proc(struct menu_item *item,
                             enum menu_callback_reason reason,
                             void *data)
{
  uint16_t *equips_ptr = (void*)0x8011A640;
  int equip_row = (int)data;
  if (reason == MENU_CALLBACK_THINK_INACTIVE) {
    int equip = (*equips_ptr >> equip_row * 4) & 0x000F;
    if (menu_option_get(item) != equip)
      menu_option_set(item, equip);
  }
  else if (reason == MENU_CALLBACK_DEACTIVATE)
    *equips_ptr = (*equips_ptr & ~(0x000F << equip_row * 4)) |
                  (menu_option_get(item) << equip_row * 4);
  return 0;
}

static int bytemod_proc(struct menu_item *item,
                        enum menu_callback_reason reason,
                        void *data)
{
  uint8_t *data_ptr = data;
  if (reason == MENU_CALLBACK_THINK_INACTIVE) {
    if (menu_intinput_get(item) != *data_ptr)
      menu_intinput_set(item, *data_ptr);
  }
  else if (reason == MENU_CALLBACK_CHANGED)
    *data_ptr = menu_intinput_get(item);
  return 0;
}

static int swordless_proc(struct menu_item *item,
                          enum menu_callback_reason reason,
                          void *data)
{
  uint8_t *flag_ptr = (void*)0x8011B503;
  if (reason == MENU_CALLBACK_THINK_INACTIVE) {
    if (menu_option_get(item) != *flag_ptr)
      menu_option_set(item, *flag_ptr);
  }
  else if (reason == MENU_CALLBACK_DEACTIVATE)
    *flag_ptr = menu_option_get(item);
  return 0;
}

static int item_switch_proc(struct menu_item *item,
                            enum menu_callback_reason reason,
                            void *data)
{
  struct switch_info *e = data;
  if (reason == MENU_CALLBACK_SWITCH_ON)
    *e->address = e->mask;
  else if (reason == MENU_CALLBACK_SWITCH_OFF)
    *e->address = -1;
  else if (reason == MENU_CALLBACK_THINK)
    menu_switch_set(item, *e->address == e->mask);
  return 0;
}

static void clear_flags_proc(struct menu_item *item, void *data)
{
  (*(uint32_t*)0x801CA1C8) = 0x00000000;
  (*(uint32_t*)0x801CA1CC) = 0x00000000;
  (*(uint32_t*)0x801CA1D8) = 0x00000000;
  (*(uint32_t*)0x801CA1DC) = 0x00000000;
}
static void set_flags_proc(struct menu_item *item, void *data)
{
  (*(uint32_t*)0x801CA1C8) = 0xFFFFFFFF;
  (*(uint32_t*)0x801CA1CC) = 0xFFFFFFFF;
  (*(uint32_t*)0x801CA1D8) = 0xFFFFFFFF;
  (*(uint32_t*)0x801CA1DC) = 0xFFFFFFFF;
}

static void tp_slot_dec_proc(struct menu_item *item, void *data)
{
  struct menu_item *tp_slot_display = data;
  tp_slot = (tp_slot - 1) % 10;
  if (tp_slot < 0)
    tp_slot += 10;
  tp_slot_display->text[0] = '0' + tp_slot;
}

static void tp_slot_inc_proc(struct menu_item *item, void *data)
{
  struct menu_item *tp_slot_display = data;
  tp_slot = (tp_slot + 1) % 10;
  tp_slot_display->text[0] = '0' + tp_slot;
}

static void warp_proc(struct menu_item *item, void *data)
{
  struct warp_info *warp_info = data;
  uint16_t entrance = menu_intinput_get(warp_info->entrance);
  uint8_t  age      = menu_option_get(warp_info->age);
  (*(uint16_t*)0x801DA2BA) = entrance;
  (*(uint8_t*) 0x801DA288) = age;
  (*(uint16_t*)0x801DA2B4) = 0x0014;
}

static void clear_csp_proc(struct menu_item *item, void *data)
{
  static uint32_t null_cs[] = {0, 0};
  (*(void**)0x801CA208) = &null_cs;
}

static void input_hook()
{
  if (frames_queued != 0)
    ((void(*)())0x800A0BA0)();
}

static void update_hook()
{
  if (frames_queued != 0) {
    if (frames_queued > 0)
      --frames_queued;
    ((void(*)())0x8009AF1C)();
  }
}

static void pause_proc(struct menu_item *item, void *data)
{
  uint32_t *input_call = (void*)0x800A16AC;
  *input_call = MIPS_JAL(&input_hook);
  uint32_t *update_call = (void*)0x8009CAE8;
  *update_call = MIPS_JAL(&update_hook);
  if (frames_queued >= 0)
    frames_queued = -1;
  else
    frames_queued = 0;
}

static void advance_proc(struct menu_item *item, void *data)
{
  if (frames_queued >= 0)
    ++frames_queued;
  else
    pause_proc(item, data);
}

#if 0
static uint8_t old_exc_handler[MIPS_EVSIZE];
static uint8_t new_exc_handler[4 * 32];

static void sup_exc_proc(struct menu_item *item, void *data)
{
  memcpy(&old_exc_handler, (void*)MIPS_EV_GE, MIPS_EVSIZE);
  uint32_t exc_handler[] =
  {
    MIPS_MFC0(MIPS_K0, MIPS_CP0_CAUSE),
    MIPS_LA(MIPS_K1, MIPS_CAUSE_EXCMASK),
    MIPS_AND(MIPS_K0, MIPS_K0, MIPS_K1),
    MIPS_LA(MIPS_K1, MIPS_MAKE_FIELD(MIPS_CAUSE_EXC, MIPS_EXC_TLBL)),
    MIPS_BNE(MIPS_K0, MIPS_K1, 4 * 7),
    MIPS_NOP,
    MIPS_MFC0(MIPS_K0, MIPS_CP0_SR),
    MIPS_LA(MIPS_K1, ~MIPS_STATUS_EXL),
    MIPS_AND(MIPS_K0, MIPS_K0, MIPS_K1),
    MIPS_MTC0(MIPS_K0, MIPS_CP0_SR),
    MIPS_ERET,
    MIPS_LA(MIPS_K0, &old_exc_handler),
    MIPS_JR(MIPS_K0),
    MIPS_NOP,
  };
  memcpy(&new_exc_handler, &exc_handler, sizeof(exc_handler));
  uint32_t exc_hook[] =
  {
    MIPS_LA(MIPS_K0, &new_exc_handler),
    MIPS_JR(MIPS_K0),
    MIPS_NOP,
  };
  memcpy((void*)MIPS_EV_GE, &exc_hook, sizeof(exc_hook));
}
#endif

ENTRY void _start(void *text_ptr)
{
  init_gp();
  if (!g_ready) {
    g_ready = 1;
    do_global_ctors();

    /* disable map toggling */
    (*(uint32_t*)0x8006CD50) = MIPS_BEQ(MIPS_R0, MIPS_R0, 0x82C);
    (*(uint32_t*)0x8006D4E4) = MIPS_BEQ(MIPS_R0, MIPS_R0, 0x98);

    g_text_ptr = text_ptr;

#if 0
    console_init(36, 288);
    console_set_view(2, 8, 36, 18);
#endif
    menu_init(&g_menu_main);
    g_menu_main.selector = menu_add_button(&g_menu_main, 2, 6, "return",
                                           main_return_proc, NULL, 0);
    menu_add_submenu(&g_menu_main, 2, 7, &g_menu_watches, "watches", 0);
    menu_add_submenu(&g_menu_main, 2, 8, &g_menu_equipment, "equipment", 0);
    menu_add_submenu(&g_menu_main, 2, 9, &g_menu_equips, "equips", 0);
    menu_add_submenu(&g_menu_main, 2, 10, &g_menu_items, "items", 0);
    menu_add_submenu(&g_menu_main, 2, 11, &g_menu_misc, "misc", 0);
    menu_add_submenu(&g_menu_main, 2, 12, &g_menu_cheats, "cheats", 0);
    menu_add_submenu(&g_menu_main, 2, 13, &g_menu_warps, "warps", 0);

    menu_init(&g_menu_watches);
    g_menu_watches.selector = menu_add_submenu(&g_menu_watches, 2, 6, NULL,
                                               "return", 0);
    menu_add_watchlist(&g_menu_watches, 2, 7, 0);

    menu_init(&g_menu_equipment);
    g_menu_equipment.selector = menu_add_submenu(&g_menu_equipment, 2, 6, NULL,
                                                 "return", 0);
    for (int i = 0; i < sizeof(equipment_list) / sizeof(*equipment_list); ++i)
      menu_add_switch(&g_menu_equipment,
                      2 + i / 3 % 2 * 20,
                      7 + i / 6 * 3 + i % 3,
                      equipment_list[i].name,
                      equipment_switch_proc,
                      &equipment_list[i],
                      0);

    menu_init(&g_menu_equips);
    g_menu_equips.selector = menu_add_submenu(&g_menu_equips, 2, 6, NULL,
                                              "return", 0);
    menu_add_static(&g_menu_equips, 2, 7, "sword", 0xFFFFFF);
    menu_add_option(&g_menu_equips, 12, 7,
                    "none\0""kokiri\0""master\0""biggoron\0",
                    equip_option_proc, (void*)0, 0);
    menu_add_static(&g_menu_equips, 2, 8, "shield", 0xFFFFFF);
    menu_add_option(&g_menu_equips, 12, 8,
                    "none\0""deku\0""hylian\0""mirror\0",
                    equip_option_proc, (void*)1, 0);
    menu_add_static(&g_menu_equips, 2, 9, "tunic", 0xFFFFFF);
    menu_add_option(&g_menu_equips, 12, 9,
                    "none\0""kokiri\0""goron\0""zora\0",
                    equip_option_proc, (void*)2, 0);
    menu_add_static(&g_menu_equips, 2, 10, "boots", 0xFFFFFF);
    menu_add_option(&g_menu_equips, 12, 10,
                    "none\0""kokiri\0""iron\0""hover\0",
                    equip_option_proc, (void*)3, 0);
    menu_add_static(&g_menu_equips, 2, 11, "b button", 0xFFFFFF);
    menu_add_intinput(&g_menu_equips, 12, 11, 16, 2, bytemod_proc,
                      (void*)0x8011A638, 0);
    menu_add_static(&g_menu_equips, 2, 12, "c left", 0xFFFFFF);
    menu_add_intinput(&g_menu_equips, 12, 12, 16, 2, bytemod_proc,
                      (void*)0x8011A639, 0);
    menu_add_static(&g_menu_equips, 2, 13, "c down", 0xFFFFFF);
    menu_add_intinput(&g_menu_equips, 12, 13, 16, 2, bytemod_proc,
                      (void*)0x8011A63A, 0);
    menu_add_static(&g_menu_equips, 2, 14, "c right", 0xFFFFFF);
    menu_add_intinput(&g_menu_equips, 12, 14, 16, 2, bytemod_proc,
                      (void*)0x8011A63B, 0);
    menu_add_static(&g_menu_equips, 2, 15, "swordless", 0xFFFFFF);
    menu_add_option(&g_menu_equips, 12, 15, "no\0""yes\0",
                    swordless_proc, NULL, 0);

    menu_init(&g_menu_items);
    g_menu_items.selector = menu_add_submenu(&g_menu_items, 2, 6, NULL,
                                             "return", 0);
    for (int i = 0; i < sizeof(item_list) / sizeof(*item_list); ++i)
      menu_add_switch(&g_menu_items,
                      2 + i % 2 * 20,
                      7 + i / 2,
                      item_list[i].name,
                      item_switch_proc,
                      &item_list[i],
                      0);
    menu_add_submenu(&g_menu_items, 2, 17, &g_menu_variable_items,
                     "variable items", 0);

    menu_init(&g_menu_variable_items);
    g_menu_variable_items.selector = menu_add_submenu(&g_menu_variable_items,
                                                      2, 6, NULL, "return", 0);
    for (int i = 0; i < 4; ++i) {
      char s[41];
      sprintf(s, "bottle %i:", i + 1);
      menu_add_static(&g_menu_variable_items, 2, 7 + i, s, 0xFFFFFF);
      menu_add_option(&g_menu_variable_items, 20, 7 + i,
                      "nothing\0""bottle\0""red potion\0""green potion\0"
                      "blue potion\0""fairy\0""fish\0""milk\0""blue fire\0"
                      "bug\0""big poe\0""half milk\0""poe\0",
                      bottle_option_proc, (void*)(0x8011A656 + i), 0);
    }
    menu_add_static(&g_menu_variable_items, 2, 11, "child trade item:",
                    0xFFFFFF);
    menu_add_option(&g_menu_variable_items, 20, 11,
                    "nothing\0""weird egg\0""chicken\0""zelda's letter\0"
                    "keaton mask\0""skull mask\0""spooky mask\0""bunny hood\0"
                    "goron mask\0""zora mask\0""gerudo mask\0""mask of truth\0"
                    "sold out\0",
                    child_trade_option_proc, NULL, 0);
    menu_add_static(&g_menu_variable_items, 2, 12, "adult trade item:",
                    0xFFFFFF);
    menu_add_option(&g_menu_variable_items, 20, 12,
                    "nothing\0""pocket egg\0""pocket cucco\0""cojiro\0"
                    "odd mushroom\0""odd potion\0""poacher's saw\0"
                    "broken goron's sword\0""prescription\0""eyeball frog\0"
                    "eye drops\0""claim check\0",
                    adult_trade_option_proc, NULL, 0);

    menu_init(&g_menu_misc);
    g_menu_misc.selector = menu_add_submenu(&g_menu_misc, 2, 6, NULL,
                                            "return", 0);
    menu_add_button(&g_menu_misc, 2, 7, "clear flags",
                    clear_flags_proc, NULL, 0);
    menu_add_button(&g_menu_misc, 2, 8, "set flags",
                    set_flags_proc, NULL, 0);
    menu_add_static(&g_menu_misc, 2, 9, "teleport slot", 0xFFFFFF);
    struct menu_item *tp_slot_display = menu_add_static(&g_menu_misc,
                                                        18, 9, "0",
                                                        0xA0A0A0);
    menu_add_button(&g_menu_misc, 16, 9, "-", tp_slot_dec_proc,
                    tp_slot_display, 0);
    menu_add_button(&g_menu_misc, 20, 9, "+", tp_slot_inc_proc,
                    tp_slot_display, 0);
    menu_add_button(&g_menu_misc, 2, 10, "clear cutscene pointer",
                    clear_csp_proc, NULL, 0);
    menu_add_button(&g_menu_misc, 2, 11, "pause / unpause", pause_proc,
                    NULL, 0);
    menu_add_button(&g_menu_misc, 2, 12, "frame advance", advance_proc,
                    NULL, 0);
#if 0
    menu_add_button(&g_menu_misc, 2, 13, "suppress exceptions", sup_exc_proc,
                    NULL, 0);
#endif

    menu_init(&g_menu_cheats);
    g_menu_cheats.selector = menu_add_submenu(&g_menu_cheats, 2, 6, NULL,
                                              "return", 0);
    menu_add_switch(&g_menu_cheats, 2, 7, "energy",
                    generic_switch_proc, &g_cheats_energy, 0);
    menu_add_switch(&g_menu_cheats, 2, 8, "magic",
                    generic_switch_proc, &g_cheats_magic, 0);
    menu_add_switch(&g_menu_cheats, 2, 9, "deku sticks",
                    generic_switch_proc, &g_cheats_sticks, 0);
    menu_add_switch(&g_menu_cheats, 2, 10, "deku nuts",
                    generic_switch_proc, &g_cheats_nuts, 0);
    menu_add_switch(&g_menu_cheats, 2, 11, "bombs",
                    generic_switch_proc, &g_cheats_bombs, 0);
    menu_add_switch(&g_menu_cheats, 2, 12, "arrows",
                    generic_switch_proc, &g_cheats_arrows, 0);
    menu_add_switch(&g_menu_cheats, 2, 13, "deku seeds",
                    generic_switch_proc, &g_cheats_seeds, 0);
    menu_add_switch(&g_menu_cheats, 2, 14, "bombchus",
                    generic_switch_proc, &g_cheats_bombchus, 0);
    menu_add_switch(&g_menu_cheats, 2, 15, "magic beans",
                    generic_switch_proc, &g_cheats_beans, 0);
    menu_add_switch(&g_menu_cheats, 2, 16, "small keys",
                    generic_switch_proc, &g_cheats_keys, 0);
    menu_add_switch(&g_menu_cheats, 2, 17, "rupees",
                    generic_switch_proc, &g_cheats_rupees, 0);
    menu_add_switch(&g_menu_cheats, 2, 18, "nayru's love",
                    generic_switch_proc, &g_cheats_nayru, 0);
    menu_add_switch(&g_menu_cheats, 2, 19, "advance time",
                    generic_switch_proc, &g_cheats_time, 0);

    menu_init(&g_menu_warps);
    g_menu_warps.selector = menu_add_submenu(&g_menu_warps, 2, 6, NULL,
                                             "return", 0);
    static struct warp_info warp_info;
    menu_add_static(&g_menu_warps, 2, 7, "entrance", 0xFFFFFF);
    warp_info.entrance = menu_add_intinput(&g_menu_warps, 12, 7, 16, 4,
                                           NULL, NULL, 0);
    menu_add_static(&g_menu_warps, 2, 8, "age", 0xFFFFFF);
    warp_info.age = menu_add_option(&g_menu_warps, 12, 8,
                                    "adult\0""child\0", NULL, NULL, 0);
    menu_add_button(&g_menu_warps, 2, 9, "warp", warp_proc, &warp_info, 0);
  }

#if 1
  static uint16_t pad_prev = 0;
  uint16_t pad_pressed = (pad_prev ^ input_ptr->pad) &
                         input_ptr->pad;
  pad_prev = input_ptr->pad;
  static int button_time[16] = {0};
  for (int i = 0; i < 16; ++i) {
    int button_state = (input_ptr->pad >> i) & 0x0001;
    button_time[i] = (button_time[i] + button_state) * button_state;
    if (button_time[i] >= 8)
      pad_pressed |= 1 << i;
  }

  if (g_cheats_energy)
    (*(uint16_t*)0x8011A600) = 0x0140;
  if (g_cheats_magic) {
    if ((*(uint8_t*) 0x8011A609) == 0x08)
      (*(uint8_t*) 0x8011A60A) = 0x01;
    (*(uint8_t*) 0x8011A60C) = 0x01;
    (*(uint8_t*) 0x8011A603) = 0x60;
  }
  if (g_cheats_sticks)
    (*(uint8_t*) 0x8011A65C) = 0x09;
  if (g_cheats_nuts)
    (*(uint8_t*) 0x8011A65D) = 0x09;
  if (g_cheats_bombs)
    (*(uint8_t*) 0x8011A65E) = 0x09;
  if (g_cheats_arrows)
    (*(uint8_t*) 0x8011A65F) = 0x09;
  if (g_cheats_seeds)
    (*(uint8_t*) 0x8011A662) = 0x09;
  if (g_cheats_bombchus)
    (*(uint8_t*) 0x8011A664) = 0x09;
  if (g_cheats_beans)
    (*(uint8_t*) 0x8011A66A) = 0x09;
  if (g_cheats_keys) {
    (*(uint8_t*) 0x8011A68F) = 0x09;
    (*(uint8_t*) 0x8011A690) = 0x09;
    (*(uint8_t*) 0x8011A691) = 0x09;
    (*(uint8_t*) 0x8011A692) = 0x09;
    (*(uint8_t*) 0x8011A693) = 0x09;
    (*(uint8_t*) 0x8011A697) = 0x09;
    (*(uint8_t*) 0x8011A699) = 0x09;
    (*(uint8_t*) 0x8011A69C) = 0x09;
  }
  if (g_cheats_rupees)
    (*(uint16_t*)0x8011A604) = 0x03E7;
  if (g_cheats_nayru)
    (*(uint16_t*)0x8011B998) = 0x044B;
  if (g_cheats_time)
    (*(uint16_t*)0x8011A5DC) += 0x0100;
  /* activated */
  if (frames_queued == -1 && input_ptr->pad & BUTTON_Z) {
    /* reload zone with d-pad down */
    if (button_time[BUTTON_INDEX_D_DOWN] >= 10)
      (*(uint16_t*)0x801DA2B4) = 0x0014;
    /* title screen with d-pad up */
    if (button_time[BUTTON_INDEX_D_UP] >= 10) {
      (*(uint8_t*) 0x8011B92F) = 0x02;
      (*(uint16_t*)0x801DA2B4) = 0x0014;
    }
    /* levitate with l */
    if (input_ptr->pad & BUTTON_L)
      (*(uint16_t*)0x801DAA90) = 0x40CB;
    /* teleportation */
    static z64_xyz_t stored_pos[10];
    static z64_rot_t stored_rot[10];
    if (input_ptr->pad & BUTTON_D_LEFT) {
      stored_pos[tp_slot] = z64_link_pos;
      stored_rot[tp_slot] = z64_link_rot;
    }
    if (input_ptr->pad & BUTTON_D_RIGHT) {
      z64_link_pos = stored_pos[tp_slot];
      (*(z64_xyz_t*)0x801DAA38) = stored_pos[tp_slot]; /* actor position */
      z64_link_rot = stored_rot[tp_slot];
      (*(uint16_t*)0x801DB25E) = stored_rot[tp_slot].y; /* locked rotation */
      /* (*(uint8_t*)0x801DAADE) = 0xFF; prevents collision with actors */
    }
  }
  else {
    if (g_menu_active) {
      if (pad_pressed & BUTTON_D_UP)
        menu_navigate(&g_menu_main, MENU_NAVIGATE_UP);
      if (pad_pressed & BUTTON_D_DOWN)
        menu_navigate(&g_menu_main, MENU_NAVIGATE_DOWN);
      if (pad_pressed & BUTTON_D_LEFT)
        menu_navigate(&g_menu_main, MENU_NAVIGATE_LEFT);
      if (pad_pressed & BUTTON_D_RIGHT)
        menu_navigate(&g_menu_main, MENU_NAVIGATE_RIGHT);
      if (pad_pressed & BUTTON_L)
        menu_activate(&g_menu_main);
    }
    else {
      if (pad_pressed & BUTTON_L)
        g_menu_active = 1;
    }
  }
  if (g_menu_active)
    menu_draw(&g_menu_main);
#endif

#if 1
  SetTextRGBA(g_text_ptr, 0xC8, 0xC8, 0xC8, 0xFF);
  SetTextXY(g_text_ptr, 2, 28);
  SetTextString(g_text_ptr, "%4i %4i", input_ptr->x, input_ptr->y);
  static struct
  {
    uint16_t    mask;
    const char *name;
    uint32_t    color;
  }
  buttons[] =
  {
    {0x8000, "A", 0x0000FF},
    {0x4000, "B", 0x00FF00},
    {0x1000, "S", 0xFF0000},
    {0x0020, "L", 0xC8C8C8},
    {0x0010, "R", 0xC8C8C8},
    {0x2000, "Z", 0xC8C8C8},
    {0x0008, "u", 0xFFFF00},
    {0x0004, "d", 0xFFFF00},
    {0x0002, "l", 0xFFFF00},
    {0x0001, "r", 0xFFFF00},
    {0x0800, "u", 0xC8C8C8},
    {0x0400, "d", 0xC8C8C8},
    {0x0200, "l", 0xC8C8C8},
    {0x0100, "r", 0xC8C8C8},
  };
  for (int i = 0; i < sizeof(buttons) / sizeof(*buttons); ++i) {
    if (!(input_ptr->pad & buttons[i].mask))
      continue;
    SetTextRGBA(g_text_ptr, (buttons[i].color >> 16) & 0xFF,
                            (buttons[i].color >> 8)  & 0xFF,
                            (buttons[i].color >> 0)  & 0xFF,
                            0xFF);
    SetTextXY(g_text_ptr, 12 + i, 28);
    SetTextString(g_text_ptr, "%s", buttons[i].name);
  }
#endif

#if 0
  if (input_ptr->du)
    console_scroll(0, -1);
  if (input_ptr->dd)
    console_scroll(0, 1);
  if (input_ptr->dl)
    console_scroll(-1, 0);
  if (input_ptr->dr)
    console_scroll(1, 0);
  console_print();
#endif
}


/* custom support library */
#include <startup.c>
#include <vector/vector.c>
#include <list/list.c>