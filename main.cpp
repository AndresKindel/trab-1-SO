#include <SFML/Graphics.hpp>
#include <iostream>
#include <vector>
#include <pthread.h>
#include <unistd.h>
#include <mutex>
#include <condition_variable>

#define MAX_DINOS 5           // Número máximo de dinossauros antes de game over
#define DEPOSIT_CAPACITY 5    // Capacidade de armazenamento de mísseis no depósito

// Variáveis de nível de dificuldade
int difficultyLevel = 1;     // Nível de dificuldade (1 - Fácil, 2 - Médio, 3 - Difícil)
int m = 3;                   // Mísseis necessários para destruir o dinossauro
int n = 5;                   // Capacidade de mísseis no helicóptero
int reloadTime = 5;          // Tempo em segundos para o caminhão recarregar a cabana
int dinosaurSpawnTime = 3;   // Tempo em segundos para gerar um novo dinossauro

// Escalas globais para sprites
const float HELICOPTER_SCALE = 0.1f; // Escala do helicóptero
const float MISSILE_SCALE = 0.02f;    // Escala do míssil
const float DINO_SCALE = 0.02f;       // Escala do dinossauro

// Estrutura para o míssil
struct Missile {
    sf::Sprite sprite;
    bool active;
};

// Estrutura para o dinossauro, com uma área da cabeça definida
struct Dinosaur {
    sf::Sprite body;      // Sprite do dinossauro
    sf::RectangleShape head; // Representa a "área da cabeça"
    int hits;             // Contador de acertos na cabeça
    bool active;
};

// Variáveis globais
int currentMissiles = n;     // Mísseis atuais no helicóptero
int depositMissiles = DEPOSIT_CAPACITY; // Mísseis no depósito
int activeDinos = 0;         // Número de dinossauros ativos na tela
bool gameOver = false;       // Indica se o jogo acabou
std::mutex mtx;              // Mutex para controlar o acesso ao depósito
std::condition_variable cv;  // Condition variable para coordenação do depósito

// Função para carregar uma textura e configurar um sprite
sf::Sprite loadSprite(const std::string& filepath, sf::Texture& texture, float scale = 1.0f) {
    if (!texture.loadFromFile(filepath)) {
        std::cerr << "Erro ao carregar imagem: " << filepath << std::endl;
    }
    sf::Sprite sprite(texture);
    sprite.setScale(scale, scale);
    return sprite;
}

// Função para configurar o nível de dificuldade
void setDifficulty(int level) {
    switch (level) {
        case 1: // Fácil
            m = 2;
            n = 5;
            reloadTime = 5;
            dinosaurSpawnTime = 4;
            break;
        case 2: // Médio
            m = 3;
            n = 3;
            reloadTime = 3;
            dinosaurSpawnTime = 3;
            break;
        case 3: // Difícil
            m = 4;
            n = 2;
            reloadTime = 2;
            dinosaurSpawnTime = 2;
            break;
    }
    currentMissiles = n; // Ajusta a capacidade inicial do helicóptero
}

// Função para controlar o recarregamento de mísseis pelo caminhão
void* truckFunction(void*) {
    while (!gameOver) {
        sleep(reloadTime);

        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [] { return depositMissiles < DEPOSIT_CAPACITY; });

        // Reabastece o depósito com a capacidade máxima
        depositMissiles = DEPOSIT_CAPACITY;
        std::cout << "Caminhão reabasteceu o depósito com mísseis.\n";

        cv.notify_all();
    }
    return nullptr;
}

// Função para gerar novos dinossauros
void* dinosaurSpawner(void* arg) {
    auto* dinos = static_cast<std::vector<Dinosaur>*>(arg);

    while (!gameOver) {
        sleep(dinosaurSpawnTime);

        std::unique_lock<std::mutex> lock(mtx);
        if (activeDinos < MAX_DINOS) {
            Dinosaur dino;
            sf::Texture dinoTexture;
            dino.body = loadSprite("dinosaur.png", dinoTexture, DINO_SCALE);
            dino.body.setPosition(rand() % 800, rand() % 600);
            dino.active = true;
            dino.hits = 0;

            // Define a área da cabeça como um retângulo sobre a parte superior do dinossauro
            dino.head.setSize(sf::Vector2f(10, 10)); // Cabeça menor proporcional ao dinossauro
            dino.head.setFillColor(sf::Color::Red); // Cor vermelha para depuração
            dino.head.setPosition(dino.body.getPosition().x + 10, dino.body.getPosition().y);

            dinos->push_back(dino);
            activeDinos++;
            std::cout << "Novo dinossauro gerado. Dinossauros ativos: " << activeDinos << std::endl;

            if (activeDinos >= MAX_DINOS) {
                gameOver = true;
                std::cout << "A Terra está condenada! Game over.\n";
            }
        }
    }
    return nullptr;
}

// Função para detectar colisões entre mísseis e a área da cabeça dos dinossauros
void checkCollisions(std::vector<Missile>& missiles, std::vector<Dinosaur>& dinos) {
    for (auto& missile : missiles) {
        if (!missile.active) continue;

        for (auto& dino : dinos) {
            if (!dino.active) continue;

            // Verifica se o helicóptero está alinhado verticalmente com a cabeça do dinossauro
            if (missile.sprite.getPosition().y == dino.head.getPosition().y &&
                missile.sprite.getGlobalBounds().intersects(dino.head.getGlobalBounds())) {
                
                missile.active = false; // Desativa o míssil
                dino.hits++; // Incrementa o contador de acertos na cabeça
                std::cout << "Tiro na cabeça! Acertos: " << dino.hits << "/" << m << "\n";

                if (dino.hits >= m) {
                    dino.active = false; // Destrói o dinossauro
                    activeDinos--;
                    std::cout << "Dinossauro destruído!\n";
                }
                break;
            } else if (missile.sprite.getGlobalBounds().intersects(dino.body.getGlobalBounds())) {
                // Tiro no corpo é ineficaz
                missile.active = false;
                std::cout << "Tiro no corpo é ineficaz.\n";
            }
        }
    }
}

// Função para verificar colisão entre helicóptero e dinossauros
bool checkHelicopterCollision(sf::Sprite& helicopter, std::vector<Dinosaur>& dinos) {
    for (auto& dino : dinos) {
        if (dino.active && helicopter.getGlobalBounds().intersects(dino.body.getGlobalBounds())) {
            return true;
        }
    }
    return false;
}

// Função principal
int main() {
    sf::RenderWindow window(sf::VideoMode(800, 600), "Jogo Helicoptero e Dinossauro");

    sf::Texture helicopterTexture, missileTexture;
    sf::Sprite helicopter = loadSprite("helicopter.png", helicopterTexture, HELICOPTER_SCALE);
    helicopter.setPosition(400, 300);

    std::vector<Missile> missiles;
    std::vector<Dinosaur> dinos;
    const int MAX_ACTIVE_MISSILES = 10;

    setDifficulty(difficultyLevel);

    pthread_t truckThread, dinoThread;
    pthread_create(&truckThread, nullptr, truckFunction, nullptr);
    pthread_create(&dinoThread, nullptr, dinosaurSpawner, &dinos);

    while (window.isOpen()) {
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed)
                window.close();
        }

        if (sf::Keyboard::isKeyPressed(sf::Keyboard::A)) {
            helicopter.move(-0.2, 0);
        }
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::D)) {
            helicopter.move(0.2, 0);
        }
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::W)) {
            helicopter.move(0, -0.2);
        }
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::S)) {
            helicopter.move(0, 0.2);
        }

        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Space) && currentMissiles > 0 && missiles.size() < MAX_ACTIVE_MISSILES) {
            Missile missile;
            missile.sprite = loadSprite("missile.png", missileTexture, MISSILE_SCALE);
            missile.sprite.setPosition(helicopter.getPosition().x + 0.00002, helicopter.getPosition().y);
            missile.active = true;
            missiles.push_back(missile);
            currentMissiles--;
        }

        std::unique_lock<std::mutex> lock(mtx);
        if (helicopter.getPosition().y > 550 && currentMissiles < n) {
            cv.wait(lock, [] { return depositMissiles > 0; });

            int missilesToRefill = std::min(depositMissiles, n - currentMissiles);
            depositMissiles -= missilesToRefill;
            currentMissiles += missilesToRefill;
            std::cout << "Reabastecido! Mísseis no helicóptero: " << currentMissiles << "\n";

            cv.notify_all();
        }
        lock.unlock();

        checkCollisions(missiles, dinos);

        if (checkHelicopterCollision(helicopter, dinos)) {
            std::cout << "Colisão entre helicóptero e dinossauro! Game over.\n";
            gameOver = true;
            window.close();
        }

        window.clear();
        window.draw(helicopter);
        for (auto& missile : missiles) {
            if (missile.active) {
                missile.sprite.move(0, -10);
                window.draw(missile.sprite);
            }
        }
        for (auto& dino : dinos) {
            if (dino.active) {
                window.draw(dino.body);
                window.draw(dino.head);
            }
        }
        window.display();
    }

    pthread_join(truckThread, nullptr);
    pthread_join(dinoThread, nullptr);

    return 0;
}
