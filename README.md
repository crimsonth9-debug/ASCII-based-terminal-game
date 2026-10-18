# Text-Free Action RPG

A 2D ASCII-rendered terminal role-playing game built in C++ using `ncurses`. Explore an interactive overworld, talk to villagers, go fishing, visit local buildings, and battle roving goblins in turn-based combat.

---

## Features

* **Interactive Overworld:** Features animated water ripples, torches, roads, wandering NPCs, a stray dog, and map obstacles.


* **Building Interiors:**
* **Cottage:** A quiet interior featuring a lootable treasure chest.


* **Trading Post (Shop):** Purchase healing potions, mana elixirs, iron dungeon keys, and permanent sword upgrades.


* **Village Inn:** Spend gold to rest and fully restore health and mana.


* **Ancient Crypt:** A gated dungeon featuring a locked chest requiring an Iron Key.




* **Fishing Minigame:** Cast a line at the northwest lake dock and time your reel to catch fish and earn gold.


* **Turn-Based Combat:** Encounter wandering goblins on the overworld. Choose between melee strikes, mana-consuming magic blasts, healing items, or fleeing.


* **AABB Collision & AI:** Axis-Aligned Bounding Box collision detection for terrain/buildings and path tracking for enemies.



---

## Controls

### Overworld & Exploration

| Key | Action |
| --- | --- |
| **Arrow Keys** | Move character

 |
| **Z** | Interact / Talk / Confirm / Enter doorway

 |
| **X** / **Q** | Close dialogue / Exit building interior

 |
| **Q** | Quit game (from overworld)

 |

### Fishing Minigame

| Key | Action |
| --- | --- |
| **Z** | Reel in when prompted

 |

### Turn-Based Combat

| Key | Action |
| --- | --- |
| **Arrow Keys** | Navigate actions

 |
| **Z** / **Enter** | Execute selected action

 |
| **C** | Select / Execute Flee

 |

### Shop & Inn

| Key | Action |
| --- | --- |
| **Up / Down Arrows** | Select shop item

 |
| **Z** / **Enter** | Purchase item / Rent room

 |
| **X** / **C** | Exit shop / Leave inn

 |

---

## Prerequisites

To build and run the game, you need a C++ compiler (`g++` or `clang++`) and the `ncurses` development library.

### Installing `ncurses`

* **Ubuntu / Debian / Mint:**
```bash
sudo apt update
sudo apt install build-essential libncurses5-dev libncursesw5-dev

```


* **Arch Linux:**
```bash
sudo pacman -S ncurses base-devel

```


* **Fedora / RHEL:**
```bash
sudo dnf install gcc-c++ ncurses-devel

```


* **macOS (via Homebrew):**
```bash
brew install ncurses

```



---

## Compilation & Running

### Using the Terminal

Compile the source file using `g++` and link the `ncurses` library with the `-lncurses` flag:

```bash
g++ -std=c++17 -O2 3.cpp -o terminal_rpg -lncurses

```

Run the compiled executable:

```bash
./terminal_rpg

```

---

## Recommended Terminal Settings

* **Window Size:** At least **80 columns by 25 rows** to ensure UI elements, combat logs, and overworld boundaries render properly without clipping.


* **Terminal Type:** A standard terminal emulator supporting ANSI escape codes (e.g., GNOME Terminal, Alacritty, Kitty, Windows Terminal with WSL).
