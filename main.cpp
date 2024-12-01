#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <windows.h>
#include <time.h>

// Definicoes de constantes para simbolos e configuracoes iniciais
#define HELI_SYMBOL 'H'
#define MISSILE_SYMBOL '*'
#define DINO_SYMBOL 'C'
#define HELI_START_X 10
#define HELI_START_Y 10
#define MAX_DINOSAURS 5
#define MAX_MISSILES 3
#define MAX_DEPOT_SLOTS 5
#define DEPOT_X 10
#define DEPOT_Y 18

// Estruturas que representam dinossauros e o deposito de misseis
typedef struct {
    int x, y;
    int active;
    int health;
} Dinosaur;

typedef struct {
    int* slots;
    int available_missiles;
} Depot;

// Variaveis de dificuldade do jogo
int spawn_time = 9000;
int dino_health = 1;
int depot_size = 5;

// Variaveis globais do jogo
int helicopter_x = HELI_START_X;
int helicopter_y = HELI_START_Y;
int game_running = 1;
int current_dinos = 0;
int heli_missiles = MAX_MISSILES;
int truck_loading = 0;
int elapsed_time = 0;
int player_won = 0;
int game_paused = 0;
int player_score = 0;

// Arrays para os dinossauros e o deposito
Dinosaur dinos[MAX_DINOSAURS];
Depot missile_depot;

// Mutexes e variavel de condicao para sincronizacao de threads
pthread_mutex_t screen_mutex;
pthread_mutex_t dino_mutex;
pthread_mutex_t depot_mutex;
pthread_mutex_t missile_mutex;
pthread_mutex_t time_mutex;
pthread_mutex_t game_state_mutex;
pthread_mutex_t score_mutex;
pthread_cond_t depot_condition;

// Funcao para configurar o tamanho do console
void configureConsole(int width, int height) {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    COORD bufferSize = {(SHORT)width, (SHORT)height};
    SetConsoleScreenBufferSize(hConsole, bufferSize);
    SMALL_RECT windowSize = {0, 0, (SHORT)(width - 1), (SHORT)(height - 1)};
    SetConsoleWindowInfo(hConsole, TRUE, &windowSize);
}

// Funcao para exibir o menu de selecao de dificuldade
void displayMenu() {
    system("cls");
    printf("DIFICULDADE\n");
    printf("1. Facil: Tempo de Surgimento = 8s, Vida do Dino = 1, Tamanho do Deposito = 5\n");
    printf("2. Medio: Tempo de Surgimento = 6s, Vida do Dino = 2, Tamanho do Deposito = 5\n");
    printf("3. Dificil: Tempo de Surgimento = 4s, Vida do Dino = 2, Tamanho do Deposito = 5\n");
    printf("Escolha uma opcao: ");
}

// Funcao para configurar as variaveis de dificuldade com base na escolha do jogador
void setDifficulty(int choice) {
    switch (choice) {
        case 1:
            spawn_time = 8000;
            dino_health = 1;
            depot_size = 5;
            break;
        case 2:
            spawn_time = 6000;
            dino_health = 2;
            depot_size = 5;
            break;
        case 3:
            spawn_time = 4000;
            dino_health = 2;
            depot_size = 5;
            break;
        default:
            spawn_time = 8000;
            dino_health = 1;
            depot_size = 5;
            break;
    }
}

// Funcao para mover o cursor para uma posicao especifica no console
void moveCursor(int x, int y) {
    COORD coord;
    coord.X = (SHORT)x;
    coord.Y = (SHORT)y;
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), coord);
}

// Funcao para atualizar a contagem de misseis do helicoptero na tela
void updateMissileCount() {
    pthread_mutex_lock(&screen_mutex);
    moveCursor(1, 23);
    printf("                                   ");
    moveCursor(1, 23);
    pthread_mutex_lock(&missile_mutex);
    printf("Misseis: %d", heli_missiles);
    pthread_mutex_unlock(&missile_mutex);
    pthread_mutex_unlock(&screen_mutex);
}

// Funcao para atualizar o status do deposito na tela
void updateDepotStatus() {
    pthread_mutex_lock(&screen_mutex);
    moveCursor(3, 19);
    printf("[");
    pthread_mutex_lock(&depot_mutex);
    for (int i = 0; i < depot_size; i++) {
        if (missile_depot.slots[i] == 1) {
            printf("M");
        } else {
            printf(".");
        }
    }
    printf("]");
    pthread_mutex_unlock(&depot_mutex);
    pthread_mutex_unlock(&screen_mutex);
}

// Funcao para atualizar o tempo decorrido na tela
void updateTimeDisplay() {
    pthread_mutex_lock(&screen_mutex);
    moveCursor(60, 23);
    int minutes = elapsed_time / 60;
    int seconds = elapsed_time % 60;
    printf("Tempo Sobrevivido: %02d:%02d", minutes, seconds);
    pthread_mutex_unlock(&screen_mutex);
}

// Funcao para atualizar a pontuacao do jogador na tela
void updateScoreDisplay() {
    pthread_mutex_lock(&screen_mutex);
    moveCursor(1, 24);
    printf("Pontuacao: %d", player_score);
    pthread_mutex_unlock(&screen_mutex);
}

// Funcao para exibir a mensagem de pausa
void displayPauseMessage() {
    pthread_mutex_lock(&screen_mutex);
    moveCursor(30, 10);
    printf("JOGO PAUSADO");
    pthread_mutex_unlock(&screen_mutex);
}

// Funcao para limpar a mensagem de pausa
void clearPauseMessage() {
    pthread_mutex_lock(&screen_mutex);
    moveCursor(30, 10);
    printf("                   ");
    pthread_mutex_unlock(&screen_mutex);
}

// Funcao para desenhar a cena inicial do jogo
void drawScene() {
    system("cls");
    printf("HELICOPTERO VS DINOSSAUROS\n");
    printf("Pressione ESPACO para disparar misseis.\n");
    printf("(W) e (S) para mover o Helicoptero.\n");
    printf("Pressione 'P' para pausar/continuar o jogo.\n");
    moveCursor(0, 5);
    for (int i = 0; i < 80; i++) printf("-");
    for (int i = 6; i < 20; i++) {
        moveCursor(0, i);
        printf("|");
        moveCursor(79, i);
        printf("|");
    }
    moveCursor(0, 20);
    for (int i = 0; i < 80; i++) printf("-");
    moveCursor(5, 18);
    printf("[DEPOSITO]");
    moveCursor(40, 18);
    printf("[FABRICA]");
}

// Funcao para verificar se o jogo acabou devido ao excesso de dinossauros
void checkGameOver() {
    pthread_mutex_lock(&dino_mutex);
    if (current_dinos >= MAX_DINOSAURS) {
        game_running = 0;
        pthread_mutex_unlock(&dino_mutex);
        pthread_mutex_lock(&screen_mutex);
        system("cls");
        moveCursor(10, 10);
        printf("Game Over! Muitos dinossauros a solta.\n");
        pthread_mutex_unlock(&screen_mutex);
        return;
    }
    pthread_mutex_unlock(&dino_mutex);
}

// Funcao para desenhar o helicoptero na posicao atual
void drawHelicopter(int x, int y) {
    pthread_mutex_lock(&screen_mutex);
    moveCursor(x, y);
    printf("%c", HELI_SYMBOL);
    pthread_mutex_unlock(&screen_mutex);
}

// Funcao para apagar o helicoptero da posicao anterior
void eraseHelicopter(int x, int y) {
    pthread_mutex_lock(&screen_mutex);
    moveCursor(x, y);
    printf(" ");
    pthread_mutex_unlock(&screen_mutex);
}

// Funcao para recarregar os misseis do helicoptero se estiver sobre o deposito
void reloadHelicopter() {
    pthread_mutex_lock(&depot_mutex);
    pthread_mutex_lock(&missile_mutex);
    if (helicopter_y >= DEPOT_Y && heli_missiles < MAX_MISSILES && missile_depot.available_missiles > 0 && !truck_loading) {
        int needed_missiles = MAX_MISSILES - heli_missiles;
        int missiles_to_load = (needed_missiles < missile_depot.available_missiles) ? needed_missiles : missile_depot.available_missiles;
        for (int i = 0; i < depot_size && missiles_to_load > 0; i++) {
            if (missile_depot.slots[i] == 1) {
                missile_depot.slots[i] = 0;
                missile_depot.available_missiles--;
                heli_missiles++;
                missiles_to_load--;
            }
        }
        pthread_mutex_unlock(&missile_mutex);
        pthread_mutex_unlock(&depot_mutex);
        updateMissileCount();
        updateDepotStatus();
    } else {
        pthread_mutex_unlock(&missile_mutex);
        pthread_mutex_unlock(&depot_mutex);
    }
}

// Thread que controla o helicoptero baseado na entrada do usuario
void* helicopterController(void* arg) {
    while (game_running) {
        pthread_mutex_lock(&game_state_mutex);
        if (game_paused) {
            pthread_mutex_unlock(&game_state_mutex);
            Sleep(100);
            continue;
        }
        pthread_mutex_unlock(&game_state_mutex);
        if (GetAsyncKeyState(0x57) & 0x8000 && helicopter_y > 6) {
            eraseHelicopter(helicopter_x, helicopter_y);
            helicopter_y--;
        }
        if (GetAsyncKeyState(0x53) & 0x8000 && helicopter_y < 19) {
            eraseHelicopter(helicopter_x, helicopter_y);
            helicopter_y++;
        }
        drawHelicopter(helicopter_x, helicopter_y);
        if (helicopter_y >= DEPOT_Y) {
            reloadHelicopter();
        }
        Sleep(50);
    }
    return NULL;
}

// Thread que movimenta um dinossauro na tela
void* dinosaurMovement(void* arg) {
    int dino_id = *(int*)arg;
    free(arg);
    while (dinos[dino_id].x > 1 && game_running && dinos[dino_id].active) {
        pthread_mutex_lock(&game_state_mutex);
        if (game_paused) {
            pthread_mutex_unlock(&game_state_mutex);
            Sleep(100);
            continue;
        }
        pthread_mutex_unlock(&game_state_mutex);
        pthread_mutex_lock(&screen_mutex);
        moveCursor(dinos[dino_id].x, dinos[dino_id].y);
        printf("%c", DINO_SYMBOL);
        moveCursor(dinos[dino_id].x, dinos[dino_id].y - 1);
        printf("O");
        pthread_mutex_unlock(&screen_mutex);
        Sleep(500);
        pthread_mutex_lock(&screen_mutex);
        moveCursor(dinos[dino_id].x, dinos[dino_id].y);
        printf(" ");
        moveCursor(dinos[dino_id].x, dinos[dino_id].y - 1);
        printf(" ");
        pthread_mutex_unlock(&screen_mutex);
        dinos[dino_id].x--;
        if (dinos[dino_id].x == helicopter_x && dinos[dino_id].y == helicopter_y) {
            game_running = 0;
            pthread_mutex_lock(&screen_mutex);
            system("cls");
            moveCursor(10, 10);
            printf("Helicoptero foi destruido.\n");
            pthread_mutex_unlock(&screen_mutex);
            break;
        }
    }
    pthread_mutex_lock(&dino_mutex);
    if (dinos[dino_id].active) {
        dinos[dino_id].active = 0;
        current_dinos--;
    }
    pthread_mutex_unlock(&dino_mutex);
    pthread_mutex_lock(&screen_mutex);
    moveCursor(dinos[dino_id].x, dinos[dino_id].y);
    printf(" ");
    moveCursor(dinos[dino_id].x, dinos[dino_id].y - 1);
    printf(" ");
    pthread_mutex_unlock(&screen_mutex);
    return NULL;
}

// Thread que gerencia o surgimento de dinossauros
void* dinosaurManager(void* arg) {
    Sleep(2000);
    while (game_running) {
        pthread_mutex_lock(&game_state_mutex);
        if (game_paused) {
            pthread_mutex_unlock(&game_state_mutex);
            Sleep(100);
            continue;
        }
        pthread_mutex_unlock(&game_state_mutex);
        pthread_mutex_lock(&dino_mutex);
        if (current_dinos < MAX_DINOSAURS) {
            pthread_mutex_unlock(&dino_mutex);
            pthread_t dino_thread;
            int slot = -1;
            for (int i = 0; i < MAX_DINOSAURS; i++) {
                if (!dinos[i].active) {
                    slot = i;
                    break;
                }
            }
            if (slot != -1) {
                dinos[slot].active = 1;
                dinos[slot].x = 78;
                do {
                    dinos[slot].y = (rand() % 13) + 6;
                } while (dinos[slot].y >= DEPOT_Y);
                dinos[slot].health = dino_health;
                int* dino_id = (int*)malloc(sizeof(int));
                *dino_id = slot;
                pthread_create(&dino_thread, NULL, dinosaurMovement, dino_id);
                pthread_detach(dino_thread);
                pthread_mutex_lock(&dino_mutex);
                current_dinos++;
                pthread_mutex_unlock(&dino_mutex);
                checkGameOver();
            }
        } else {
            pthread_mutex_unlock(&dino_mutex);
        }
        Sleep(spawn_time);
    }
    return NULL;
}

// Funcoes para desenhar e apagar o caminhao
void drawTruck(int x, int y) {
    pthread_mutex_lock(&screen_mutex);
    moveCursor(x, y);
    printf("T");
    pthread_mutex_unlock(&screen_mutex);
}

void eraseTruck(int x, int y) {
    pthread_mutex_lock(&screen_mutex);
    moveCursor(x, y);
    printf(" ");
    pthread_mutex_unlock(&screen_mutex);
}

// Funcao que movimenta o caminhao entre a fabrica e o deposito
void moveTruck(int start_x, int start_y, int end_x, int end_y) {
    int x = start_x, y = start_y;
    while (x != end_x || y != end_y) {
        pthread_mutex_lock(&game_state_mutex);
        if (game_paused) {
            pthread_mutex_unlock(&game_state_mutex);
            Sleep(100);
            continue;
        }
        pthread_mutex_unlock(&game_state_mutex);
        eraseTruck(x, y);
        if (x < end_x) x++;
        else if (x > end_x) x--;
        if (y < end_y) y++;
        else if (y > end_y) y--;
        drawTruck(x, y);
        Sleep(100);
    }
}

// Thread que gerencia o caminhao de abastecimento
void* truckManager(void* arg) {
    while (game_running) {
        pthread_mutex_lock(&game_state_mutex);
        if (game_paused) {
            pthread_mutex_unlock(&game_state_mutex);
            Sleep(100);
            continue;
        }
        pthread_mutex_unlock(&game_state_mutex);
        pthread_mutex_lock(&depot_mutex);
        while (missile_depot.available_missiles == depot_size) {
            pthread_cond_wait(&depot_condition, &depot_mutex);
        }
        pthread_mutex_unlock(&depot_mutex);
        moveTruck(40, 19, 11, 19);
        pthread_mutex_lock(&depot_mutex);
        truck_loading = 1;
        for (int i = 0; i < depot_size; i++) {
            if (missile_depot.slots[i] == 0) {
                missile_depot.slots[i] = 1;
                missile_depot.available_missiles++;
                pthread_mutex_unlock(&depot_mutex);
                updateDepotStatus();
                Sleep(2000);
                pthread_mutex_lock(&depot_mutex);
            }
        }
        truck_loading = 0;
        pthread_mutex_unlock(&depot_mutex);
        moveTruck(11, 19, 40, 19);
        Sleep(5000);
    }
    return NULL;
}

// Thread que controla o disparo de misseis
void* fireMissile(void* arg) {
    int missile_x = helicopter_x + 1;
    int missile_y = helicopter_y;
    while (missile_x < 79 && game_running) {
        pthread_mutex_lock(&game_state_mutex);
        if (game_paused) {
            pthread_mutex_unlock(&game_state_mutex);
            Sleep(100);
            continue;
        }
        pthread_mutex_unlock(&game_state_mutex);
        pthread_mutex_lock(&screen_mutex);
        moveCursor(missile_x, missile_y);
        printf("%c", MISSILE_SYMBOL);
        pthread_mutex_unlock(&screen_mutex);
        Sleep(50);
        pthread_mutex_lock(&screen_mutex);
        moveCursor(missile_x, missile_y);
        printf(" ");
        pthread_mutex_unlock(&screen_mutex);
        missile_x++;
        pthread_mutex_lock(&dino_mutex);
        for (int i = 0; i < MAX_DINOSAURS; i++) {
            if (dinos[i].active && missile_x == dinos[i].x && missile_y == dinos[i].y - 1) {
                dinos[i].health--;
                if (dinos[i].health <= 0) {
                    dinos[i].active = 0;
                    current_dinos--;
                    pthread_mutex_lock(&score_mutex);
                    player_score++;
                    pthread_mutex_unlock(&score_mutex);
                    updateScoreDisplay();
                    pthread_mutex_unlock(&dino_mutex);
                    pthread_mutex_lock(&screen_mutex);
                    moveCursor(dinos[i].x, dinos[i].y);
                    printf(" ");
                    moveCursor(dinos[i].x, dinos[i].y - 1);
                    printf(" ");
                    pthread_mutex_unlock(&screen_mutex);
                    return NULL;
                }
                pthread_mutex_unlock(&dino_mutex);
                return NULL;
            }
        }
        pthread_mutex_unlock(&dino_mutex);
    }
    return NULL;
}

// Thread que controla o temporizador do jogo
void* gameTimer(void* arg) {
    while (game_running) {
        pthread_mutex_lock(&game_state_mutex);
        if (game_paused) {
            pthread_mutex_unlock(&game_state_mutex);
            Sleep(1000);
            continue;
        }
        pthread_mutex_unlock(&game_state_mutex);
        Sleep(1000);
        pthread_mutex_lock(&time_mutex);
        elapsed_time++;
        pthread_mutex_unlock(&time_mutex);
        updateTimeDisplay();
        if (elapsed_time >= 300) {
            pthread_mutex_lock(&game_state_mutex);
            game_running = 0;
            player_won = 1;
            pthread_mutex_unlock(&game_state_mutex);
            break;
        }
    }
    return NULL;
}

// Funcao para exibir a mensagem de vitoria em uma caixa na tela
void displayVictoryMessage() {
    pthread_mutex_lock(&screen_mutex);
    moveCursor(0, 8);
    printf("                                                                                ");
    moveCursor(0, 9);
    printf("                                                                                ");
    int box_width = 40;
    int box_height = 5;
    int start_x = 20;
    int start_y = 8;
    moveCursor(start_x, start_y);
    printf("+");
    for (int i = 0; i < box_width; i++) printf("-");
    printf("+");
    moveCursor(start_x, start_y + 1);
    printf("|");
    for (int i = 0; i < box_width; i++) printf(" ");
    printf("|");
    moveCursor(start_x, start_y + 2);
    printf("|");
    for (int i = 0; i < box_width; i++) printf(" ");
    printf("|");
    moveCursor(start_x, start_y + 3);
    printf("|");
    for (int i = 0; i < box_width; i++) printf(" ");
    printf("|");
    moveCursor(start_x, start_y + 4);
    printf("+");
    for (int i = 0; i < box_width; i++) printf("-");
    printf("+");
    char message[] = "Voce sobreviveu 5 minutos!";
    int msg_length = strlen(message);
    int msg_x = start_x + (box_width - msg_length) / 2 + 1;
    int msg_y = start_y + 2;
    moveCursor(msg_x, msg_y);
    printf("%s", message);
    pthread_mutex_unlock(&screen_mutex);
}

// Funcao para salvar a pontuacao do jogador em um arquivo
void saveScore(int score) {
    FILE* file = fopen("game_history.txt", "a");
    if (file == NULL) {
        fprintf(stderr, "Erro ao abrir o arquivo de historico de jogo.\n");
        return;
    }
    fprintf(file, "Pontuacao: %d\n", score);
    fclose(file);
}

// Funcao para exibir o historico de jogos
void displayGameHistory() {
    FILE* file = fopen("game_history.txt", "r");
    if (file == NULL) {
        printf("Nenhum historico de jogo disponivel.\n");
        return;
    }
    printf("\nHistorico de Jogos\n");
    char line[256];
    int game_number = 1;
    while (fgets(line, sizeof(line), file)) {
        printf("Jogo %d: %s", game_number++, line);
    }
    fclose(file);
}

int main() {
    srand((unsigned int)time(NULL));
    missile_depot.slots = (int*)malloc(depot_size * sizeof(int));
    if (!missile_depot.slots) {
        fprintf(stderr, "Erro ao alocar memoria para os slots do deposito\n");
        exit(EXIT_FAILURE);
    }
    for (int i = 0; i < depot_size; i++) {
        missile_depot.slots[i] = 0;
    }
    missile_depot.available_missiles = 0;
    pthread_mutex_init(&screen_mutex, NULL);
    pthread_mutex_init(&dino_mutex, NULL);
    pthread_mutex_init(&depot_mutex, NULL);
    pthread_mutex_init(&missile_mutex, NULL);
    pthread_mutex_init(&time_mutex, NULL);
    pthread_mutex_init(&game_state_mutex, NULL);
    pthread_mutex_init(&score_mutex, NULL);
    pthread_cond_init(&depot_condition, NULL);
    int play_again = 1;
    while (play_again) {
        helicopter_x = HELI_START_X;
        helicopter_y = HELI_START_Y;
        game_running = 1;
        current_dinos = 0;
        heli_missiles = MAX_MISSILES;
        truck_loading = 0;
        elapsed_time = 0;
        player_won = 0;
        game_paused = 0;
        player_score = 0;
        for (int i = 0; i < MAX_DINOSAURS; i++) {
            dinos[i].active = 0;
        }
        for (int i = 0; i < depot_size; i++) {
            missile_depot.slots[i] = 0;
        }
        missile_depot.available_missiles = 0;
        displayMenu();
        int choice;
        scanf("%d", &choice);
        setDifficulty(choice);
        configureConsole(90, 30);
        drawScene();
        updateMissileCount();
        updateDepotStatus();
        updateTimeDisplay();
        updateScoreDisplay();
        pthread_t heli_thread, dino_manager_thread, truck_thread, timer_thread;
        pthread_create(&heli_thread, NULL, helicopterController, NULL);
        pthread_create(&dino_manager_thread, NULL, dinosaurManager, NULL);
        pthread_create(&truck_thread, NULL, truckManager, NULL);
        pthread_create(&timer_thread, NULL, gameTimer, NULL);
        while (game_running) {
            if (GetAsyncKeyState('P') & 0x8000) {
                pthread_mutex_lock(&game_state_mutex);
                game_paused = !game_paused;
                pthread_mutex_unlock(&game_state_mutex);
                if (game_paused) {
                    displayPauseMessage();
                } else {
                    clearPauseMessage();
                }
                Sleep(300);
            }
            pthread_mutex_lock(&game_state_mutex);
            if (!game_paused) {
                pthread_mutex_unlock(&game_state_mutex);
                if (GetAsyncKeyState(VK_SPACE) & 0x8000) {
                    pthread_mutex_lock(&missile_mutex);
                    if (heli_missiles > 0 && helicopter_y < DEPOT_Y) {
                        heli_missiles--;
                        pthread_mutex_unlock(&missile_mutex);
                        updateMissileCount();
                        pthread_t missile_thread;
                        pthread_create(&missile_thread, NULL, fireMissile, NULL);
                        pthread_detach(missile_thread);
                    } else {
                        pthread_mutex_unlock(&missile_mutex);
                        reloadHelicopter();
                    }
                    Sleep(500);
                }
            } else {
                pthread_mutex_unlock(&game_state_mutex);
            }
            Sleep(50);
        }
        pthread_join(heli_thread, NULL);
        pthread_join(dino_manager_thread, NULL);
        pthread_join(truck_thread, NULL);
        pthread_join(timer_thread, NULL);
        saveScore(player_score);
        if (player_won) {
            displayVictoryMessage();
        } else {
            printf("Game Over!\n");
        }
        printf("\nSua Pontuacao: %d\n", player_score);
        displayGameHistory();
        printf("\nDeseja jogar novamente? (1 - Sim, 0 - Nao): ");
        scanf("%d", &play_again);
    }
    pthread_mutex_destroy(&screen_mutex);
    pthread_mutex_destroy(&dino_mutex);
    pthread_mutex_destroy(&depot_mutex);
    pthread_mutex_destroy(&missile_mutex);
    pthread_mutex_destroy(&time_mutex);
    pthread_mutex_destroy(&game_state_mutex);
    pthread_mutex_destroy(&score_mutex);
    pthread_cond_destroy(&depot_condition);
    free(missile_depot.slots);
    system("pause");
    return 0;
}