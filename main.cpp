#include <SDL.h>
#include <SDL_image.h>
#include <SDL_mixer.h>
#include <SDL_ttf.h>
#include <vector>
#include <algorithm>
#include <cstdlib>
#include <ctime>

const int SCREEN_WIDTH = 480;
const int SCREEN_HEIGHT = 640;

struct Player {
    SDL_Rect rect;
    SDL_Texture* texture;
    int speed;
};

struct Bullet {
    SDL_Rect rect;
    SDL_Texture* texture;
    int speed;
};

struct Enemy {
    SDL_Rect rect;
    SDL_Texture* texture;
    int speed;
};

struct Game {
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    Player player;
    std::vector<Bullet> bullets;
    std::vector<Enemy> enemies;
    SDL_Texture* backgroundTexture = nullptr;
    SDL_Texture* heartTexture = nullptr;
    Mix_Chunk* shootSound = nullptr;
    Mix_Chunk* explosionSound = nullptr;
    Mix_Music* backgroundMusic = nullptr;
    Uint32 lastEnemySpawnTime = 0;
    int hearts = 5;
    bool running = true;
    bool shaking = false;
    Uint32 shakeStartTime = 0;
    int shakeDuration = 300; // rung 300ms

    bool init();
    void run();
    void handleEvents();
    void update();
    void render();
    void clean();
    void showGameOver();
    SDL_Texture* loadTexture(const char* path);
    bool checkCollision(SDL_Rect a, SDL_Rect b);
    void startShake();
    void applyShake();
};

bool Game::init() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) return false;

    window = SDL_CreateWindow("Space Shooter", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
    if (!window) return false;

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) return false;

    if (IMG_Init(IMG_INIT_PNG) != IMG_INIT_PNG) return false;
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) return false;

    player = { {SCREEN_WIDTH/2 - 25, SCREEN_HEIGHT - 80, 50, 50}, loadTexture("player.png"), 5 };
    backgroundTexture = loadTexture("background.png");
    heartTexture = loadTexture("heart.png");

    if (!backgroundTexture || !heartTexture || !player.texture) return false;

    shootSound = Mix_LoadWAV("shoot.wav");
    explosionSound = Mix_LoadWAV("explosion.wav");
    backgroundMusic = Mix_LoadMUS("background_music.mp3");
    if (backgroundMusic) Mix_PlayMusic(backgroundMusic, -1);

    lastEnemySpawnTime = SDL_GetTicks();
    srand((unsigned int)time(NULL));
    return true;
}

SDL_Texture* Game::loadTexture(const char* path) {
    SDL_Surface* surface = IMG_Load(path);
    if (!surface) return nullptr;
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    return texture;
}

bool Game::checkCollision(SDL_Rect a, SDL_Rect b) {
    return SDL_HasIntersection(&a, &b);
}

void Game::startShake() {
    shaking = true;
    shakeStartTime = SDL_GetTicks();
}

void Game::applyShake() {
    if (shaking) {
        int elapsed = SDL_GetTicks() - shakeStartTime;
        if (elapsed > shakeDuration) {
            shaking = false;
            SDL_RenderSetLogicalSize(renderer, SCREEN_WIDTH, SCREEN_HEIGHT);
        } else {
            int offsetX = rand() % 10 - 5;
            int offsetY = rand() % 10 - 5;
            SDL_RenderSetLogicalSize(renderer, SCREEN_WIDTH + offsetX, SCREEN_HEIGHT + offsetY);
        }
    }
}

void Game::handleEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) running = false;
    }

    const Uint8* keystates = SDL_GetKeyboardState(NULL);
    if (keystates[SDL_SCANCODE_LEFT] && player.rect.x > 0) player.rect.x -= player.speed;
    if (keystates[SDL_SCANCODE_RIGHT] && player.rect.x + player.rect.w < SCREEN_WIDTH) player.rect.x += player.speed;
    if (keystates[SDL_SCANCODE_UP] && player.rect.y > 0) player.rect.y -= player.speed;
    if (keystates[SDL_SCANCODE_DOWN] && player.rect.y + player.rect.h < SCREEN_HEIGHT) player.rect.y += player.speed;
    if (keystates[SDL_SCANCODE_SPACE]) {
        static Uint32 lastShoot = 0;
        Uint32 now = SDL_GetTicks();
        if (now > lastShoot + 300) {
            Bullet bullet = { {player.rect.x + player.rect.w/2 - 5, player.rect.y, 10, 20}, loadTexture("bullet.png"), 8 };
            bullets.push_back(bullet);
            Mix_PlayChannel(-1, shootSound, 0);
            lastShoot = now;
        }
    }
}

void Game::update() {
    Uint32 now = SDL_GetTicks();

    // Update bullets
    for (auto& bullet : bullets)
        bullet.rect.y -= bullet.speed;
    bullets.erase(std::remove_if(bullets.begin(), bullets.end(), [](Bullet& b) {
        return b.rect.y < 0;
    }), bullets.end());

    // Update enemies
    for (auto& enemy : enemies) {
        enemy.rect.y += enemy.speed;
        if (enemy.rect.y + enemy.rect.h >= SCREEN_HEIGHT) {
            hearts--; // mất 1 máu
            SDL_DestroyTexture(enemy.texture);
            enemy.texture = nullptr;
            startShake(); // rung màn hình
        }
    }
    enemies.erase(std::remove_if(enemies.begin(), enemies.end(), [](Enemy& e) {
        return e.texture == nullptr;
    }), enemies.end());

    // Spawn enemy mỗi 1000ms
    if (now - lastEnemySpawnTime > 1000) {
        Enemy enemy = { {rand() % (SCREEN_WIDTH - 40), 0, 40, 40}, loadTexture("enemy.png"), 1 };
        enemies.push_back(enemy);
        lastEnemySpawnTime = now;
    }

    // Xử lý va chạm đạn và enemy
    for (size_t i = 0; i < bullets.size(); ++i) {
        for (size_t j = 0; j < enemies.size(); ++j) {
            if (checkCollision(bullets[i].rect, enemies[j].rect)) {
                SDL_DestroyTexture(bullets[i].texture);
                SDL_DestroyTexture(enemies[j].texture);
                bullets.erase(bullets.begin() + i);
                enemies.erase(enemies.begin() + j);
                Mix_PlayChannel(-1, explosionSound, 0);
                goto skip;
            }
        }
    }
skip:
    ;

    // Nếu hết máu => thua
    if (hearts <= 0) {
        running = false;
        return;
    }
}

void Game::render() {
    applyShake();
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, backgroundTexture, nullptr, nullptr);
    SDL_RenderCopy(renderer, player.texture, nullptr, &player.rect);

    for (auto& bullet : bullets)
        SDL_RenderCopy(renderer, bullet.texture, nullptr, &bullet.rect);

    for (auto& enemy : enemies)
        SDL_RenderCopy(renderer, enemy.texture, nullptr, &enemy.rect);

    // Vẽ thanh máu
    for (int i = 0; i < hearts; ++i) {
        SDL_Rect heartRect = {10 + i * 40, 10, 30, 30};
        SDL_RenderCopy(renderer, heartTexture, nullptr, &heartRect);
    }

    SDL_RenderPresent(renderer);
}

void Game::clean() {
    Mix_FreeChunk(shootSound);
    Mix_FreeChunk(explosionSound);
    Mix_FreeMusic(backgroundMusic);
    SDL_DestroyTexture(backgroundTexture);
    SDL_DestroyTexture(player.texture);
    SDL_DestroyTexture(heartTexture);
    for (auto& bullet : bullets)
        SDL_DestroyTexture(bullet.texture);
    for (auto& enemy : enemies)
        SDL_DestroyTexture(enemy.texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    IMG_Quit();
    Mix_CloseAudio();
    SDL_Quit();
}

void Game::showGameOver() {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    SDL_Color white = {255, 255, 255, 255};

    if (TTF_Init() == -1) {
        SDL_Log("Failed to init TTF: %s", TTF_GetError());
        return;
    }

    TTF_Font* font = TTF_OpenFont("Arial.ttf", 48);
    if (!font) {
        SDL_Log("Failed to load font: %s", TTF_GetError());
        TTF_Quit();
        return;
    }

    SDL_Surface* surfaceMessage = TTF_RenderText_Solid(font, "GAME OVER", white);
    SDL_Texture* message = SDL_CreateTextureFromSurface(renderer, surfaceMessage);

    SDL_Rect message_rect;
    message_rect.x = SCREEN_WIDTH / 2 - surfaceMessage->w / 2;
    message_rect.y = SCREEN_HEIGHT / 2 - surfaceMessage->h / 2;
    message_rect.w = surfaceMessage->w;
    message_rect.h = surfaceMessage->h;

    SDL_RenderCopy(renderer, message, nullptr, &message_rect);
    SDL_RenderPresent(renderer);

    SDL_Delay(3000);

    SDL_FreeSurface(surfaceMessage);
    SDL_DestroyTexture(message);
    TTF_CloseFont(font);
    TTF_Quit();
}

void Game::run() {
    while (running) {
        handleEvents();
        update();
        render();
        SDL_Delay(16);
    }
    showGameOver();
}

int main(int argc, char* argv[]) {
    Game game;
    if (game.init()) {
        game.run();
    }
    game.clean();
    return 0;
}
