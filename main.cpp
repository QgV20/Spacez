#include <SDL.h>
#include <SDL_image.h>
#include <SDL_mixer.h>
#include <SDL_ttf.h>
#include <vector>
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <string>

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
    int shakeDuration = 300;
    int score = 0;

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
    if (TTF_Init() == -1) return false;

    player = { {SCREEN_WIDTH/2 - 25, SCREEN_HEIGHT - 80, 70, 70}, loadTexture("player.png"), 5 };
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
            Bullet bullet = { {player.rect.x + player.rect.w/2 - 5, player.rect.y, 20, 40}, loadTexture("bullet.png"), 8 };
            bullets.push_back(bullet);
            Mix_PlayChannel(-1, shootSound, 0);
            lastShoot = now;
        }
    }
}

void Game::update() {
    Uint32 now = SDL_GetTicks();

    for (auto& bullet : bullets)
        bullet.rect.y -= bullet.speed;
    bullets.erase(std::remove_if(bullets.begin(), bullets.end(), [](Bullet& b) {
        return b.rect.y < 0;
    }), bullets.end());

    for (auto& enemy : enemies) {
        enemy.rect.y += enemy.speed;
        if (enemy.rect.y + enemy.rect.h >= SCREEN_HEIGHT) {
            hearts--;
            SDL_DestroyTexture(enemy.texture);
            enemy.texture = nullptr;
            startShake();
        }
    }
    enemies.erase(std::remove_if(enemies.begin(), enemies.end(), [](Enemy& e) {
        return e.texture == nullptr;
    }), enemies.end());

    if (now - lastEnemySpawnTime > 1000 - std::min(500U, (now / 10000) * 50)) {
        int numEnemies = std::min(5U, (now / 5000) + 1);
        for (int i = 0; i < numEnemies; ++i) {
            Enemy enemy = { {rand() % (SCREEN_WIDTH - 40), 0, 50, 50}, loadTexture("enemy.png"), 1 + (now / 10000) };
            enemies.push_back(enemy);
        }
        lastEnemySpawnTime = now;
    }

    for (size_t i = 0; i < bullets.size(); ++i) {
        for (size_t j = 0; j < enemies.size(); ++j) {
            if (checkCollision(bullets[i].rect, enemies[j].rect)) {
                SDL_DestroyTexture(bullets[i].texture);
                SDL_DestroyTexture(enemies[j].texture);
                bullets.erase(bullets.begin() + i);
                enemies.erase(enemies.begin() + j);
                Mix_PlayChannel(-1, explosionSound, 0);
                score += 100; // Cá»™ng Ä‘iá»ƒm
                goto skip;
            }
        }
    }
skip:
    ;
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

    for (int i = 0; i < hearts; ++i) {
        SDL_Rect heartRect = {10 + i * 40, 10, 30, 30};
        SDL_RenderCopy(renderer, heartTexture, nullptr, &heartRect);
    }


    TTF_Font* font = TTF_OpenFont("Arial.ttf", 24);
    if (font) {
        SDL_Color white = {255, 255, 255, 255};
        std::string scoreText = "Score:" + std::to_string(score);
        SDL_Surface* surfaceMessage = TTF_RenderText_Solid(font, scoreText.c_str(), white);
        SDL_Texture* message = SDL_CreateTextureFromSurface(renderer, surfaceMessage);

        SDL_Rect message_rect = {10, 50, surfaceMessage->w, surfaceMessage->h};
        SDL_RenderCopy(renderer, message, nullptr, &message_rect);

        SDL_FreeSurface(surfaceMessage);
        SDL_DestroyTexture(message);
        TTF_CloseFont(font);
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
    TTF_Quit();
    SDL_Quit();
}

void Game::showGameOver() {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    SDL_Color white = {255, 255, 255, 255};

    // Font lá»›n cho GAME OVER
    TTF_Font* fontBig = TTF_OpenFont("Arial.ttf", 48);
    // Font nhá» hÆ¡n cho Your Score
    TTF_Font* fontSmall = TTF_OpenFont("Arial.ttf", 28);

    if (!fontBig || !fontSmall) {
        SDL_Log("Failed to load font: %s", TTF_GetError());
        return;
    }

    // --- Váº½ GAME OVER ---
    SDL_Surface* surfaceGameOver = TTF_RenderText_Solid(fontBig, "GAME OVER", white);
    SDL_Texture* textureGameOver = SDL_CreateTextureFromSurface(renderer, surfaceGameOver);

    SDL_Rect rectGameOver;
    rectGameOver.x = SCREEN_WIDTH / 2 - surfaceGameOver->w / 2;
    rectGameOver.y = SCREEN_HEIGHT / 2 - surfaceGameOver->h;
    rectGameOver.w = surfaceGameOver->w;
    rectGameOver.h = surfaceGameOver->h;

    SDL_RenderCopy(renderer, textureGameOver, nullptr, &rectGameOver);

    SDL_FreeSurface(surfaceGameOver);
    SDL_DestroyTexture(textureGameOver);

    // --- Váº½ Your Score ---
    std::string scoreText = "Your Score:" + std::to_string(score);
    SDL_Surface* surfaceScore = TTF_RenderText_Solid(fontSmall, scoreText.c_str(), white);
    SDL_Texture* textureScore = SDL_CreateTextureFromSurface(renderer, surfaceScore);

    SDL_Rect rectScore;
    rectScore.x = SCREEN_WIDTH / 2 - surfaceScore->w / 2;
    rectScore.y = SCREEN_HEIGHT / 2 + 10;
    rectScore.w = surfaceScore->w;
    rectScore.h = surfaceScore->h;

    SDL_RenderCopy(renderer, textureScore, nullptr, &rectScore);

    SDL_RenderPresent(renderer);
    SDL_Delay(3000);

    SDL_FreeSurface(surfaceScore);
    SDL_DestroyTexture(textureScore);

    TTF_CloseFont(fontBig);
    TTF_CloseFont(fontSmall);
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
    return 0;;
}

