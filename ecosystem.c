#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <omp.h>
#include <time.h>


typedef enum {
    CELL_EMPTY = 0,
    CELL_ROCK,
    CELL_RABBIT,
    CELL_FOX
} CellType;

typedef struct {
    CellType type;
    int proc_age;
    int food_age;
} Cell;

// Global variables
int GEN_PROC_RABBITS, GEN_PROC_FOXES, GEN_FOOD_FOXES, N_GEN, R, C, N_objects;
Cell *world_curr = NULL, *world_next = NULL;

#define IDX(x, y) ((x) * C + (y))

void init_world(Cell *w, int R, int C) {
    #pragma omp parallel for
    for (int i = 0; i < R * C; i++) {
        w[i].type = CELL_EMPTY;
        w[i].proc_age = 0;
        w[i].food_age = 0;
    }
}

void read_input() {
    if (scanf("%d %d %d %d %d %d %d",
              &GEN_PROC_RABBITS,
              &GEN_PROC_FOXES,
              &GEN_FOOD_FOXES,
              &N_GEN,
              &R, &C,
              &N_objects) != 7) {
        fprintf(stderr, "Erro ao ler parâmetros iniciais.\n");
        exit(EXIT_FAILURE);
    }
    world_curr = (Cell *) malloc(R * C * sizeof(Cell));
    world_next = (Cell *) malloc(R * C * sizeof(Cell));
    if (!world_curr || !world_next) {
        fprintf(stderr, "Erro ao alocar memória para o mundo.\n");
        exit(EXIT_FAILURE);
    }
    init_world(world_curr, R, C);
    init_world(world_next, R, C);
    for (int i = 0; i < N_objects; i++) {
        char obj[16];
        int x, y;
        if (scanf("%15s %d %d", obj, &x, &y) != 3) {
            fprintf(stderr, "Erro ao ler objeto %d.\n", i);
            exit(EXIT_FAILURE);
        }
        if (x < 0 || x >= R || y < 0 || y >= C) {
            fprintf(stderr, "Coordenadas fora dos limites: %d %d\n", x, y);
            exit(EXIT_FAILURE);
        }
        Cell *cell = &world_curr[IDX(x, y)];
        if (strcmp(obj, "ROCK") == 0) cell->type = CELL_ROCK;
        else if (strcmp(obj, "RABBIT") == 0) cell->type = CELL_RABBIT;
        else if (strcmp(obj, "FOX") == 0) cell->type = CELL_FOX;
        else {
            fprintf(stderr, "Objeto desconhecido: %s\n", obj);
            exit(EXIT_FAILURE);
        }
        cell->proc_age = 0;
        cell->food_age = 0;
    }
}

typedef struct {
    int proc_age;
    int food_age;
    int from_x, from_y;
    int valid;
    int ate;
} MoveInfo;

const int dx[4] = {-1, 0, 1, 0};
const int dy[4] = {0, 1, 0, -1};

int in_bounds(int x, int y) {
    return x >= 0 && x < R && y >= 0 && y < C;
}

void move_rabbits(int gen) {
    init_world(world_next, R, C);
    for (int i = 0; i < R * C; i++) {
        if (world_curr[i].type == CELL_ROCK) world_next[i] = world_curr[i];
    }
    MoveInfo *move_map = calloc(R * C, sizeof(MoveInfo));
    for (int i = 0; i < R * C; i++) move_map[i].valid = 0;
    #pragma omp parallel for collapse(2)
    for (int x = 0; x < R; x++) {
        for (int y = 0; y < C; y++) {
            Cell *cell = &world_curr[IDX(x, y)];
            if (cell->type != CELL_RABBIT) continue;
            int empty_dirs[4], empty_count = 0;
            for (int d = 0; d < 4; d++) {
                int nx = x + dx[d], ny = y + dy[d];
                if (in_bounds(nx, ny) && world_curr[IDX(nx, ny)].type == CELL_EMPTY) {
                    empty_dirs[empty_count++] = d;
                }
            }
            int tx = x, ty = y, moved = 0, tdir = -1;
            if (empty_count > 0) {
                int sel = (gen + x + y) % empty_count;
                tdir = empty_dirs[sel];
                tx = x + dx[tdir];
                ty = y + dy[tdir];
                moved = 1;
            }
            int idx = IDX(tx, ty);
            #pragma omp critical
            {
            if (!move_map[idx].valid || cell->proc_age > move_map[idx].proc_age) {
                move_map[idx].proc_age = cell->proc_age;
                move_map[idx].from_x = x;
                move_map[idx].from_y = y;
                move_map[idx].valid = 1;
            }
            }
        }
    }
    for (int x = 0; x < R; x++) {
        for (int y = 0; y < C; y++) {
            int idx = IDX(x, y);
            if (!move_map[idx].valid) continue;
            int fx = move_map[idx].from_x, fy = move_map[idx].from_y;
            Cell *src = &world_curr[IDX(fx, fy)];
            Cell *dst = &world_next[idx];
            dst->type = CELL_RABBIT;
            if ((src->proc_age + 1) >= GEN_PROC_RABBITS && (x != fx || y != fy)) {
                Cell *old = &world_next[IDX(fx, fy)];
                old->type = CELL_RABBIT;
                old->proc_age = 0;
                old->food_age = 0;
                dst->proc_age = 0;
                dst->food_age = 0;
            } else {
                dst->proc_age = src->proc_age + 1;
                dst->food_age = 0;
            }
        }
    }
    free(move_map);
}

void move_foxes(int gen) {
    for (int i = 0; i < R * C; i++) {
        if (world_curr[i].type == CELL_ROCK) world_next[i] = world_curr[i];
        if (world_next[i].type == CELL_RABBIT) continue;
        world_next[i].type = CELL_EMPTY;
        world_next[i].proc_age = 0;
        world_next[i].food_age = 0;
    }
    MoveInfo *move_map = calloc(R * C, sizeof(MoveInfo));
    for (int i = 0; i < R * C; i++) move_map[i].valid = 0;
    #pragma omp parallel for collapse(2)
    for (int x = 0; x < R; x++) {
        for (int y = 0; y < C; y++) {
            Cell *cell = &world_curr[IDX(x, y)];
            if (cell->type != CELL_FOX) continue;
            if (cell->food_age >= GEN_FOOD_FOXES) continue;
            int rabbit_dirs[4], rabbit_count = 0;
            int empty_dirs[4], empty_count = 0;
            for (int d = 0; d < 4; d++) {
                int nx = x + dx[d], ny = y + dy[d];
                if (!in_bounds(nx, ny)) continue;
                if (world_curr[IDX(nx, ny)].type == CELL_RABBIT) rabbit_dirs[rabbit_count++] = d;
                else if (world_curr[IDX(nx, ny)].type == CELL_EMPTY && world_next[IDX(nx, ny)].type != CELL_RABBIT) empty_dirs[empty_count++] = d;
            }
            int tx = x, ty = y, ate = 0, moved = 0, tdir = -1;
            int new_proc_age = cell->proc_age + 1;
            int new_food_age = cell->food_age + 1;
            if (rabbit_count > 0) {
                int sel = (gen + x + y) % rabbit_count;
                tdir = rabbit_dirs[sel];
                tx = x + dx[tdir];
                ty = y + dy[tdir];
                ate = 1;
                new_food_age = 0;
                moved = 1;
            } else if (empty_count > 0) {
                int sel = (gen + x + y) % empty_count;
                tdir = empty_dirs[sel];
                tx = x + dx[tdir];
                ty = y + dy[tdir];
                moved = 1;
            }
            int idx = IDX(tx, ty);
            #pragma omp critical
            {
            if (!move_map[idx].valid ||
                cell->proc_age > move_map[idx].proc_age ||
                (cell->proc_age == move_map[idx].proc_age && cell->food_age < move_map[idx].food_age)) {
                move_map[idx].proc_age = new_proc_age;
                move_map[idx].food_age = new_food_age;
                move_map[idx].from_x = x;
                move_map[idx].from_y = y;
                move_map[idx].valid = 1;
                move_map[idx].ate = ate;
            }
            }
        }
    }
    for (int x = 0; x < R; x++) {
        for (int y = 0; y < C; y++) {
            int idx = IDX(x, y);
            if (!move_map[idx].valid) continue;
            int fx = move_map[idx].from_x, fy = move_map[idx].from_y;
            Cell *src = &world_curr[IDX(fx, fy)];
            Cell *dst = &world_next[idx];
            dst->type = CELL_FOX;
            if ((src->proc_age + 1) >= GEN_PROC_FOXES && (x != fx || y != fy)) {
                Cell *old = &world_next[IDX(fx, fy)];
                old->type = CELL_FOX;
                old->proc_age = 0;
                old->food_age = 0;
                dst->proc_age = 0;
                dst->food_age = move_map[idx].food_age;
            } else {
                dst->proc_age = move_map[idx].proc_age;
                dst->food_age = move_map[idx].food_age;
            }
        }
    }
    free(move_map);
}

void print_final_state(Cell *w) {
    // Count objects
    int nobj = 0;
    for (int x = 0; x < R; x++) {
        for (int y = 0; y < C; y++) {
            Cell *cell = &w[IDX(x, y)];
            if (cell->type == CELL_ROCK || cell->type == CELL_RABBIT || cell->type == CELL_FOX)
                nobj++;
        }
    }
    // Print parameters (with N_GEN = 0 as in reference outputs)
    printf("%d %d %d 0 %d %d %d\n", GEN_PROC_RABBITS, GEN_PROC_FOXES, GEN_FOOD_FOXES, R, C, nobj);
    // Print objects
    for (int x = 0; x < R; x++) {
        for (int y = 0; y < C; y++) {
            Cell *cell = &w[IDX(x, y)];
            if (cell->type == CELL_ROCK)
                printf("ROCK %d %d\n", x, y);
            else if (cell->type == CELL_RABBIT)
                printf("RABBIT %d %d\n", x, y);
            else if (cell->type == CELL_FOX)
                printf("FOX %d %d\n", x, y);
        }
    }
}

int main() {
    read_input();
    for (int gen = 0; gen < N_GEN; gen++) {
        move_rabbits(gen);
        Cell *tmp = world_curr; world_curr = world_next; world_next = tmp;
        move_foxes(gen);
        tmp = world_curr; world_curr = world_next; world_next = tmp;
    }
    print_final_state(world_curr);
    free(world_curr);
    free(world_next);
    return 0;
}