#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define EMPTY 0
#define ROCK 1
#define RABBIT 2
#define FOX 3

typedef struct {
    int type;
    int proc_age;
    int food_age;
} Cell; 

// Variáveis globais 
int GEN_PROC_RABBITS, GEN_PROC_FOXES, GEN_FOOD_FOXES, N_GEN, R, C, N_objects;
Cell *grid1;
Cell *grid2;

void init_grids() {
    grid1 = (Cell *)calloc(R * C, sizeof(Cell));
    grid2 = (Cell *)calloc(R * C, sizeof(Cell));
    if (!grid1 || !grid2) {
        fprintf(stderr, "Erro ao alocar memória\n");
        exit(EXIT_FAILURE);
    }
}

void destroy_grids() {
    free(grid1);
    free(grid2);
}

int get_adjacent_index(int gen, int r, int c, int p_count) {
    return (gen + r + c) % p_count;
}

void solve_rabbit_conflict(Cell *dest, int proc_age) {
    // Assumes lock is held, without locks
    if (dest->type == EMPTY) {
        dest->type = RABBIT;
        dest->proc_age = proc_age;
    } else if (dest->type == RABBIT) {
        if (proc_age > dest->proc_age) {
            dest->proc_age = proc_age;
        }
    }
}

void solve_fox_conflict(Cell *dest, int proc_age, int food_age) {
    // Assumes lock is held, without locks 
    if (dest->type == FOX) {
        if (proc_age > dest->proc_age) {
            dest->proc_age = proc_age;
            dest->food_age = food_age;
        } else if (proc_age == dest->proc_age) {
            if (food_age < dest->food_age) {
                dest->food_age = food_age;
            }
        }
    } else { //subscribe empty or rabbit
        dest->type = FOX;
        dest->proc_age = proc_age;
        dest->food_age = food_age;
    }
}

int main(void) {

    if (scanf("%d %d %d %d %d %d %d", &GEN_PROC_RABBITS, &GEN_PROC_FOXES, &GEN_FOOD_FOXES, &N_GEN, &R, &C, &N_objects) != 7) {
        fprintf(stderr, "Erro na leitura dos parâmetros iniciais\n");
        return 1;
    }

    init_grids();

    for (int k = 0; k < N_objects; k++) {
        char type[10];
        int r, c;
        if (scanf("%s %d %d", type, &r, &c) != 3) {
            fprintf(stderr, "Erro na leitura dos objetos iniciais\n");
            destroy_grids();
            return 1;
        }
        int idx = r * C + c;
        if (type[0] == 'R') {
            if (type[1] == 'O')
                grid1[idx].type = ROCK;
            else
                grid1[idx].type = RABBIT;
        } else if (type[0] == 'F')
            grid1[idx].type = FOX;
    }

    clock_t start_time = clock(); // Start timing sequencial 
    // Directions: up, right, down, left
    int dr[] = {-1, 0, 1, 0};
    int dc[] = {0, 1, 0, -1};

    for (int gen = 0; gen < N_GEN; gen++) {
        // =========== FASE 1: COELHOS ===========
        // Input: grid1, Output: grid2
        for (int k = 0; k < R * C; k++) {
            // Initialize grid2 with static elements from grid1
            if (grid1[k].type == ROCK || grid1[k].type == FOX) {
                grid2[k] = grid1[k];
            } else {
                grid2[k].type = EMPTY;
                grid2[k].proc_age = 0;
                grid2[k].food_age = 0;
            }
        }

        // Process of movement and reproduction of rabbits
        for (int i = 0; i < R; i++) {
            for (int j = 0; j < C; j++) {
                int idx = i * C + j;
                if (grid1[idx].type == RABBIT) {
                    int current_proc_age = grid1[idx].proc_age;

                    int possible[4];
                    int p_count = 0;
                    for (int k = 0; k < 4; k++) {
                        int ni = i + dr[k];
                        int nj = j + dc[k];
                        if (ni >= 0 && ni < R && nj >= 0 && nj < C) {
                            int nidx = ni * C + nj;
                            if (grid1[nidx].type == EMPTY) {
                                possible[p_count++] = k;
                            }
                        }
                    }

                    int next_r = i, next_c = j;
                    int moved = 0;

                    if (p_count > 0) {
                        int k = get_adjacent_index(gen, i, j, p_count);
                        int dir = possible[k];
                        next_r = i + dr[dir];
                        next_c = j + dc[dir];
                        moved = 1;
                    }

                    // Procreation Logic
                    int new_proc_age = current_proc_age + 1;
                    int baby = 0;

                    if (moved && new_proc_age > GEN_PROC_RABBITS) {
                        baby = 1;
                        new_proc_age = 0;
                    }
                    
                    int next_idx = next_r * C + next_c;
                    if (moved) {
                        // Move to next_r, next_c
                        solve_rabbit_conflict(&grid2[next_idx], new_proc_age);

                        if (baby) {
                            // Leave baby at old position
                            solve_rabbit_conflict(&grid2[idx], 0);
                        }
                    } else {
                        // Stay in place 
                        solve_rabbit_conflict(&grid2[idx], new_proc_age);
                    }
                }
            }
        }

        // =========== FASE 2: FOXES ===========
        // Input: grid2, Output: grid1 
        // Copy static elements and reset others from grid2 to grid1
        for (int k = 0; k < R * C; k++) {
            if (grid2[k].type == ROCK || grid2[k].type == RABBIT) {
                grid1[k] = grid2[k];
            } else {
                grid1[k].type = EMPTY;
                grid1[k].proc_age = 0;
                grid1[k].food_age = 0;
            }
        }

        // Process of movement, feeding, reproduction and death of foxes 
        for (int i = 0; i < R; i++) {
            for (int j = 0; j < C; j++) {
                int idx = i * C + j;
                if (grid2[idx].type == FOX) {
                    int current_proc_age = grid2[idx].proc_age;
                    int current_food_age = grid2[idx].food_age;

                    int rabbit_moves[4];
                    int r_count = 0;

                    // Look for adjacent rabbits
                    for (int k = 0; k < 4; k++) {
                        int ni = i + dr[k];
                        int nj = j + dc[k];
                        if (ni >= 0 && ni < R && nj >= 0 && nj < C) {
                            int nidx = ni * C + nj;
                            if (grid2[nidx].type == RABBIT) {
                                rabbit_moves[r_count++] = k;
                            }
                        }
                    }

                    int next_r = i, next_c = j;
                    int moved = 0;
                    int ate = 0;

                    if (r_count > 0) {
                        int k = get_adjacent_index(gen, i, j, r_count);
                        int dir = rabbit_moves[k];
                        next_r = i + dr[dir];
                        next_c = j + dc[dir];
                        moved = 1;
                        ate = 1;
                    } else {
                        // No prey found, try to move to empty
                        if (current_food_age + 1 >= GEN_FOOD_FOXES) {
                            // Die. Don't put in grid1.
                            continue;
                        }

                        // Try to move to empty cell
                        int empty_moves[4];
                        int e_count = 0;
                        for (int k = 0; k < 4; k++) {
                            int ni = i + dr[k];
                            int nj = j + dc[k];
                            if (ni >= 0 && ni < R && nj >= 0 && nj < C) {
                                int nidx = ni * C + nj;
                                if (grid2[nidx].type == EMPTY) {
                                    empty_moves[e_count++] = k;
                                }
                            }
                        }

                        if (e_count > 0) {
                            int k = get_adjacent_index(gen, i, j, e_count);
                            int dir = empty_moves[k];
                            next_r = i + dr[dir];
                            next_c = j + dc[dir];
                            moved = 1;
                        }
                    }

                    // Procreation Logic
                    int new_proc_age = current_proc_age + 1;
                    int new_food_age = ate ? 0 : current_food_age + 1;
                    int baby = 0;

                    if (moved && new_proc_age > GEN_PROC_FOXES) {
                        baby = 1;
                        new_proc_age = 0;
                    }

                    int next_idx = next_r * C + next_c;

                    if (moved) {
                        // Move to next_r, next_c
                        solve_fox_conflict(&grid1[next_idx], new_proc_age, new_food_age);

                        if (baby) {
                            // Leave baby at old position
                            solve_fox_conflict(&grid1[idx], 0, 0);
                        }
                    } else {
                        // Stay in place 
                        solve_fox_conflict(&grid1[idx], new_proc_age, new_food_age);
                    }
                }
            }
        }
    }
    // =========== FASE 3: FINALIZATION ===========
    clock_t end_time = clock(); // End timing sequencial
    double elapsed_ms = ((double)(end_time - start_time)) / (double)CLOCKS_PER_SEC * 1000.0;

    // Output final state of grid1 
    int count = 0;
    for (int i = 0; i < R * C; i++) {
        if (grid1[i].type != EMPTY) {
            count++;
        }
    }

    printf("%d %d %d %d %d %d %d\n", GEN_PROC_RABBITS, GEN_PROC_FOXES, GEN_FOOD_FOXES, 0, R, C, count);

    // Print each object 
    for (int i = 0; i < R; i++) {
        for (int j = 0; j < C; j++) {
            int idx = i * C + j;
            if (grid1[idx].type == ROCK)
                printf("ROCK %d %d\n", i, j);
            else if (grid1[idx].type == RABBIT)
                printf("RABBIT %d %d\n", i, j);
            else if (grid1[idx].type == FOX)
                printf("FOX %d %d\n", i, j);
        }
    }

    fprintf(stderr, "Execution Time (sequential): %.3f milliseconds\n", elapsed_ms);

    // Clean up
    destroy_grids();
    return 0;
}