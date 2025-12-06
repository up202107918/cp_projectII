#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <omp.h>

#define EMPTY 0
#define ROCK 1
#define RABBIT 2
#define FOX 3

#define LOCK_SIZE 65536
#define LOCK_MASK 0xFFFF

typedef struct {
    int type;
    int proc_age;
    int food_age;
} Cell;

int GEN_PROC_RABBITS, GEN_PROC_FOXES, GEN_FOOD_FOXES, N_GENM, R, C, N;
Cell *grid1;
Cell *grid2;
omp_lock_t locks[LOCK_SIZE];

void init_grids() {
    grid1 = (Cell *)calloc(R * C, sizeof(Cell));
    grid2 = (Cell *)calloc(R * C, sizeof(Cell));
    
    #pragma omp parallel for
    for (int i = 0; i < LOCK_SIZE; i++) {
        omp_init_lock(&locks[i]);
    }
}

void destroy_grids() {
    #pragma omp parallel for
    for (int i = 0; i < LOCK_SIZE; i++) {
        omp_destroy_lock(&locks[i]);
    }
    free(grid1);
    free(grid2);
}

int get_adjacent_index(int gen, int r, int c, int p_count) {
    return (gen + r + c) % p_count;
}

void solve_rabbit_conflict(Cell *dest, int proc_age) {
    // Assumes lock is held
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
    // Assumes lock is held
    if (dest->type == FOX) {
        if (proc_age > dest->proc_age) {
            dest->proc_age = proc_age;
            dest->food_age = food_age;
        } else if (proc_age == dest->proc_age) {
            if (food_age < dest->food_age) {
                dest->food_age = food_age;
            }
        }
    } else {
        // Overwrite Empty or Rabbit
        dest->type = FOX;
        dest->proc_age = proc_age;
        dest->food_age = food_age;
    }
}

int main(int argc, char *argv[]) {
    omp_set_dynamic(0); // Disable dynamic teams 

    // Set number of threads from command line argument
    if (argc > 1) {
        int n_threads = atoi(argv[1]); // Number of threads from command line
        if (n_threads <= 0) {
            fprintf(stderr, "Uso: %s <num_threads_positivo>\n", argv[0]);
            exit(EXIT_FAILURE);
        }
        omp_set_num_threads(n_threads);
    }
    else {
        omp_set_num_threads(1); // Default to 1 thread
    }

    // Read input
    if (scanf("%d %d %d %d %d %d %d", &GEN_PROC_RABBITS, &GEN_PROC_FOXES, &GEN_FOOD_FOXES, &N_GENM, &R, &C, &N) != 7) {
        return 1;
    }

    init_grids();

    for (int k = 0; k < N; k++) {
        char type[10];
        int r, c;
        scanf("%s %d %d", type, &r, &c);
        int idx = r * C + c;
        if (type[0] == 'R') {
            if (type[1] == 'O') grid1[idx].type = ROCK;
            else grid1[idx].type = RABBIT;
        }
        else if (type[0] == 'F') grid1[idx].type = FOX;
    }

    double start_time = omp_get_wtime(); // Start timing 

    int dr[] = {-1, 0, 1, 0}; // N, E, S, W
    int dc[] = {0, 1, 0, -1};

    #pragma omp parallel
    {
        for (int gen = 0; gen < N_GENM; gen++) {
            
            // ================= PHASE 1: RABBITS =================
            // Input: grid1, Output: grid2
            
            #pragma omp for
            for (int k = 0; k < R * C; k++) {
                // Initialize grid2 with static elements from grid1
                if (grid1[k].type == ROCK) {
                    grid2[k] = grid1[k];
                } else if (grid1[k].type == FOX) {
                    grid2[k] = grid1[k];
                } else {
                    grid2[k].type = EMPTY;
                    grid2[k].proc_age = 0;
                    grid2[k].food_age = 0;
                }
            }

            #pragma omp for schedule(guided)
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

                        // Apply Move
                        int next_idx = next_r * C + next_c;
                        if (moved) {
                            // Move to next_r, next_c
                            omp_set_lock(&locks[next_idx & LOCK_MASK]);
                            solve_rabbit_conflict(&grid2[next_idx], new_proc_age);
                            omp_unset_lock(&locks[next_idx & LOCK_MASK]);

                            if (baby) {
                                // Leave baby at old position
                                omp_set_lock(&locks[idx & LOCK_MASK]);
                                solve_rabbit_conflict(&grid2[idx], 0);
                                omp_unset_lock(&locks[idx & LOCK_MASK]);
                            }
                        } else {
                            // Stay at i, j
                            omp_set_lock(&locks[idx & LOCK_MASK]);
                            solve_rabbit_conflict(&grid2[idx], new_proc_age);
                            omp_unset_lock(&locks[idx & LOCK_MASK]);
                        }
                    }
                }
            }

            // ================= PHASE 2: FOXES =================
            // Input: grid2, Output: grid1
            
            #pragma omp for
            for (int k = 0; k < R * C; k++) {
                // Initialize grid1 with static elements from grid2
                if (grid2[k].type == ROCK) {
                    grid1[k] = grid2[k];
                } else if (grid2[k].type == RABBIT) {
                    grid1[k] = grid2[k];
                } else {
                    grid1[k].type = EMPTY;
                    grid1[k].proc_age = 0;
                    grid1[k].food_age = 0;
                }
            }

            #pragma omp for schedule(guided)
            for (int i = 0; i < R; i++) {
                for (int j = 0; j < C; j++) {
                    int idx = i * C + j;
                    if (grid2[idx].type == FOX) {
                        int current_proc_age = grid2[idx].proc_age;
                        int current_food_age = grid2[idx].food_age;

                        int rabbit_moves[4];
                        int r_count = 0;
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
                            // No rabbit. Check starvation.
                            if (current_food_age + 1 >= GEN_FOOD_FOXES) {
                                // Die. Don't put in grid1.
                                continue;
                            }

                            // Try to move to empty
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
                            omp_set_lock(&locks[next_idx & LOCK_MASK]);
                            solve_fox_conflict(&grid1[next_idx], new_proc_age, new_food_age);
                            omp_unset_lock(&locks[next_idx & LOCK_MASK]);

                            if (baby) {
                                // Leave baby at old position
                                omp_set_lock(&locks[idx & LOCK_MASK]);
                                solve_fox_conflict(&grid1[idx], 0, 0);
                                omp_unset_lock(&locks[idx & LOCK_MASK]);
                            }
                        } else {
                            // Stay
                            omp_set_lock(&locks[idx & LOCK_MASK]);
                            solve_fox_conflict(&grid1[idx], new_proc_age, new_food_age);
                            omp_unset_lock(&locks[idx & LOCK_MASK]);
                        }
                    }
                }
            }
        }
    }
    double end_time = omp_get_wtime(); // End timing

    // Print Output
    int count = 0;
    for(int i=0; i<R*C; i++) if(grid1[i].type != EMPTY) count++;
    
    printf("%d %d %d %d %d %d %d\n", GEN_PROC_RABBITS, GEN_PROC_FOXES, GEN_FOOD_FOXES, 0, R, C, count);

    for (int i = 0; i < R; i++) {
        for (int j = 0; j < C; j++) {
            int idx = i * C + j;
            if (grid1[idx].type == ROCK) printf("ROCK %d %d\n", i, j);
            else if (grid1[idx].type == RABBIT) printf("RABBIT %d %d\n", i, j);
            else if (grid1[idx].type == FOX) printf("FOX %d %d\n", i, j);
        }
    }
    fprintf(stderr, "Execution Time: %f seconds\n", end_time - start_time);
    destroy_grids();
    return 0;
}