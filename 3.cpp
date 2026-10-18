#include <iostream>
#include <string>
#include <vector>
#include <cmath>
#include <unistd.h>
#include <ncurses.h>
#include <cstdlib>
#include <ctime>
#include <memory>
#include <algorithm>

using namespace std;

enum GameState {
    STATE_OVERWORLD,
    STATE_HOUSE_INTERIOR,
    STATE_SHOP_INTERIOR,
    STATE_INN_INTERIOR,
    STATE_CRYPT_INTERIOR,
    STATE_BATTLE,
    STATE_GAMEOVER
};

void init_terminal() {
    initscr();
    cbreak();
    noecho();
    curs_set(0);
    keypad(stdscr, TRUE);
    timeout(0); // Non-blocking polling for real-time responsiveness
}

// -------------------------------------------------------------
// BASE GAME OBJECT & AABB COLLISION
// -------------------------------------------------------------
class game_object {
public:
    int x, y, width, height;
    game_object(int sx, int sy, int w, int h) : x(sx), y(sy), width(w), height(h) {}
    virtual ~game_object() = default;
    virtual void draw() const = 0;
};

// Tight bounding box checker
bool check_collision(int ax, int ay, int aw, int ah, const game_object& b) {
    return (ax < b.x + b.width && ax + aw > b.x &&
            ay < b.y + b.height && ay + ah > b.y);
}

// -------------------------------------------------------------
// CHEST
// -------------------------------------------------------------
class Chest : public game_object {
public:
    int reward;
    bool opened;
    bool locked;

    Chest(int sx, int sy, bool is_locked = false) 
        : game_object(sx, sy, 9, 3), reward(180), opened(false), locked(is_locked) {}

    void draw() const override {
        if (!opened) {
            if (locked) {
                mvprintw(y,     x, " _______ ");
                mvprintw(y + 1, x, "|__[#]__|");
                mvprintw(y + 2, x, "|_LOCK__|");
            } else {
                mvprintw(y,     x, " _______ ");
                mvprintw(y + 1, x, "|__[_]__|");
                mvprintw(y + 2, x, "|_______|");
            }
        } else {
            mvprintw(y,     x, " _______ ");
            mvprintw(y + 1, x, "|  OPEN |");
            mvprintw(y + 2, x, "|_empty_|");
        }
    }
};

// -------------------------------------------------------------
// UNBRANDED SYMBOLIC BUILDINGS
// -------------------------------------------------------------
enum BuildingType {
    BLDG_HOUSE,
    BLDG_SHOP,
    BLDG_INN,
    BLDG_CRYPT
};

class Building : public game_object {
public:
    int door_x, door_y;
    BuildingType type;

    Building(int sx, int sy, BuildingType t)
        : game_object(sx, sy, 15, 5), door_x(sx + 5), door_y(sy + 5), type(t) {}

    void draw() const override {
        if (type == BLDG_SHOP) {
            mvprintw(y,     x, "  /=========\\  ");
            mvprintw(y + 1, x, " /   [$$$]   \\ ");
            mvprintw(y + 2, x, "|  $$$   $$$  | ");
            mvprintw(y + 3, x, "|  [ ]   [ ]  | ");
            mvprintw(y + 4, x, "|____|#|______| ");
            mvprintw(door_y, door_x - 1, "[DOOR]");
        } else if (type == BLDG_INN) {
            mvprintw(y,     x, "  /~~~~~~~~~\\  ");
            mvprintw(y + 1, x, " /   [ZZZ]   \\ ");
            mvprintw(y + 2, x, "|  (u)   (u)  | ");
            mvprintw(y + 3, x, "|  [ ]   [ ]  | ");
            mvprintw(y + 4, x, "|____|#|______| ");
            mvprintw(door_y, door_x - 1, "[DOOR]");
        } else if (type == BLDG_CRYPT) {
            mvprintw(y,     x, "  /!\\=====/!\\  ");
            mvprintw(y + 1, x, " /   [CRYPT] \\ ");
            mvprintw(y + 2, x, "|  [+]   [+]  | ");
            mvprintw(y + 3, x, "|  RIP   RIP  | ");
            mvprintw(y + 4, x, "|____|#|______| ");
            mvprintw(door_y, door_x - 1, "[GATE]");
        } else {
            mvprintw(y,     x, "  /---------\\  ");
            mvprintw(y + 1, x, " /   [---]   \\ ");
            mvprintw(y + 2, x, "|   _     _   | ");
            mvprintw(y + 3, x, "|  [ ]   [ ]  | ");
            mvprintw(y + 4, x, "|____|#|______| ");
            mvprintw(door_y, door_x - 1, "[DOOR]");
        }
    }

    bool is_standing_at_door(int px, int py, int pw, int ph) const {
        return (px + pw > door_x - 1 && px < door_x + 6 && 
                py + ph >= door_y && py <= door_y + 1);
    }
};

// -------------------------------------------------------------
// ORIGINAL MULTI-LINE PLAYER
// -------------------------------------------------------------
class Player : public game_object {
public:
    int health, max_health, mana, max_mana, gold;
    int potions, elixirs, keys, sword_level, fish_count;
    int invulnerable_ticks;

    // Hitbox is set to 4x2 at feet to prevent getting snagged on walls
    Player(int sx, int sy)
        : game_object(sx, sy, 4, 2), health(100), max_health(100), 
          mana(50), max_mana(50), gold(60), potions(2), elixirs(1), keys(1), sword_level(1), fish_count(0), invulnerable_ticks(0) {}

    void draw() const override {
        if (invulnerable_ticks > 0 && (invulnerable_ticks / 4) % 2 == 0) {
            return;
        }
        mvprintw(y - 2, x, "  _O_ ");
        mvprintw(y - 1, x, " //|\\");
        mvprintw(y,     x, "   |  ");
        mvprintw(y + 1, x, "  / \\ ");
    }
};

// -------------------------------------------------------------
// LIVING VILLAGERS & PETS
// -------------------------------------------------------------
class Villager : public game_object {
public:
    string dialogue;
    int timer = 0;

    Villager(int sx, int sy, string d) 
        : game_object(sx, sy, 4, 2), dialogue(d) {}

    void draw() const override {
        mvprintw(y,     x, " O ");
        mvprintw(y + 1, x, "/|\\");
    }

    void wander() {
        timer++;
        if (timer < 35) return;
        timer = 0;
        int d = rand() % 5;
        if (d == 0 && x > 26) x--;
        else if (d == 1 && x < 33) x++;
        else if (d == 2 && y > 15) y--;
        else if (d == 3 && y < 19) y++;
    }
};

class StrayDog : public game_object {
public:
    int timer = 0;
    StrayDog(int sx, int sy) : game_object(sx, sy, 7, 1) {}

    void draw() const override {
        mvprintw(y, x, "(=^.^=)");
    }

    void wander() {
        timer++;
        if (timer < 20) return;
        timer = 0;
        int d = rand() % 4;
        if (d == 0 && x > 25) x--;
        else if (d == 1 && x < 33) x++;
        else if (d == 2 && y > 15) y--;
        else if (d == 3 && y < 19) y++;
    }
};

// -------------------------------------------------------------
// ORIGINAL GOBLIN ENEMY
// -------------------------------------------------------------
class Goblin : public game_object {
public:
    int health, max_health, damage;
    int move_cooldown, speed_delay;
    bool active;

    Goblin(int sx, int sy)
        : game_object(sx, sy, 9, 4), health(50), max_health(50), 
          damage(12), move_cooldown(0), speed_delay(15), active(true) {}

    void draw() const override {
        if (!active) return;
        mvprintw(y,     x, "    ^ ^  ");
        mvprintw(y + 1, x, "   (o.o) ");
        mvprintw(y + 2, x, "   >/|\\<");
        mvprintw(y + 3, x, "    / \\ ");
    }

    void track_and_move(int target_x, int target_y, const vector<shared_ptr<Building>>& buildings) {
        if (!active) return;

        move_cooldown++;
        if (move_cooldown < speed_delay) return;
        move_cooldown = 0;

        float dx = static_cast<float>(target_x - x);
        float dy = static_cast<float>(target_y - y);
        float distance = std::sqrt(dx * dx + dy * dy);

        if (distance > 18.0f || distance < 2.0f) return;

        int step_x = (dx > 0) ? 1 : ((dx < 0) ? -1 : 0);
        int step_y = (dy > 0) ? 1 : ((dy < 0) ? -1 : 0);

        bool blocked_x = false;
        for (const auto& b : buildings) {
            if (check_collision(x + step_x, y, width, height, *b)) {
                blocked_x = true;
                break;
            }
        }
        if (!blocked_x && x + step_x >= 2 && x + step_x + width < COLS - 2) x += step_x;

        bool blocked_y = false;
        for (const auto& b : buildings) {
            if (check_collision(x, y + step_y, width, height, *b)) {
                blocked_y = true;
                break;
            }
        }
        if (!blocked_y && y + step_y >= 2 && y + step_y + height < LINES - 2) y += step_y;
    }
};

// -------------------------------------------------------------
// OVERWORLD MAP RENDERER
// -------------------------------------------------------------
void draw_living_overworld(int anim_frame) {
    // 1. Boundary
    for (int c = 0; c < COLS; c++) {
        mvprintw(1, c, "#");
        mvprintw(LINES - 1, c, "#");
    }
    for (int r = 1; r < LINES; r++) {
        mvprintw(r, 0, "#");
        mvprintw(r, COLS - 1, "#");
    }

    // 2. Main Stone Roads
    for (int c = 1; c < COLS - 1; c++) {
        mvprintw(14, c, "=");
        mvprintw(19, c, "=");
    }
    for (int r = 2; r < LINES - 1; r++) {
        mvprintw(r, 34, "|");
        mvprintw(r, 46, "|");
    }

    // 3. Lake & Fishing Dock (Northwest)
    const char* lake_wave = (anim_frame % 20 < 10) ? "~ . ~ . ~ . ~ . ~" : ". ~ . ~ . ~ . ~ .";
    mvprintw(3,  3, "+-------------------+");
    mvprintw(4,  3, "| %s |", lake_wave);
    mvprintw(5,  3, "| %s |", lake_wave);
    mvprintw(6,  3, "|  [FISHING DOCK]   |");
    mvprintw(7,  3, "+-------------------+");

    // 4. Water Well
    const char* ripple = (anim_frame % 20 < 10) ? "~ . ~" : ". ~ .";
    mvprintw(15, 37, "+-------+");
    mvprintw(16, 37, "| %s |", ripple);
    mvprintw(17, 37, "| [WELL]|");
    mvprintw(18, 37, "+-------+");

    // 5. Torches
    const char* torch = (anim_frame % 10 < 5) ? "(o)" : "(*)";
    mvprintw(13, 32, "%s", torch);
    mvprintw(14, 32, " | ");
    mvprintw(13, 48, "%s", torch);
    mvprintw(14, 48, " | ");

    // 6. Tall Grass Field
    for (int r = 20; r < LINES - 2; r++) {
        mvprintw(r, 52, "\" \" \" \" \" \" \" \" \" \" \" \"");
    }
}

// -------------------------------------------------------------
// FISHING MINIGAME
// -------------------------------------------------------------
void run_fishing_minigame(Player& hero) {
    timeout(-1);
    clear();

    mvprintw(LINES / 2 - 3, COLS / 2 - 15, "=== FISHING BY THE LAKE ===");
    mvprintw(LINES / 2 - 1, COLS / 2 - 15, "Casting line into the water...");
    refresh();
    usleep(600000);

    mvprintw(LINES / 2 + 1, COLS / 2 - 15, ">> A fish is biting! PRESS [Z] FAST! <<");
    refresh();

    int start_time = time(nullptr);
    bool caught = false;

    while (time(nullptr) - start_time <= 2) {
        int key = getch();
        if (key == 'z' || key == 'Z') {
            caught = true;
            break;
        }
    }

    clear();
    if (caught) {
        int earned = 25 + rand() % 25;
        hero.gold += earned;
        hero.fish_count++;
        mvprintw(LINES / 2, COLS / 2 - 15, "SUCCESS! Caught a Silver Trout!");
        mvprintw(LINES / 2 + 1, COLS / 2 - 15, "+%d Gold earned! [Press Z]", earned);
    } else {
        mvprintw(LINES / 2, COLS / 2 - 15, "The fish got away... [Press Z]");
    }
    refresh();

    while (true) {
        int k = getch();
        if (k == 'z' || k == 'Z' || k == 10) break;
    }

    timeout(0);
}

// -------------------------------------------------------------
// TURN-BASED BATTLE SCENE
// -------------------------------------------------------------
void run_battle_loop(Player& hero, Goblin& enemy, GameState& current_state) {
    timeout(-1);
    string combat_log = "A wandering Goblin attacks!";
    int selected_action = 0;

    while (current_state == STATE_BATTLE) {
        clear();

        for (int i = 0; i < COLS; i++) {
            mvprintw(0, i, "=");
            mvprintw(LINES - 8, i, "-");
            mvprintw(LINES - 1, i, "=");
        }

        // Enemy (Top-Right)
        mvprintw(2, COLS - 26, "GOBLIN (Lv. 3)");
        mvprintw(3, COLS - 26, "HP: [%d / %d]", enemy.health, enemy.max_health);
        mvprintw(5, COLS - 22, "    ^ ^  ");
        mvprintw(6, COLS - 22, "   (o.o) ");
        mvprintw(7, COLS - 22, "   >/|\\<");
        mvprintw(8, COLS - 22, "    / \\ ");

        // Player (Bottom-Left)
        mvprintw(6, 6, "  _O_  (YOU)");
        mvprintw(7, 6, " //|\\");
        mvprintw(8, 6, "   |  ");
        mvprintw(9, 6, "  / \\ ");
        mvprintw(11, 6, "HERO HP: [%d / %d] | MP: [%d / %d] | POTIONS: %d",
                 hero.health, hero.max_health, hero.mana, hero.max_mana, hero.potions);

        mvprintw(LINES - 7, 4, ">> %s", combat_log.c_str());

        int c1 = 6, c2 = 36;
        int r1 = LINES - 5, r2 = LINES - 3;
        int slash_dmg = 14 + (hero.sword_level * 6);

        mvprintw(r1, c1, "   Sword Slash (%d DMG)", slash_dmg);
        mvprintw(r1, c2, "   Magic Blast (32 DMG, -15 MP)");
        mvprintw(r2, c1, "   Healing Salve (+40 HP) [%dx]", hero.potions);
        mvprintw(r2, c2, "   Flee Combat");

        if (selected_action == 0) mvprintw(r1, c1, " > ");
        else if (selected_action == 1) mvprintw(r1, c2, " > ");
        else if (selected_action == 2) mvprintw(r2, c1, " > ");
        else if (selected_action == 3) mvprintw(r2, c2, " > ");

        mvprintw(LINES - 2, 4, "[ARROWS]: Select Action | [Z]: Execute | [C]: Flee");
        refresh();

        int ch = getch();
        bool player_acted = false;

        switch (ch) {
            case KEY_UP:    if (selected_action >= 2) selected_action -= 2; break;
            case KEY_DOWN:  if (selected_action <= 1) selected_action += 2; break;
            case KEY_LEFT:  if (selected_action % 2 == 1) selected_action -= 1; break;
            case KEY_RIGHT: if (selected_action % 2 == 0) selected_action += 1; break;
            case 'c':
            case 'C':       selected_action = 3; break;
        }

        if (ch == 'z' || ch == 'Z' || ch == 10 || ch == 'c' || ch == 'C') {
            if (selected_action == 0) {
                int dmg = slash_dmg + rand() % 5;
                enemy.health -= dmg;
                combat_log = "You slashed the Goblin for " + to_string(dmg) + " DMG!";
                player_acted = true;
            } else if (selected_action == 1) {
                if (hero.mana >= 15) {
                    hero.mana -= 15;
                    int dmg = 30 + rand() % 8;
                    enemy.health -= dmg;
                    combat_log = "Magic blast detonated for " + to_string(dmg) + " DMG!";
                    player_acted = true;
                } else combat_log = "Not enough Mana!";
            } else if (selected_action == 2) {
                if (hero.potions > 0) {
                    hero.potions--;
                    hero.health = min(hero.max_health, hero.health + 40);
                    combat_log = "Applied healing salve! Restored 40 HP!";
                    player_acted = true;
                } else combat_log = "No Potions remaining!";
            } else if (selected_action == 3) {
                hero.invulnerable_ticks = 250;
                enemy.x = (rand() % 2 == 0) ? 6 : COLS - 16;
                enemy.y = (rand() % 2 == 0) ? 4 : LINES - 8;
                current_state = STATE_OVERWORLD;
                break;
            }
        }

        if (enemy.health <= 0) {
            clear();
            mvprintw(LINES / 2, COLS / 2 - 15, "VICTORY! Monster was defeated!");
            mvprintw(LINES / 2 + 1, COLS / 2 - 15, "+150 Gold gained! [Press Z]");
            hero.gold += 150;
            enemy.active = false;
            refresh();
            while (true) {
                int k = getch();
                if (k == 'z' || k == 'Z' || k == 10) break;
            }
            current_state = STATE_OVERWORLD;
            break;
        }

        if (player_acted) {
            usleep(250000);
            int edmg = enemy.damage + (rand() % 4);
            hero.health -= edmg;
            combat_log += " | Goblin struck you for " + to_string(edmg) + " DMG!";

            if (hero.health <= 0) {
                current_state = STATE_GAMEOVER;
                break;
            }
        }
    }
    timeout(0);
}

// -------------------------------------------------------------
// TRADER & GENERAL STORE
// -------------------------------------------------------------
void run_shop_scene(Player& hero, GameState& current_state, Building& shop) {
    timeout(-1);
    int selected = 0;
    string msg = "Welcome! What supplies do you require?";

    struct Item {
        string name;
        int cost;
        string desc;
    };

    vector<Item> stock = {
        {"Healing Potion (+40 HP)", 30, "Restores 40 Hit Points."},
        {"Mana Elixir (+30 MP)", 40, "Restores 30 Mana Points."},
        {"Iron Dungeon Key", 50, "Unlocks sealed crypt chests."},
        {"Sword Sharpening (+6 DMG)", 100, "Permanent weapon attack upgrade."},
        {"Leave Counter", 0, "Return outside."}
    };

    while (current_state == STATE_SHOP_INTERIOR) {
        clear();

        for (int i = 0; i < COLS; i++) {
            mvprintw(0, i, "=");
            mvprintw(LINES - 1, i, "=");
        }
        mvprintw(1, COLS / 2 - 10, "=== TRADING POST ===");

        mvprintw(4, COLS / 2 - 2, "(^.^)");
        mvprintw(5, COLS / 2 - 4, "[MERCHANT]");
        mvprintw(6, 8, "================================================================");

        mvprintw(8, COLS / 2 - 2, "GOLD: %d G | POTIONS: %d | ELIXIRS: %d | KEYS: %d | SWORD LV: %d",
                 hero.gold, hero.potions, hero.elixirs, hero.keys, hero.sword_level);
        mvprintw(10, 10, ">> %s", msg.c_str());

        for (size_t i = 0; i < stock.size(); i++) {
            int ypos = 12 + i * 2;
            if (i == stock.size() - 1) {
                mvprintw(ypos, 14, "%s", stock[i].name.c_str());
            } else {
                mvprintw(ypos, 14, "%-28s - %3d G   (%s)",
                         stock[i].name.c_str(), stock[i].cost, stock[i].desc.c_str());
            }
            if ((int)i == selected) mvprintw(ypos, 10, " > ");
        }

        mvprintw(LINES - 3, 10, "[ARROWS]: Select | [Z]: Buy | [X/C]: Exit");
        refresh();

        int ch = getch();
        switch (ch) {
            case KEY_UP:   selected = (selected - 1 + stock.size()) % stock.size(); break;
            case KEY_DOWN: selected = (selected + 1) % stock.size(); break;
            case 'x':
            case 'X':
            case 'c':
            case 'C':
                current_state = STATE_OVERWORLD;
                hero.x = shop.door_x;
                hero.y = shop.door_y + 2;
                break;
        }

        if (ch == 'z' || ch == 'Z' || ch == 10) {
            if (selected == 0) {
                if (hero.gold >= 30) {
                    hero.gold -= 30;
                    hero.potions++;
                    msg = "Bought 1 Healing Potion!";
                } else msg = "Not enough Gold!";
            } else if (selected == 1) {
                if (hero.gold >= 40) {
                    hero.gold -= 40;
                    hero.elixirs++;
                    msg = "Bought 1 Mana Elixir!";
                } else msg = "Not enough Gold!";
            } else if (selected == 2) {
                if (hero.gold >= 50) {
                    hero.gold -= 50;
                    hero.keys++;
                    msg = "Acquired 1 Iron Dungeon Key!";
                } else msg = "Not enough Gold!";
            } else if (selected == 3) {
                if (hero.gold >= 100) {
                    hero.gold -= 100;
                    hero.sword_level++;
                    msg = "Sword sharpened! Attack power increased!";
                } else msg = "Not enough Gold!";
            } else if (selected == 4) {
                current_state = STATE_OVERWORLD;
                hero.x = shop.door_x;
                hero.y = shop.door_y + 2;
                break;
            }
        }
    }
    timeout(0);
}

// -------------------------------------------------------------
// REST INN SCENE
// -------------------------------------------------------------
void run_inn_scene(Player& hero, GameState& current_state, Building& inn) {
    timeout(-1);
    string msg = "Rest your wounds for 20 Gold.";

    while (current_state == STATE_INN_INTERIOR) {
        clear();

        for (int i = 0; i < COLS; i++) {
            mvprintw(0, i, "=");
            mvprintw(LINES - 1, i, "=");
        }
        mvprintw(1, COLS / 2 - 8, "=== VILLAGE INN ===");

        mvprintw(4, COLS / 2 - 2, "[INNKEEPER]");
        mvprintw(6, 10, "HP: %d/%d | MP: %d/%d | GOLD: %d G", hero.health, hero.max_health, hero.mana, hero.max_mana, hero.gold);
        mvprintw(8, 10, ">> %s", msg.c_str());

        mvprintw(11, 14, "[1] Rest & Sleep (Full HP/MP Recovery) - 20 Gold");
        mvprintw(13, 14, "[2] Leave Inn");

        mvprintw(LINES - 3, 10, "[1/2] or [Z]: Select | [X]: Leave");
        refresh();

        int ch = getch();
        if (ch == '1' || ch == 'z' || ch == 'Z' || ch == 10) {
            if (hero.gold >= 20) {
                hero.gold -= 20;
                hero.health = hero.max_health;
                hero.mana = hero.max_mana;
                msg = "You slept soundly. HP and MP fully restored!";
            } else {
                msg = "Not enough gold to rent a bed!";
            }
        } else if (ch == '2' || ch == 'x' || ch == 'X' || ch == 'c' || ch == 'C') {
            current_state = STATE_OVERWORLD;
            hero.x = inn.door_x;
            hero.y = inn.door_y + 2;
            break;
        }
    }
    timeout(0);
}

// -------------------------------------------------------------
// ANCIENT CRYPT SCENE
// -------------------------------------------------------------
void run_crypt_scene(Player& hero, GameState& current_state, Building& crypt) {
    Chest crypt_chest(COLS / 2 - 4, 4, true);
    hero.x = COLS / 2 - 3;
    hero.y = LINES - 7;
    string crypt_msg = "A sealed iron chest sits upon an altar.";

    while (current_state == STATE_CRYPT_INTERIOR) {
        clear();

        for (int r = 2; r < LINES - 2; r++) {
            for (int c = 10; c < COLS - 10; c++) {
                if (r == 2 || r == LINES - 3 || c == 10 || c == COLS - 11) mvprintw(r, c, "%");
                else mvprintw(r, c, " ");
            }
        }

        mvprintw(4, 14, "[SARCOPHAGUS]");
        mvprintw(4, COLS - 26, "[SARCOPHAGUS]");
        mvprintw(LINES - 3, COLS / 2 - 4, "[EXIT GATE]");

        crypt_chest.draw();
        hero.draw();

        mvprintw(0, 0, "ANCIENT CRYPT | KEYS: %d | %s", hero.keys, crypt_msg.c_str());
        refresh();

        int ch = getch();
        int nx = hero.x;
        int ny = hero.y;

        switch (ch) {
            case KEY_UP:    ny -= 1; break;
            case KEY_DOWN:  ny += 1; break;
            case KEY_LEFT:  nx -= 2; break;
            case KEY_RIGHT: nx += 2; break;
            case 'x':
            case 'X':
            case 'q':
                current_state = STATE_OVERWORLD;
                hero.x = crypt.door_x;
                hero.y = crypt.door_y + 2;
                break;
        }

        if (nx >= 12 && nx + hero.width <= COLS - 12 && ny >= 3 && ny + hero.height <= LINES - 3) {
            hero.x = nx;
            hero.y = ny;
        }

        if (!crypt_chest.opened && check_collision(hero.x, hero.y, hero.width, hero.height, crypt_chest)) {
            if (crypt_chest.locked) {
                if (hero.keys > 0) {
                    hero.keys--;
                    crypt_chest.locked = false;
                    crypt_chest.opened = true;
                    hero.gold += 250;
                    crypt_msg = "Unlocked Crypt Chest! Gained +250 Gold!";
                } else {
                    crypt_msg = "LOCKED! Requires an Iron Key from the Trader.";
                }
            }
        }

        if (hero.y + hero.height >= LINES - 3 && abs(hero.x - (COLS / 2 - 3)) <= 4) {
            current_state = STATE_OVERWORLD;
            hero.x = crypt.door_x;
            hero.y = crypt.door_y + 2;
            break;
        }

        usleep(16000);
    }
}

// -------------------------------------------------------------
// COTTAGE SCENE
// -------------------------------------------------------------
void run_house_scene(Player& hero, GameState& current_state, Building& house) {
    Chest inside_chest(COLS / 2 - 4, 4, false);
    hero.x = COLS / 2 - 3;
    hero.y = LINES - 7;

    while (current_state == STATE_HOUSE_INTERIOR) {
        clear();

        for (int r = 2; r < LINES - 2; r++) {
            for (int c = 10; c < COLS - 10; c++) {
                if (r == 2 || r == LINES - 3 || c == 10 || c == COLS - 11) mvprintw(r, c, "#");
                else mvprintw(r, c, " ");
            }
        }

        mvprintw(4, 14, "[BED]");
        mvprintw(4, COLS - 24, "[BOOKSHELF]");
        mvprintw(LINES - 3, COLS / 2 - 4, "[EXIT DOOR]");

        inside_chest.draw();
        hero.draw();

        mvprintw(0, 0, "COTTAGE INTERIOR | Walk on Chest to open | Step on [EXIT DOOR] to leave");
        refresh();

        int ch = getch();
        int nx = hero.x;
        int ny = hero.y;

        switch (ch) {
            case KEY_UP:    ny -= 1; break;
            case KEY_DOWN:  ny += 1; break;
            case KEY_LEFT:  nx -= 2; break;
            case KEY_RIGHT: nx += 2; break;
            case 'x':
            case 'X':
            case 'q':
                current_state = STATE_OVERWORLD;
                hero.x = house.door_x;
                hero.y = house.door_y + 2;
                break;
        }

        if (nx >= 12 && nx + hero.width <= COLS - 12 && ny >= 3 && ny + hero.height <= LINES - 3) {
            hero.x = nx;
            hero.y = ny;
        }

        if (!inside_chest.opened && check_collision(hero.x, hero.y, hero.width, hero.height, inside_chest)) {
            inside_chest.opened = true;
            hero.gold += 120;
        }

        if (hero.y + hero.height >= LINES - 3 && abs(hero.x - (COLS / 2 - 3)) <= 4) {
            current_state = STATE_OVERWORLD;
            hero.x = house.door_x;
            hero.y = house.door_y + 2;
            break;
        }

        usleep(16000);
    }
}

// -------------------------------------------------------------
// MAIN GAME LOOP
// -------------------------------------------------------------
int main() {
    srand(static_cast<unsigned int>(time(nullptr)));
    init_terminal();

    printw("===============================================================\n");
    printw("                   TEXT-FREE ACTION RPG                        \n");
    printw("===============================================================\n\n");
    printw(" Controls:\n");
    printw("   [ARROWS] : Move Hero / Navigate Menus\n");
    printw("   [Z]      : Action / Fish / Talk / Enter Door / Confirm\n");
    printw("   [X]      : Back / Cancel / Exit Interior\n");
    printw("   [C]      : Flee from Battle\n\n");
    printw(" Press 'Z' to start...\n");
    refresh();

    while (true) {
        int k = getch();
        if (k == 'z' || k == 'Z' || k == 10) break;
    }

    // Spawn on open road, safely away from any obstacle
    Player hero(24, 16);

    vector<shared_ptr<Building>> buildings;
    buildings.push_back(make_shared<Building>(8, 6, BLDG_HOUSE));
    buildings.push_back(make_shared<Building>(54, 6, BLDG_SHOP));
    buildings.push_back(make_shared<Building>(8, 20, BLDG_INN));
    buildings.push_back(make_shared<Building>(54, 20, BLDG_CRYPT));

    Villager elder(30, 16, "Beware the southern ruins! Goblins lurk there.");
    StrayDog dog(28, 17);

    vector<shared_ptr<Goblin>> goblins;
    goblins.push_back(make_shared<Goblin>(62, 18));
    goblins.push_back(make_shared<Goblin>(18, 18));

    GameState state = STATE_OVERWORLD;
    string dialogue_box = "";
    int anim_frame = 0;

    while (state != STATE_GAMEOVER) {
        if (state == STATE_OVERWORLD) {
            clear();
            anim_frame++;

            if (hero.invulnerable_ticks > 0) hero.invulnerable_ticks--;

            elder.wander();
            dog.wander();

            draw_living_overworld(anim_frame);
            for (const auto& b : buildings) b->draw();
            elder.draw();
            dog.draw();
            for (const auto& g : goblins) g->draw();
            hero.draw();

            if (!dialogue_box.empty()) {
                mvprintw(LINES - 2, 2, ">> %s [Z: Close]", dialogue_box.c_str());
            } else {
                mvprintw(0, 0, "HP: %d/%d | MP: %d/%d | GOLD: %d G | FISH: %d | POTIONS: %d | KEYS: %d",
                         hero.health, hero.max_health, hero.mana, hero.max_mana, hero.gold, hero.fish_count, hero.potions, hero.keys);
            }

            refresh();

            int ch = getch();
            if (ch == 'q' || ch == 'Q') break;

            if (!dialogue_box.empty() && (ch == 'z' || ch == 'Z' || ch == 10 || ch == 'x' || ch == 'X')) {
                dialogue_box = "";
                continue;
            }

            int nx = hero.x;
            int ny = hero.y;

            switch (ch) {
                case KEY_UP:    ny -= 1; break;
                case KEY_DOWN:  ny += 1; break;
                case KEY_LEFT:  nx -= 2; break;
                case KEY_RIGHT: nx += 2; break;
            }

            // Fishing Dock Trigger (Northwest Lake)
            if ((ch == 'z' || ch == 'Z') && hero.x >= 2 && hero.x <= 24 && hero.y >= 5 && hero.y <= 8) {
                run_fishing_minigame(hero);
                continue;
            }

            // Villager Interaction
            if ((ch == 'z' || ch == 'Z') && abs(hero.x - elder.x) <= 4 && abs(hero.y - elder.y) <= 2) {
                dialogue_box = elder.dialogue;
                continue;
            }

            // Door Transitions
            shared_ptr<Building> entered = nullptr;
            for (const auto& b : buildings) {
                if (b->is_standing_at_door(hero.x, hero.y, hero.width, hero.height)) {
                    if (ch == 'z' || ch == 'Z' || ny < hero.y) {
                        entered = b;
                        break;
                    }
                }
            }

            if (entered) {
                if (entered->type == BLDG_SHOP) {
                    state = STATE_SHOP_INTERIOR;
                    run_shop_scene(hero, state, *entered);
                } else if (entered->type == BLDG_INN) {
                    state = STATE_INN_INTERIOR;
                    run_inn_scene(hero, state, *entered);
                } else if (entered->type == BLDG_CRYPT) {
                    state = STATE_CRYPT_INTERIOR;
                    run_crypt_scene(hero, state, *entered);
                } else {
                    state = STATE_HOUSE_INTERIOR;
                    run_house_scene(hero, state, *entered);
                }
                continue;
            }

            // Building Collisions
            bool blocked = false;
            for (const auto& b : buildings) {
                if (check_collision(nx, ny, hero.width, hero.height, *b)) {
                    blocked = true;
                    break;
                }
            }

            // Lake Water Barrier Collision
            if (nx >= 2 && nx <= 24 && ny >= 3 && ny <= 5) blocked = true;

            // Well Barrier Collision
            if (nx >= 35 && nx <= 46 && ny >= 14 && ny <= 18) blocked = true;

            // Screen Boundaries
            if (!blocked && nx >= 2 && nx + hero.width < COLS - 2 && ny >= 3 && ny + hero.height < LINES - 2) {
                hero.x = nx;
                hero.y = ny;
            }

            // Goblin AI Tracking & Battle
            for (auto& g : goblins) {
                if (g->active) {
                    g->track_and_move(hero.x, hero.y, buildings);

                    if (hero.invulnerable_ticks == 0 && check_collision(hero.x, hero.y, hero.width, hero.height, *g)) {
                        state = STATE_BATTLE;
                        run_battle_loop(hero, *g, state);
                        break;
                    }
                }
            }

            usleep(16000); // 60 FPS
        }
    }

    if (state == STATE_GAMEOVER) {
        clear();
        mvprintw(LINES / 2, COLS / 2 - 10, "=== GAME OVER ===");
        mvprintw(LINES / 2 + 1, COLS / 2 - 14, "You were defeated in battle.");
        refresh();
        sleep(2);
    }

    endwin();
    return 0;
}
