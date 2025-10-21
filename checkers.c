// CS3503 Project 1 - BitBoard Checkers
// Build:  gcc -std=c99 -O2 -Wall -Wextra -o bitcheckers bitcheckers.c
// Run:    ./bitcheckers

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>

#ifdef _MSC_VER
#define strcasecmp _stricmp
#endif


// Phase 1: Bit Manipulation API

//32-bit utilities
typedef uint32_t U32;

typedef uint64_t U64;

// Bounds-check helper for 0..31
static int in_range32(int position){ return position >= 0 && position < 32; }

U32 SetBit(U32 value, int position){
    if(!in_range32(position)) return value;
    return value | (1u << position);
}

U32 ClearBit(U32 value, int position){
    if(!in_range32(position)) return value;
    return value & ~(1u << position);
}

U32 ToggleBit(U32 value, int position){
    if(!in_range32(position)) return value;
    return value ^ (1u << position);
}

int GetBit(U32 value, int position){
    if(!in_range32(position)) return 0;
    return (value >> position) & 1u;
}

int CountBits(U32 value){
    // Brian Kernighan's method
    int c = 0;
    while(value){ value &= (value - 1); ++c; }
    return c;
}

U32 ShiftLeft(U32 value, int positions){
    if(positions <= 0) return value;
    if(positions >= 32) return 0u; // shifting >= width is undefined; return 0 safely
    return value << positions;
}

U32 ShiftRight(U32 value, int positions){
    if(positions <= 0) return value;
    if(positions >= 32) return 0u;
    return value >> positions;
}

void PrintBinary(U32 value){
    for(int i = 31; i >= 0; --i){
        putchar((value & (1u << i)) ? '1' : '0');
        if(i % 4 == 0 && i) putchar(' ');
    }
}

void PrintHex(U32 value){
    printf("0x%08X", value);
}

// 64-bit helpers (for convenience in Phase 2)
static inline int popcount64(U64 x){
    int c = 0; while(x){ x &= (x - 1); ++c; } return c;
}

// Phase 2: Bitboard Checkers


// Board indexing: bit 0 = top-left (row 0, col 0), increases left-to-right, row-major.
// A move to the NorthEast is +9 (<< 9), NorthWest +7 (<< 7),
// SouthEast -7 (>> 7), SouthWest -9 (>> 9).

// Global masks initialized at runtime
static U64 FILE_A = 0, FILE_B = 0, FILE_G = 0, FILE_H = 0;
static U64 FILE_AB = 0, FILE_GH = 0;
static U64 ROW_0 = 0, ROW_7 = 0;
static U64 DARK_SQUARES = 0; // (row+col) % 2 == 1

static void init_masks(void){
    for(int r=0;r<8;++r){
        for(int c=0;c<8;++c){
            int idx = r*8 + c;
            U64 b = 1ull << idx;
            if(c==0) FILE_A |= b;
            if(c==1) FILE_B |= b;
            if(c==6) FILE_G |= b;
            if(c==7) FILE_H |= b;
            if(r==0) ROW_0 |= b;
            if(r==7) ROW_7 |= b;
            if( ((r + c) & 1) == 1 ) DARK_SQUARES |= b;
        }
    }
    FILE_AB = FILE_A | FILE_B;
    FILE_GH = FILE_G | FILE_H;
}

// Game state: two players (P1 = Red, moves "down"; P2 = Black, moves "up")
typedef struct {
    U64 p1_men;
    U64 p1_kings;
    U64 p2_men;
    U64 p2_kings;
    int current_player; // 1 for P1 (Red), 2 for P2 (Black)
} GameState;

static inline U64 all_pieces(const GameState* g){
    return g->p1_men | g->p1_kings | g->p2_men | g->p2_kings;
}
// Forward declarations (must be outside the struct)
// Forward declarations (outside the struct)
static int player_piece_count(const GameState* g, int player);
static int check_winner(const GameState* g);
static int parse_move_line(const char* line, int* from, int* to);
static void show_help(void){
    printf("Commands:\n");
    printf("  move:  <from> <to>    e.g., 12 21\n");
    printf("  save   <file>        save current game state to file\n");
    printf("  load   <file>        load game state from file\n");
    printf("  help                 show this help\n");
    printf("  quit                 exit the game\n");
}



static void print_board(const GameState* g){
    printf("\n   Checkers - Bitboard (bit index shown)\n");
    printf("   P1=red (r/R), P2=black (b/B). Only dark squares are used.\n\n");
    for(int r=0;r<8;++r){
        printf("%d  ", r);
        for(int c=0;c<8;++c){
            int idx = r*8 + c;
            U64 m = 1ull << idx;
            char ch = '.'; // light square
            if( ((r+c)&1) == 1 ){
                ch = '-'; // dark empty by default
                if(g->p1_men & m)   ch = 'r';
                else if(g->p1_kings & m) ch = 'R';
                else if(g->p2_men & m)   ch = 'b';
                else if(g->p2_kings & m) ch = 'B';
            }
            printf("%c ", ch);
        }
        printf("  ");
        for(int c=0;c<8;++c){
            int idx = r*8 + c;
            printf("%2d ", idx);
        }
        printf("\n");
    }
    printf("   ");
    for(int c=0;c<8;++c) printf("%d ", c);
    printf("\n\n");
}

static void print_counts(const GameState* g){
    printf("P1 men=%d kings=%d | P2 men=%d kings=%d\n",
        popcount64(g->p1_men), popcount64(g->p1_kings),
        popcount64(g->p2_men), popcount64(g->p2_kings));
}

static GameState initial_state(void){
    GameState g = {0};
    g.current_player = 1;
    // Place men on the first 3 rows (dark squares) for P2 (top) and last 3 rows for P1 (bottom)
    for(int r=0;r<3;++r){
        for(int c=0;c<8;++c){
            if(((r+c)&1)==1){ g.p2_men |= 1ull << (r*8 + c); }
        }
    }
    for(int r=5;r<8;++r){
        for(int c=0;c<8;++c){
            if(((r+c)&1)==1){ g.p1_men |= 1ull << (r*8 + c); }
        }
    }
    return g;
}

// Direction masks to prevent wrap-around on single and double steps
// Single steps: NE/NW/SE/SW
static inline U64 mask_from_before_NE(U64 bb){ return bb & ~FILE_H; }
static inline U64 mask_from_before_NW(U64 bb){ return bb & ~FILE_A; }
static inline U64 mask_from_before_SE(U64 bb){ return bb & ~FILE_H; }
static inline U64 mask_from_before_SW(U64 bb){ return bb & ~FILE_A; }

// Double steps (captures)
static inline U64 mask_from_before_NE_NE(U64 bb){ return bb & ~FILE_GH; }
static inline U64 mask_from_before_NW_NW(U64 bb){ return bb & ~FILE_AB; }
static inline U64 mask_from_before_SE_SE(U64 bb){ return bb & ~FILE_GH; }
static inline U64 mask_from_before_SW_SW(U64 bb){ return bb & ~FILE_AB; }



#ifdef _WIN32
// MinGW/Windows: provide POSIX-y case-insensitive compares if missing
#define strcasecmp  _stricmp
#define strncasecmp _strnicmp
#endif

// forward declaration so calls compile cleanly even if the def is below
static int square_is_dark(int sq);

static int square_is_dark(int sq){
    int r = sq / 8, c = sq % 8;
    return ((r + c) & 1) == 1;  // dark squares have odd (row+col)
}


// Check if a move is legal
// If it's a capture, then it jumps the piece to the correct square on the board
static int is_valid_move(const GameState* g, int player, int from, int to, int* captured_idx){
    *captured_idx = -1;
    if(from < 0 || from > 63 || to < 0 || to > 63) return 0;
    if(!square_is_dark(from) || !square_is_dark(to)) return 0;

    U64 from_b = 1ull << from;
    U64 to_b   = 1ull << to;

    U64 my_men = (player==1) ? g->p1_men : g->p2_men;
    U64 my_kng = (player==1) ? g->p1_kings : g->p2_kings;
    U64 op_all = (player==1) ? (g->p2_men | g->p2_kings) : (g->p1_men | g->p1_kings);
    U64 occ    = all_pieces(g);

    // From must contain my piece
    int is_king = 0;
    if (my_men & from_b) is_king = 0;
    else if (my_kng & from_b) is_king = 1;
    else return 0;

    // Destination must be empty
    if (occ & to_b) return 0;

    int d = to - from;

    // non capture step movement
    if (d == 7 || d == 9 || d == -7 || d == -9){
        // Men move only "forward" relative to the player.
        // In your game: P1 moves UP (negative d), P2 moves DOWN (positive d).
        if(!is_king){
            if(player==1 && d > 0) return 0; // P1 must be negative
            if(player==2 && d < 0) return 0; // P2 must be positive
        }
        // Prevent wrapping off the board by file
        if(d==9   && (from_b & FILE_H)) return 0;
        if(d==7   && (from_b & FILE_A)) return 0;
        if(d==-7  && (from_b & FILE_H)) return 0;
        if(d==-9  && (from_b & FILE_A)) return 0;
        return 1;
    }

    // capture step movement
    if (d == 14 || d == 18 || d == -14 || d == -18){
        int mid = (from + to) / 2;     // the jumped square
        U64 mid_b = 1ull << mid;

        // Must jump over an opponent piece
        if ((op_all & mid_b) == 0) return 0;

        // Men directional rule also applies to captures
        if(!is_king){
            if(player==1 && d > 0) return 0; // P1 captures "up" only (negative)
            if(player==2 && d < 0) return 0; // P2 captures "down" only (positive)
        }

        // Prevent edge wrap on 2-step diagonals
        if(d==18   && (from_b & FILE_GH)) return 0;
        if(d==14   && (from_b & FILE_AB)) return 0;
        if(d==-14  && (from_b & FILE_GH)) return 0;
        if(d==-18  && (from_b & FILE_AB)) return 0;

        *captured_idx = mid;
        return 1;
    }

    return 0; // not a legal delta
}


static void apply_move(GameState* g, int player, int from, int to, int captured_idx){
    U64 from_b = 1ull << from;
    U64 to_b   = 1ull << to;
    U64* my_men = (player==1) ? &g->p1_men : &g->p2_men;
    U64* my_kng = (player==1) ? &g->p1_kings : &g->p2_kings;

    int moving_is_king = ((*my_kng) & from_b) != 0ull;

    // Remove piece from origin
    if(moving_is_king) *my_kng &= ~from_b; else *my_men &= ~from_b;

    // Place at destination (may promote later)
    if(moving_is_king) *my_kng |= to_b; else *my_men |= to_b;

    // If capture, remove opponent piece
    if(captured_idx >= 0){
        U64 cap_b = 1ull << captured_idx;
        if(player==1){ g->p2_men &= ~cap_b; g->p2_kings &= ~cap_b; }
        else         { g->p1_men &= ~cap_b; g->p1_kings &= ~cap_b; }
    }



    // Promotion: only the moved piece, and on the correct edge row
    if (!moving_is_king) {
        if (player == 1) {
            // P1 moves UP -> promote on top row (ROW_0)
            if (to_b & ROW_0) {
                *my_men &= ~to_b;
                *my_kng |= to_b;
            }
        } else {
            // P2 moves DOWN -> promote on bottom row (ROW_7)
            if (to_b & ROW_7) {
                *my_men &= ~to_b;
                *my_kng |= to_b;
            }
        }
    }



    // Promotion: only the moved piece, and on the correct edge row
    if (!moving_is_king) {
        if (player == 1) {
            // P1 moves UP -> promote on top row (ROW_0)
            if (to_b & ROW_0) {
                *my_men &= ~to_b;
                *my_kng |= to_b;
            }
        } else {
            // P2 moves DOWN -> promote on bottom row (ROW_7)
            if (to_b & ROW_7) {
                *my_men &= ~to_b;
                *my_kng |= to_b;
            }
        }
    }

}





// Quick check if player has any legal (single or capture) move
static int player_has_any_move(const GameState* g, int player){
    U64 my_men = (player==1) ? g->p1_men : g->p2_men;
    U64 my_kng = (player==1) ? g->p1_kings : g->p2_kings;
    U64 my_all = my_men | my_kng;
    U64 op_all = (player==1) ? (g->p2_men | g->p2_kings) : (g->p1_men | g->p1_kings);
    U64 empty  = (~all_pieces(g)) & DARK_SQUARES;

    // Single steps for men (directional)
    if(player==1){
        // P1 moves UP (negative deltas): use >> shifts
        U64 men_SE = (mask_from_before_SE(my_men) >> 7) & empty; // up-right
        U64 men_SW = (mask_from_before_SW(my_men) >> 9) & empty; // up-left
        if(men_SE || men_SW) return 1;
    } else {
        // P2 moves DOWN (positive deltas): use << shifts
        U64 men_NE = (mask_from_before_NE(my_men) << 9) & empty; // down-right
        U64 men_NW = (mask_from_before_NW(my_men) << 7) & empty; // down-left
        if(men_NE || men_NW) return 1;
    }

    // Single steps for kings (both directions)
    U64 k_NE = (mask_from_before_NE(my_kng) << 9) & empty;
    U64 k_NW = (mask_from_before_NW(my_kng) << 7) & empty;
    U64 k_SE = (mask_from_before_SE(my_kng) >> 7) & empty;
    U64 k_SW = (mask_from_before_SW(my_kng) >> 9) & empty;
    if(k_NE || k_NW || k_SE || k_SW) return 1;

    // Captures for men/kings: adjacent opponent and empty beyond
    U64 from, adj, land;

    // P2 forward captures (increasing indices) and king captures that way
    if(player==2 || my_kng){
        from = mask_from_before_NE_NE(my_all);
        adj  = (from << 9) & op_all;
        land = (adj << 9) & empty;
        if(land) return 1;

        from = mask_from_before_NW_NW(my_all);
        adj  = (from << 7) & op_all;
        land = (adj << 7) & empty;
        if(land) return 1;
    }

    // P1 forward captures (decreasing indices) and king captures that way
    if(player==1 || my_kng){
        from = mask_from_before_SE_SE(my_all);
        adj  = (from >> 7) & op_all;
        land = (adj >> 7) & empty;
        if(land) return 1;

        from = mask_from_before_SW_SW(my_all);
        adj  = (from >> 9) & op_all;
        land = (adj >> 9) & empty;
        if(land) return 1;
    }

    return 0;   // <-- make sure THIS brace closes the function
}

static int player_piece_count(const GameState* g, int player){
    if(player==1) return popcount64(g->p1_men | g->p1_kings);
    else          return popcount64(g->p2_men | g->p2_kings);
}

static int check_winner(const GameState* g){
    int p1 = player_piece_count(g,1);
    int p2 = player_piece_count(g,2);
    if(p1==0) return 2;
    if(p2==0) return 1;
    // No-move = loss
    if(!player_has_any_move(g,1)) return 2;
    if(!player_has_any_move(g,2)) return 1;
    return 0;
}


// Parse a line like: "12 21" => from=12 to=21 ; returns 1 on success
static int parse_move_line(const char* line, int* from, int* to){
    // Accept optional commas or arrow like "12-21" or "12->21"
    int a=-1,b=-1;
    const char* p=line;
    // Skip spaces
    while(*p && isspace((unsigned char)*p)) ++p;
    if(!*p) return 0;
    a = atoi(p);
    while(*p && !isspace((unsigned char)*p) && *p!='-' && *p!='>' && *p!=',') ++p;
    while(*p && (isspace((unsigned char)*p) || *p=='-' || *p=='>' || *p==',')) ++p;
    if(!*p) return 0;
    b = atoi(p);
    if(a<0||a>63||b<0||b>63) return 0;
    *from=a; *to=b; return 1;
}


// --- Save/Load helpers -------------------------------------------------------
// Simple, portable text format. No extra headers required.

static int save_game(const GameState* g, const char* path){
    if(!path || !*path){ puts("Usage: save <filename>"); return 0; }
    FILE* f = fopen(path, "wb");
    if(!f){ printf("Could not open '%s' for writing.\n", path); return 0; }

    // 64-bit values written as fixed-width hex for portability.
    int ok = 1;
    ok &= (fprintf(f, "BCHK 1\n") > 0);
    ok &= (fprintf(f, "%016llx %016llx\n",
                   (unsigned long long)g->p1_men,
                   (unsigned long long)g->p1_kings) > 0);
    ok &= (fprintf(f, "%016llx %016llx\n",
                   (unsigned long long)g->p2_men,
                   (unsigned long long)g->p2_kings) > 0);
    ok &= (fprintf(f, "%d\n", g->current_player) > 0);

    fclose(f);
    if(!ok) { printf("Failed to write full save to '%s'.\n", path); return 0; }
    return 1;
}

static int load_game(GameState* g, const char* path){
    if(!path || !*path){ puts("Usage: load <filename>"); return 0; }
    FILE* f = fopen(path, "rb");
    if(!f){ printf("Could not open '%s' for reading.\n", path); return 0; }

    char magic[5] = {0};
    int ver = 0;
    unsigned long long a=0,b=0,c=0,d=0;
    int cur = 0;

    // Expect:
    // BCHK 1
    // <p1_men_hex> <p1_kings_hex>
    // <p2_men_hex> <p2_kings_hex>
    // <current_player>
    int ok = 1;
    if(fscanf(f, "%4s %d", magic, &ver) != 2) ok = 0;
    if(ok && (strcmp(magic,"BCHK")!=0 || ver!=1)) ok = 0;
    if(ok && fscanf(f, "%llx %llx", &a, &b) != 2) ok = 0;
    if(ok && fscanf(f, "%llx %llx", &c, &d) != 2) ok = 0;
    if(ok && fscanf(f, "%d", &cur) != 1) ok = 0;

    fclose(f);

    if(!ok){
        printf("'%s' is not a valid save file.\n", path);
        return 0;
    }

    g->p1_men    = (U64)a;
    g->p1_kings  = (U64)b;
    g->p2_men    = (U64)c;
    g->p2_kings  = (U64)d;
    g->current_player = (cur==1 || cur==2) ? cur : 1;

    // (Optional) quick sanity: only dark squares
    U64 all = g->p1_men | g->p1_kings | g->p2_men | g->p2_kings;
    if((all & ~DARK_SQUARES) != 0ull){
        puts("Warning: save contained pieces on light squares; continuing anyway.");
    }
    return 1;
}

// Trim helpers used by the REPL commands below
static void rstrip(char* s){
    size_t L = strlen(s);
    while(L && (s[L-1]=='\n' || s[L-1]=='\r' || isspace((unsigned char)s[L-1]))) s[--L]='\0';
}
static char* skip_spaces(char* s){
    while(*s && isspace((unsigned char)*s)) ++s;
    return s;
}



int main(void){
    init_masks();

    // Quick self-check for Phase 1 utilities (optional demo)
    printf("[Phase 1 demo] SetBit/CountBits example: ");
    U32 demo = 0; demo = SetBit(demo,3); PrintBinary(demo); printf("  ones=%d\n\n", CountBits(demo));

    GameState g = initial_state();



    char line[256];

    printf("Welcome to BitBoard Checkers! Two-player (local) game.\n");
    printf("P1 (red) moves UP; P2 (black) moves DOWN. Single jumps supported; captures optional.\n");
    printf("Bit indices (0..63) are shown to the right of the board.\n");
    show_help();

    while(1){
        print_board(&g);
        print_counts(&g);
        int winner = check_winner(&g);
        if(winner){
            printf("\n*** Player %d wins! ***\n", winner);
            break;
        }

        printf("Player %d> ", g.current_player);
        if(!fgets(line, sizeof(line), stdin)) break;
        rstrip(line);
        if(!*line) continue;

        // Built-in commands
        if(strcasecmp(line, "quit")==0){ puts("Goodbye!"); break; }
        if(strcasecmp(line, "help")==0){ show_help(); continue; }

        // save <file>
        if(strncasecmp(line, "save", 4)==0){
            char* arg = skip_spaces(line + 4);
            if(!*arg){ puts("Usage: save <filename>"); continue; }
            if(save_game(&g, arg)) printf("Saved to '%s'.\n", arg);
            else                   printf("Save failed for '%s'.\n", arg);
            continue;
        }

        // load <file>
        if(strncasecmp(line, "load", 4)==0){
            char* arg = skip_spaces(line + 4);
            if(!*arg){ puts("Usage: load <filename>"); continue; }
            if(load_game(&g, arg)) printf("Loaded from '%s'.\n", arg);
            else                   printf("Load failed for '%s'.\n", arg);
            continue;
        }

        // Otherwise, try to parse a move like "12 21", "12-21", "12->21", "12,21"
        int from=-1,to=-1;
        if(!parse_move_line(line, &from, &to)){
            printf("Could not parse command. Try: 12 21   or   save state.txt\n");
            continue;
        }

        int cap=-1;
        if(!is_valid_move(&g, g.current_player, from, to, &cap)){
            printf("Illegal move from %d to %d.\n", from, to);
            continue;
        }

        apply_move(&g, g.current_player, from, to, cap);
        g.current_player = (g.current_player==1)?2:1;
    }


    return 0;
}