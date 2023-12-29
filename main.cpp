#include <cstdio> //C header
#include <cstdint>
#include <iostream>
#include <cctype>
#include <GL/glew.h> //GLEW is a loading library for OpenGL functions
#include <GLFW/glfw3.h> //Renderer
#include <cstdlib>
#include <ctime>
#include <cmath>
#include <chrono>
#include <irrKlang.h>
#include <cstring>
#include <algorithm>
using namespace irrklang;

#define GAME_WIDTH 800
#define GAME_HEIGHT 800
#define PLAYER_MAX_HEALTH 10

//timer class for recording length of events
class Timer{

public:
    std::chrono::time_point<std::chrono::high_resolution_clock> startTime;
    bool isRunning = false;

    void start(){
        startTime = std::chrono::high_resolution_clock::now();
        isRunning = true;
    }

    bool hasElapsed(long duration){
        if (isRunning == false){
            return false;
        }

        auto currentTime = std::chrono::high_resolution_clock::now();
        auto timeElapsed = std::chrono::duration_cast<std::chrono::seconds>(currentTime - startTime).count();
        return timeElapsed >= duration;
    }

    void reset(){
        isRunning = false;
    }
};

bool gameRunning = false;
bool gameOver = false;
bool bulletFired = false;
int playerMoveDirection = 0;    //+1 indicates right move, -1 indicates left move
const int maxBullets = 128;
const int maxEnemyBullets = 128;
size_t playerScore = 0;
size_t bulletCooldown = 0;
const size_t maxBulletCooldown = 10;
const size_t blinkDuration = 20;
Timer enemyRespawnTimer;
Timer titleDelayTimer;
Timer playerDeathTimer;


struct Buffer{
    uint32_t* data;
    size_t w, h;
};

struct Sprite{
    uint8_t* data;
    size_t w, h;
};

struct SpriteAnimation{
    Sprite** animationFrames;   //pointer to an array of sprite pointers
    bool loop;  //animation loops?
    size_t numFrames;
    size_t frameTime;
    size_t timeElapsed;
};

struct Player{
    size_t x,y;
    int lives;
    bool isBlinking = false;
    size_t blinkTimer = 0;

    void onHit(){
        isBlinking = true;
        blinkTimer = 0;
    }
};

//Different enemy types
enum EnemyType: uint8_t{
    enemyDead = 0,
    enemyTypeA = 1,
    enemyTypeB = 2,
    enemyTypeC = 3
};

struct Enemy{
    size_t x,y;
    size_t enemyHealth;
    size_t enemyBulletDamage;
    bool isBlinking = false;
    size_t blinkTimer = 0;
    EnemyType enemyType;

    void onHit(){
        isBlinking = true;
        blinkTimer = 0;
    };
};

struct Bullet{
    size_t x,y;
    double bulletSpeed; //+1 indicates forward, -1 indicates backward
    uint32_t bulletColour;
    EnemyType enemyType;
};

enum class GameState{
    titleScreen,
    inGame,
    gameOver
};

struct GameEngine{
    size_t gameWidth, gameHeight;
    size_t numEnemies;
    size_t numBullets;
    size_t numEnemyBullets;
    long unsigned int level;
    Enemy* enemies;  //dynamically allocated
    Player player;  //only one player
    Bullet bullets[maxBullets];
    Bullet enemyBullets[maxEnemyBullets];
};

ISoundEngine *soundEngine = createIrrKlangDevice();
GameState gameState = GameState::titleScreen;

void inputCallback(GLFWwindow* window, int key, int _, int action, int mods);
void errorCallback(int error, const char* error_disc);

void clearBuffer(Buffer* buffer, uint32_t colour);
uint32_t rgbTo32(uint8_t R, uint8_t G, uint8_t B);
void drawSpriteToBuffer(Buffer* buffer, const Sprite& sprite, uint32_t colour, size_t x, size_t y, size_t scale);
bool spritesIntersect(Sprite& sprite1, size_t x1, size_t y1, Sprite& sprite2, size_t x2, size_t y2);
void drawText(const char* text, Buffer* buffer, Sprite& spriteSheet, size_t x, size_t y, uint32_t textColour, size_t scale, bool centred = false);

int main(int argc, char* argv[])
{
    //-----VARIABLES----------
    uint32_t bgColour = rgbTo32(0, 0, 0);

    const size_t buffer_w = 800;
    const size_t buffer_h = 800;

    //-----ERROR CHECKING AND CALLBACKS-----

    GLFWwindow* window; //initialise window

    glfwSetErrorCallback(errorCallback);    //Set error callback

    if (!glfwInit()){
        return 1;
    }

    //Ensure the OpenGL context we receive from GLFW is at least 3.3
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

    //Create a window (the last two parameters are not needed)
    window = glfwCreateWindow(buffer_w, buffer_h, "This is Space Invaders.", NULL, NULL);

    if (!window){
        glfwTerminate();
        return 1;
    }

    glfwSetKeyCallback(window, inputCallback);  //Set input callback

    //Set the OpenGL context to our window
    glfwMakeContextCurrent(window);

    //Initialise GLEW and check it loaded correctly
    GLenum err = glewInit();
    if (err != GLEW_OK){
        fprintf(stderr, "Error in loading GLEW.\n");
        glfwTerminate();
        return 1;
    }

    Buffer buffer;
    buffer.w = buffer_w;
    buffer.h = buffer_h;
    buffer.data = new uint32_t[buffer.w * buffer.h]; //Allocate buffer data
    clearBuffer(&buffer, bgColour); //Write bg_colour data to buffer

    ISoundSource *gameMusic = soundEngine->addSoundSourceFromFile("sounds/bgmusic_2.wav");
    gameMusic->setDefaultVolume(0.8f);

    ISoundSource *titleMusic = soundEngine->addSoundSourceFromFile("sounds/titleScreen.wav");
    titleMusic->setDefaultVolume(0.8f);

    //ISoundSource *gameOverMusic = soundEngine->addSoundSourceFromFile("sounds/...");
    //gameOverMusic->setDefaultVolume(0.8f);

    ISoundSource *fireSound = soundEngine->addSoundSourceFromFile("sounds/Galaga_Fire.wav");
    fireSound->setDefaultVolume(0.1f);

    ISoundSource *enemyDeathSound = soundEngine->addSoundSourceFromFile("sounds/Centipede_Death.wav");
    enemyDeathSound->setDefaultVolume(0.3f);

    ISoundSource *enemyDamageSound = soundEngine->addSoundSourceFromFile("sounds/Centipede_Kill.wav");
    enemyDamageSound->setDefaultVolume(0.7f);

    ISoundSource *playerDamageSound = soundEngine->addSoundSourceFromFile("sounds/Bouncer 003.wav");
    playerDamageSound->setDefaultVolume(1.0f);

    ISoundSource *playerDeathSound = soundEngine->addSoundSourceFromFile("sounds/Defender_Death.wav");
    playerDeathSound->setDefaultVolume(1.0f);

    //-----SPRITES-----------

    Sprite enemySprites[6];

    enemySprites[0].w = 8;
    enemySprites[0].h = 8;
    enemySprites[0].data = new uint8_t[64]
    {
        0,0,0,1,1,0,0,0, // ...@@...
        0,0,1,1,1,1,0,0, // ..@@@@..
        0,1,1,1,1,1,1,0, // .@@@@@@.
        1,1,0,1,1,0,1,1, // @@.@@.@@
        1,1,1,1,1,1,1,1, // @@@@@@@@
        0,1,0,1,1,0,1,0, // .@.@@.@.
        1,0,0,0,0,0,0,1, // @......@
        0,1,0,0,0,0,1,0  // .@....@.
    };

    enemySprites[1].w = 8;
    enemySprites[1].h = 8;
    enemySprites[1].data = new uint8_t[64]
    {
        0,0,0,1,1,0,0,0, // ...@@...
        0,0,1,1,1,1,0,0, // ..@@@@..
        0,1,1,1,1,1,1,0, // .@@@@@@.
        1,1,0,1,1,0,1,1, // @@.@@.@@
        1,1,1,1,1,1,1,1, // @@@@@@@@
        0,0,1,0,0,1,0,0, // ..@..@..
        0,1,0,1,1,0,1,0, // .@.@@.@.
        1,0,1,0,0,1,0,1  // @.@..@.@
    };

    enemySprites[2].w = 11;
    enemySprites[2].h = 8;
    enemySprites[2].data = new uint8_t[88]
    {
        0,0,1,0,0,0,0,0,1,0,0, // ..@.....@..
        0,0,0,1,0,0,0,1,0,0,0, // ...@...@...
        0,0,1,1,1,1,1,1,1,0,0, // ..@@@@@@@..
        0,1,1,0,1,1,1,0,1,1,0, // .@@.@@@.@@.
        1,1,1,1,1,1,1,1,1,1,1, // @@@@@@@@@@@
        1,0,1,1,1,1,1,1,1,0,1, // @.@@@@@@@.@
        1,0,1,0,0,0,0,0,1,0,1, // @.@.....@.@
        0,0,0,1,1,0,1,1,0,0,0  // ...@@.@@...
    };

    enemySprites[3].w = 11;
    enemySprites[3].h = 8;
    enemySprites[3].data = new uint8_t[88]
    {
        0,0,1,0,0,0,0,0,1,0,0, // ..@.....@..
        1,0,0,1,0,0,0,1,0,0,1, // @..@...@..@
        1,0,1,1,1,1,1,1,1,0,1, // @.@@@@@@@.@
        1,1,1,0,1,1,1,0,1,1,1, // @@@.@@@.@@@
        1,1,1,1,1,1,1,1,1,1,1, // @@@@@@@@@@@
        0,1,1,1,1,1,1,1,1,1,0, // .@@@@@@@@@.
        0,0,1,0,0,0,0,0,1,0,0, // ..@.....@..
        0,1,0,0,0,0,0,0,0,1,0  // .@.......@.
    };

    enemySprites[4].w = 12;
    enemySprites[4].h = 8;
    enemySprites[4].data = new uint8_t[96]
    {
        0,0,0,0,1,1,1,1,0,0,0,0, // ....@@@@....
        0,1,1,1,1,1,1,1,1,1,1,0, // .@@@@@@@@@@.
        1,1,1,1,1,1,1,1,1,1,1,1, // @@@@@@@@@@@@
        1,1,1,0,0,1,1,0,0,1,1,1, // @@@..@@..@@@
        1,1,1,1,1,1,1,1,1,1,1,1, // @@@@@@@@@@@@
        0,0,0,1,1,0,0,1,1,0,0,0, // ...@@..@@...
        0,0,1,1,0,1,1,0,1,1,0,0, // ..@@.@@.@@..
        1,1,0,0,0,0,0,0,0,0,1,1  // @@........@@
    };


    enemySprites[5].w = 12;
    enemySprites[5].h = 8;
    enemySprites[5].data = new uint8_t[96]
    {
        0,0,0,0,1,1,1,1,0,0,0,0, // ....@@@@....
        0,1,1,1,1,1,1,1,1,1,1,0, // .@@@@@@@@@@.
        1,1,1,1,1,1,1,1,1,1,1,1, // @@@@@@@@@@@@
        1,1,1,0,0,1,1,0,0,1,1,1, // @@@..@@..@@@
        1,1,1,1,1,1,1,1,1,1,1,1, // @@@@@@@@@@@@
        0,0,1,1,1,0,0,1,1,1,0,0, // ..@@@..@@@..
        0,1,1,0,0,1,1,0,0,1,1,0, // .@@..@@..@@.
        0,0,1,1,0,0,0,0,1,1,0,0  // ..@@....@@..
    };

    Sprite enemyDeathSprite;
    enemyDeathSprite.w = 13;
    enemyDeathSprite.h = 7;
    enemyDeathSprite.data = new uint8_t[91]
    {
        0,1,0,0,1,0,0,0,1,0,0,1,0, // .@..@...@..@.
        0,0,1,0,0,1,0,1,0,0,1,0,0, // ..@..@.@..@..
        0,0,0,1,0,0,0,0,0,1,0,0,0, // ...@.....@...
        1,1,0,0,0,0,0,0,0,0,0,1,1, // @@.........@@
        0,0,0,1,0,0,0,0,0,1,0,0,0, // ...@.....@...
        0,0,1,0,0,1,0,1,0,0,1,0,0, // ..@..@.@..@..
        0,1,0,0,1,0,0,0,1,0,0,1,0  // .@..@...@..@.
    };

    SpriteAnimation alienAnimations[3];

    for (size_t i = 0; i < 3; i++){
        alienAnimations[i].numFrames = 2;
        alienAnimations[i].loop = true;
        alienAnimations[i].frameTime = 10;
        alienAnimations[i].timeElapsed = 0;
        alienAnimations[i].animationFrames = new Sprite*[2];
        alienAnimations[i].animationFrames[0] = &enemySprites[2*i];
        alienAnimations[i].animationFrames[1] = &enemySprites[2*i + 1];
    }

    Sprite playerSprite;
    playerSprite.w = 11;
    playerSprite.h = 7;
    playerSprite.data = new uint8_t[playerSprite.w*playerSprite.h]{
        0,0,0,0,0,1,0,0,0,0,0, // .....@.....
        0,0,0,0,1,1,1,0,0,0,0, // ....@@@....
        0,0,0,0,1,1,1,0,0,0,0, // ....@@@....
        0,1,1,1,1,1,1,1,1,1,0, // .@@@@@@@@@.
        1,1,1,1,1,1,1,1,1,1,1, // @@@@@@@@@@@
        1,1,1,1,1,1,1,1,1,1,1, // @@@@@@@@@@@
        1,1,1,1,1,1,1,1,1,1,1, // @@@@@@@@@@@
    };

    Sprite bulletSprite;
    bulletSprite.w = 1;
    bulletSprite.h = 3;
    bulletSprite.data = new uint8_t[bulletSprite.w*bulletSprite.h]{
        1,  // @
        1,  // @
        1   // @
    };

    Sprite enemyBulletSprite;
    enemyBulletSprite.w = 1;
    enemyBulletSprite.h = 3;
    enemyBulletSprite.data = new uint8_t[enemyBulletSprite.w*enemyBulletSprite.h]{
        1,  // @
        1,  // @
        1   // @
    };

    Sprite emptySprite;
    emptySprite.w = 8;
    emptySprite.h = 8;
    emptySprite.data = new uint8_t[emptySprite.w*emptySprite.h]{
        0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0
    };

    Sprite textSpritesheet;
    textSpritesheet.w = 5;
    textSpritesheet.h = 7;
    textSpritesheet.data = new uint8_t[65 * 35] //65 characters each 7*5 in size
    {
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,0,0,0,0,0,1,0,0,
        0,1,0,1,0,0,1,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,1,0,1,0,0,1,0,1,0,1,1,1,1,1,0,1,0,1,0,1,1,1,1,1,0,1,0,1,0,0,1,0,1,0,
        0,0,1,0,0,0,1,1,1,0,1,0,1,0,0,0,1,1,1,0,0,0,1,0,1,0,1,1,1,0,0,0,1,0,0,
        1,1,0,1,0,1,1,0,1,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,1,0,1,1,0,1,0,1,1,
        0,1,1,0,0,1,0,0,1,0,1,0,0,1,0,0,1,1,0,0,1,0,0,1,0,1,0,0,0,1,0,1,1,1,1,
        0,0,0,1,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,0,1,0,0,0,0,0,1,
        1,0,0,0,0,0,1,0,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,0,
        0,0,1,0,0,1,0,1,0,1,0,1,1,1,0,0,0,1,0,0,0,1,1,1,0,1,0,1,0,1,0,0,1,0,0,
        0,0,0,0,0,0,0,1,0,0,0,0,1,0,0,1,1,1,1,1,0,0,1,0,0,0,0,1,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,
        0,0,0,1,0,0,0,0,1,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,1,0,0,0,0,1,0,0,0,

        0,1,1,1,0,1,0,0,0,1,1,0,0,1,1,1,0,1,0,1,1,1,0,0,1,1,0,0,0,1,0,1,1,1,0,
        0,0,1,0,0,0,1,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,1,1,1,0,
        0,1,1,1,0,1,0,0,0,1,0,0,0,0,1,0,0,1,1,0,0,1,0,0,0,1,0,0,0,0,1,1,1,1,1,
        1,1,1,1,1,0,0,0,0,1,0,0,0,1,0,0,0,1,1,0,0,0,0,0,1,1,0,0,0,1,0,1,1,1,0,
        0,0,0,1,0,0,0,1,1,0,0,1,0,1,0,1,0,0,1,0,1,1,1,1,1,0,0,0,1,0,0,0,0,1,0,
        1,1,1,1,1,1,0,0,0,0,1,1,1,1,0,0,0,0,0,1,0,0,0,0,1,1,0,0,0,1,0,1,1,1,0,
        0,1,1,1,0,1,0,0,0,1,1,0,0,0,0,1,1,1,1,0,1,0,0,0,1,1,0,0,0,1,0,1,1,1,0,
        1,1,1,1,1,0,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,
        0,1,1,1,0,1,0,0,0,1,1,0,0,0,1,0,1,1,1,0,1,0,0,0,1,1,0,0,0,1,0,1,1,1,0,
        0,1,1,1,0,1,0,0,0,1,1,0,0,0,1,0,1,1,1,1,0,0,0,0,1,1,0,0,0,1,0,1,1,1,0,

        0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,0,0,
        0,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,0,0,1,0,0,0,0,0,1,0,0,0,0,0,1,
        0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,0,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,
        1,0,0,0,0,0,1,0,0,0,0,0,1,0,0,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,0,
        0,1,1,1,0,1,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,0,1,0,0,0,0,0,0,0,0,0,1,0,0,
        0,1,1,1,0,1,0,0,0,1,1,0,1,0,1,1,1,0,1,1,1,0,1,0,0,1,0,0,0,1,0,1,1,1,0,

        0,0,1,0,0,0,1,0,1,0,1,0,0,0,1,1,0,0,0,1,1,1,1,1,1,1,0,0,0,1,1,0,0,0,1,
        1,1,1,1,0,1,0,0,0,1,1,0,0,0,1,1,1,1,1,0,1,0,0,0,1,1,0,0,0,1,1,1,1,1,0,
        0,1,1,1,0,1,0,0,0,1,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,1,0,1,1,1,0,
        1,1,1,1,0,1,0,0,0,1,1,0,0,0,1,1,0,0,0,1,1,0,0,0,1,1,0,0,0,1,1,1,1,1,0,
        1,1,1,1,1,1,0,0,0,0,1,0,0,0,0,1,1,1,1,0,1,0,0,0,0,1,0,0,0,0,1,1,1,1,1,
        1,1,1,1,1,1,0,0,0,0,1,0,0,0,0,1,1,1,1,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,
        0,1,1,1,0,1,0,0,0,1,1,0,0,0,0,1,0,1,1,1,1,0,0,0,1,1,0,0,0,1,0,1,1,1,0,
        1,0,0,0,1,1,0,0,0,1,1,0,0,0,1,1,1,1,1,1,1,0,0,0,1,1,0,0,0,1,1,0,0,0,1,
        0,1,1,1,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,1,1,1,0,
        0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,1,0,0,0,1,0,1,1,1,0,
        1,0,0,0,1,1,0,0,1,0,1,0,1,0,0,1,1,0,0,0,1,0,1,0,0,1,0,0,1,0,1,0,0,0,1,
        1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,1,1,1,1,
        1,0,0,0,1,1,1,0,1,1,1,0,1,0,1,1,0,1,0,1,1,0,0,0,1,1,0,0,0,1,1,0,0,0,1,
        1,0,0,0,1,1,0,0,0,1,1,1,0,0,1,1,0,1,0,1,1,0,0,1,1,1,0,0,0,1,1,0,0,0,1,
        0,1,1,1,0,1,0,0,0,1,1,0,0,0,1,1,0,0,0,1,1,0,0,0,1,1,0,0,0,1,0,1,1,1,0,
        1,1,1,1,0,1,0,0,0,1,1,0,0,0,1,1,1,1,1,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,
        0,1,1,1,0,1,0,0,0,1,1,0,0,0,1,1,0,0,0,1,1,0,1,0,1,1,0,0,1,1,0,1,1,1,1,
        1,1,1,1,0,1,0,0,0,1,1,0,0,0,1,1,1,1,1,0,1,0,1,0,0,1,0,0,1,0,1,0,0,0,1,
        0,1,1,1,0,1,0,0,0,1,1,0,0,0,0,0,1,1,1,0,0,0,0,0,1,1,0,0,0,1,0,1,1,1,0,
        1,1,1,1,1,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,
        1,0,0,0,1,1,0,0,0,1,1,0,0,0,1,1,0,0,0,1,1,0,0,0,1,1,0,0,0,1,0,1,1,1,0,
        1,0,0,0,1,1,0,0,0,1,1,0,0,0,1,1,0,0,0,1,1,0,0,0,1,0,1,0,1,0,0,0,1,0,0,
        1,0,0,0,1,1,0,0,0,1,1,0,0,0,1,1,0,1,0,1,1,0,1,0,1,1,1,0,1,1,1,0,0,0,1,
        1,0,0,0,1,1,0,0,0,1,0,1,0,1,0,0,0,1,0,0,0,1,0,1,0,1,0,0,0,1,1,0,0,0,1,
        1,0,0,0,1,1,0,0,0,1,0,1,0,1,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,
        1,1,1,1,1,0,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,0,1,1,1,1,1,

        0,0,0,1,1,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,0,1,1,
        0,1,0,0,0,0,1,0,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,0,1,0,0,0,0,1,0,
        1,1,0,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,1,1,0,0,0,
        0,0,1,0,0,0,1,0,1,0,1,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,
        0,0,1,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
    };

    //-----GAME LOGIC--------------------------------

    GameEngine game;

    game.gameWidth = GAME_WIDTH;
    game.gameHeight = GAME_HEIGHT;
    game.level = 0;

    game.player.x = 400-16;
    game.player.y = 600;
    game.player.lives = PLAYER_MAX_HEALTH;

    //Set up an array for the enemy array widths and heights
    size_t enemyArrWidths[] = {3, 5, 9, 10, 10};
    size_t enemyArrHeights[] = {3, 5, 5, 6, 7};

    size_t playerSpeed = 5;

    game.numEnemies = enemyArrWidths[game.level] * enemyArrHeights[game.level];
    game.enemies = new Enemy[game.numEnemies];
    game.numBullets = 0;
    game.numEnemyBullets = 0;

    uint32_t playerColour = rgbTo32(0, 191, 255);
    uint32_t explosionColour = rgbTo32(255, 100, 0);
    uint32_t bulletColour = playerColour;
    uint32_t textColour = rgbTo32(255, 255, 255);

    uint32_t enemyColours[3] = {
        rgbTo32(255, 0, 85),
        rgbTo32(0, 255, 0),
        rgbTo32(255, 0, 255)
    };

    size_t enemyHealths[] = {1, 1, 2, 3};
    size_t enemyBulletDamages[] = {1, 1, 2, 3};
    size_t enemyPoints[] = {0, 10, 20, 30};

    uint8_t* deathFrameCounter = new uint8_t[game.numEnemies];
    for (size_t i = 0; i < game.numEnemies; i++){
        deathFrameCounter[i] = 10;
    }

    uint8_t * enemyBulletCooldowns = new uint8_t[game.numEnemies];
    for (size_t i = 0; i < game.numEnemies; i++){
        enemyBulletCooldowns[i] = 0;
    }

    //Initialise enemy positions
    for (size_t i = 0; i < enemyArrHeights[game.level]; ++i){
        for (size_t j = 0; j < enemyArrWidths[game.level]; ++j){
            game.enemies[i*enemyArrWidths[game.level] + j].x = 64*j + (size_t) (game.gameWidth - enemyArrWidths[game.level]*64)/2 + 8;
            game.enemies[i*enemyArrWidths[game.level] + j].y = 64*i + 100;
            game.enemies[i*enemyArrWidths[game.level] + j].enemyType = static_cast<EnemyType>(4 - ((i/2)%3 + 1));
            game.enemies[i*enemyArrWidths[game.level] + j].enemyHealth = enemyHealths[(4 - ((i/2)%3 + 1))];
            game.enemies[i*enemyArrWidths[game.level] + j].enemyBulletDamage = enemyBulletDamages[(4 - ((i/2)%3 + 1))];

        }
    }

    //----OPENGL SHADERS-----------------------------

    //Vertex shader
    //Creates a triangle that covers the whole screen
    static const char* vertexShader =
    "#version 330 core\n"
    "noperspective out vec2 texture_coord;\n"
    "void main(){\n"
    "   texture_coord = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);\n"   //generates the following coordinates (0,0) (0,2) (2,0) (2,2)
    "   gl_Position = vec4(texture_coord * vec2(2.0, -2.0) + vec2(-1.0, 1.0), 0.0, 1.0);\n" //transform to vertex positions in NDC (normalised device coords) from -1 to 1
    "}\n";

    static const char* fragmentShader =
    "#version 330 core\n"
    "uniform sampler2D buffer;"
    "noperspective in vec2 texture_coord;\n"
    "out vec3 quadColour;\n"
    "void main(){\n"
    "   quadColour = texture(buffer, texture_coord).rgb;\n"
    "}\n";

    //Create a vertex array object
    //Contains the vertex data and the shader program

    GLuint quadVao;    //object name for VAO
    glGenVertexArrays(1, &quadVao);    //generate 1 VAO at specified address
    glBindVertexArray(quadVao);    //Set current VAO to quadVao

    //Compile shaders and link into shader program
    GLuint shaderProg = glCreateProgram();

    {
        //Vertex shader
        GLuint vertexShaderProg = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertexShaderProg, 1, &vertexShader, 0);
        glCompileShader(vertexShaderProg);
        glAttachShader(shaderProg, vertexShaderProg);
        glDeleteShader(vertexShaderProg);   //Can be discarded after attaching
    }

    {
        //Fragment shader
        GLuint fragmentShaderProg = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragmentShaderProg, 1, &fragmentShader, 0);
        glCompileShader(fragmentShaderProg);
        glAttachShader(shaderProg, fragmentShaderProg);
        glDeleteShader(fragmentShaderProg);
    }

    glLinkProgram(shaderProg);
    glUseProgram(shaderProg);

    //Creature a texture with some standard parameters
    GLuint bufferTexture;
    glGenTextures(1, &bufferTexture);
    glBindTexture(GL_TEXTURE_2D, bufferTexture);

    glTexImage2D(
        GL_TEXTURE_2D, 0, GL_RGB8,
        buffer.w, buffer.h, 0,
        GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, buffer.data
    );

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    //We need to attach the texture to our sampler in the fragment shader
    GLuint location = glGetUniformLocation(shaderProg, "buffer");
    glUniform1i(location, 0); //Set buffer sampler2D to 0

    glfwSwapInterval(1);    //Enable V-sync to fix FPS to multiples of 60
    glDisable(GL_DEPTH_TEST);
    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(quadVao);

    //-----GAME LOOP------------------
    gameRunning = true;

    while(!glfwWindowShouldClose(window) && gameRunning){
        clearBuffer(&buffer, bgColour);

        switch (gameState){

        //#############GAME TITLE SCREEN############
        case GameState::titleScreen:
            //title screen code

            if (!soundEngine->isCurrentlyPlaying(titleMusic)){
                soundEngine->stopAllSounds();
                soundEngine->play2D(titleMusic, true);
            }

            //display enemy mascot
            drawSpriteToBuffer(&buffer, enemySprites[2], enemyColours[1], 310, 200-50, 15);

            //set the title scale to be higher than normal text, say 8
            drawText("SPACE", &buffer, textSpritesheet, 0, 370-50, textColour, 9, true);
            drawText("INVADERS", &buffer, textSpritesheet, 0, 450-50, textColour, 9, true);

            drawText("PRESS ENTER TO START", &buffer, textSpritesheet, 0, 580-60, textColour, 3, true);

            drawText("MOOSA SAGHIR CS50 2023", &buffer, textSpritesheet, 30, 750, textColour, 2);

            if (titleDelayTimer.hasElapsed(2)){
                gameState = GameState::inGame;
            }
            break;

        //############MAIN GAME LOOP#############
        case GameState::inGame:

            if (playerDeathTimer.hasElapsed(2)){
                gameState = GameState::gameOver;
                break;
            }

            if (!soundEngine->isCurrentlyPlaying(gameMusic)){
                soundEngine->stopAllSounds();
                soundEngine->play2D(gameMusic, true);
            }

            char scoreText[64];
            sprintf(scoreText, "SCORE %zu", playerScore);

            char livesText[64];
            sprintf(livesText, "HP %i", game.player.lives);

            drawText(scoreText, &buffer, textSpritesheet, 20, 20, textColour, 4);
            drawText(livesText, &buffer, textSpritesheet, 20, 60, textColour, 4);

            //check if enemies are all dead and respawn if so
            if (!enemyRespawnTimer.isRunning){
                size_t enemyDeadCount = 0;
                for (size_t k = 0; k < game.numEnemies; k++){
                    if (game.enemies[k].enemyType == enemyDead){
                        enemyDeadCount++;
                    }
                }
                if (enemyDeadCount == enemyArrHeights[game.level]*enemyArrWidths[game.level]){
                    enemyRespawnTimer.start();
                    if (game.level < (sizeof(enemyArrHeights)/sizeof(size_t)) - 1){
                        game.level++;
                    }
                }
            }
            else if (enemyRespawnTimer.hasElapsed(6)){
                delete[] game.enemies;
                delete[] deathFrameCounter;
                delete[] enemyBulletCooldowns;
                game.numEnemies = enemyArrWidths[game.level] * enemyArrHeights[game.level];
                game.enemies = new Enemy[game.numEnemies];
                deathFrameCounter = new uint8_t[game.numEnemies];
                enemyBulletCooldowns = new uint8_t[game.numEnemies];

                for (size_t i = 0; i < game.numEnemies; i++){
                    deathFrameCounter[i] = 10;
                    enemyBulletCooldowns[i] = 0;
                }

                //increase player health by 2
                game.player.lives = std::min(PLAYER_MAX_HEALTH, game.player.lives + 2);

                for (size_t i = 0; i < enemyArrHeights[game.level]; ++i){
                    for (size_t j = 0; j < enemyArrWidths[game.level]; ++j){
                        game.enemies[i*enemyArrWidths[game.level] + j].x = 64*j + (size_t) (game.gameWidth - enemyArrWidths[game.level]*64)/2 + 8;
                        game.enemies[i*enemyArrWidths[game.level] + j].y = 64*i + 100;
                        game.enemies[i*enemyArrWidths[game.level] + j].enemyType = static_cast<EnemyType>(4 - ((i/2)%3 + 1));
                        game.enemies[i*enemyArrWidths[game.level] + j].enemyHealth = enemyHealths[(4 - ((i/2)%3 + 1))];
                        game.enemies[i*enemyArrWidths[game.level] + j].enemyBulletDamage = enemyBulletDamages[(4 - ((i/2)%3 + 1))];
                        deathFrameCounter[i*enemyArrWidths[game.level] + j] = 10;
                    }
                }
                enemyRespawnTimer.reset();
            }
            else{
                //display level complete text
                drawText("STAGE COMPLETE", &buffer, textSpritesheet, 232, 300, textColour, 4, true);
            }

            //Update bullet cooldown
            if (bulletCooldown > 0){
                bulletCooldown--;
            }

            //Update enemy bullet cooldowns
            for (size_t i = 0; i < game.numEnemies; i++){
                if (enemyBulletCooldowns[i] > 0){
                    enemyBulletCooldowns[i]--;
                }
            }

            for (size_t i = 0; i < game.numEnemies; i++){
                if (deathFrameCounter[i] == 0){
                    continue;   //Don't do anything if the alien is dead
                }
                Enemy& enemy = game.enemies[i];
                size_t frameNumber = alienAnimations[enemy.enemyType - 1].timeElapsed / alienAnimations[enemy.enemyType - 1].frameTime;
                if (enemy.enemyType == enemyDead){
                    drawSpriteToBuffer(&buffer, enemyDeathSprite, explosionColour, enemy.x, enemy.y, 4);
                }
                else{
                    if (enemy.isBlinking){
                        enemy.blinkTimer += 1;
                        if (enemy.blinkTimer >= blinkDuration){
                            enemy.isBlinking = false;
                            enemy.blinkTimer = 0;
                        }
                        else if (enemy.blinkTimer % 4 == 0){
                            drawSpriteToBuffer(&buffer, *alienAnimations[enemy.enemyType - 1].animationFrames[frameNumber], enemyColours[enemy.enemyType-1], enemy.x, enemy.y, 4);
                        }
                    }
                    else{
                        drawSpriteToBuffer(&buffer, *alienAnimations[enemy.enemyType - 1].animationFrames[frameNumber], enemyColours[enemy.enemyType-1], enemy.x, enemy.y, 4);
                    }
                }
            }


            //Bullet rendering
            for (size_t i = 0; i < game.numBullets; i++){
                const Bullet& currBullet = game.bullets[i];
                const Sprite& sprite = bulletSprite;
                drawSpriteToBuffer(&buffer, sprite, bulletColour, currBullet.x, currBullet.y, 4);
            }


            //Enemy bullet rendering
            if (!playerDeathTimer.isRunning){
                for (size_t i = 0; i < game.numEnemyBullets; i++){
                    const Bullet& currEnemyBullet = game.enemyBullets[i];
                    const Sprite& sprite = enemyBulletSprite;
                    drawSpriteToBuffer(&buffer, sprite, currEnemyBullet.bulletColour, currEnemyBullet.x, currEnemyBullet.y, 4);
                }
            }

            if (game.player.isBlinking){
                game.player.blinkTimer += 1;
                if (game.player.blinkTimer >= blinkDuration){
                    game.player.isBlinking = false;
                }
                else if (game.player.blinkTimer % 4 == 0){
                    drawSpriteToBuffer(&buffer, playerSprite, playerColour, game.player.x, game.player.y, 4);
                }
            }
            else{
                drawSpriteToBuffer(&buffer, playerSprite, playerColour, game.player.x, game.player.y, 4);
            }

            for (size_t i = 0; i < 3; i++){
                //Enemy animation
                alienAnimations[i].timeElapsed++;
                if (alienAnimations[i].timeElapsed == alienAnimations[i].frameTime*alienAnimations[i].numFrames){
                    if (alienAnimations[i].loop){
                        alienAnimations[i].timeElapsed = 0;
                    }
                }
            }

            for (size_t i = 0; i < game.numEnemies; i++){
                //Use a reference to the enemy rather than enemy itself?
                Enemy& enemy = game.enemies[i];
                if (enemy.enemyType == enemyDead && deathFrameCounter[i] != 0){
                    deathFrameCounter[i]--;
                }
            }

            //Player bullet logic
            for (size_t i = 0; i < game.numBullets;){
                game.bullets[i].y += game.bullets[i].bulletSpeed;
                if (game.bullets[i].y >= game.gameHeight || game.bullets[i].y < bulletSprite.h){

                    //Overwrite the bullet to be deleted with the element before last in the bullets array
                    game.bullets[i] = game.bullets[game.numBullets-1];
                    game.numBullets--;
                    continue;
                }

                for (size_t j = 0; j < game.numEnemies; j++){
                    Enemy& enemy = game.enemies[j];
                    if (enemy.enemyType == enemyDead){
                        continue;
                    }

                    SpriteAnimation& anim = alienAnimations[enemy.enemyType - 1];
                    size_t frameNumber = anim.timeElapsed / anim.frameTime;
                    Sprite& sprite = *anim.animationFrames[frameNumber];
                    if (spritesIntersect(sprite, enemy.x, enemy.y, bulletSprite, game.bullets[i].x, game.bullets[i].y)){
                        if (game.enemies[j].enemyHealth > 0){
                            game.enemies[j].enemyHealth--;
                            if (game.enemies[j].enemyHealth == 0){
                                playerScore += enemyPoints[enemy.enemyType];
                                game.enemies[j].enemyType = enemyDead;
                                game.enemies[j].x -= (enemyDeathSprite.w - sprite.w)/2;
                                soundEngine->play2D(enemyDeathSound);
                            }
                            else{
                                enemy.onHit();
                                soundEngine->play2D(enemyDamageSound);
                            }
                        }

                        game.bullets[i] = game.bullets[game.numBullets-1];
                        game.numBullets--;
                        continue;
                    }
                }
                i++;
            }

            //Enemy bullet logic
            for (size_t i = 0; i < game.numEnemyBullets; i++){

                //update bullet position
                Bullet& currBullet = game.enemyBullets[i];
                currBullet.y += currBullet.bulletSpeed;

                if (currBullet.x < 0 || currBullet.x > game.gameWidth - 4*enemyBulletSprite.w || currBullet.y < enemyBulletSprite.h || currBullet.y >= game.gameHeight){
                    game.enemyBullets[i] = game.enemyBullets[game.numEnemyBullets-1];
                    game.numEnemyBullets--;
                    continue;
                }

                if (spritesIntersect(playerSprite, game.player.x, game.player.y, enemyBulletSprite, currBullet.x, currBullet.y)){
                    if (!playerDeathTimer.isRunning){
                        game.player.onHit();
                        game.player.lives = std::max(0, game.player.lives - static_cast<int>(enemyBulletDamages[currBullet.enemyType])) ;
                        soundEngine->play2D(playerDamageSound);
                        game.enemyBullets[i] = game.enemyBullets[game.numEnemyBullets - 1];
                        game.numEnemyBullets--;
                        if (game.player.lives <= 0){
                            soundEngine->play2D(playerDeathSound);

                            //start timer
                            playerDeathTimer.start();
                        }
                    }
                }
            }

            //Player movement controls
            if (playerMoveDirection != 0 && !playerDeathTimer.isRunning){
                if (game.player.x + 4*playerSprite.w >= game.gameWidth-60 && playerMoveDirection == 1){
                    game.player.x = game.gameWidth - 4*playerSprite.w - 60;
                }
                else if ((int)game.player.x <= 60 && playerMoveDirection == -1){
                    game.player.x = 60;
                }
                else{
                    game.player.x += playerSpeed*playerMoveDirection;
                }
            }

            //Bullet firing
            //note that game.numBullets started at 0 so it is already zero indexed
            if (bulletFired && game.numBullets < maxBullets){
                game.bullets[game.numBullets].x = game.player.x + 2*playerSprite.w;
                game.bullets[game.numBullets].y = game.player.y + 2*playerSprite.h;
                soundEngine->play2D(fireSound);

                //Set bullet speed
                game.bullets[game.numBullets].bulletSpeed = -2; //Negative because of top-down coordinate system
                game.numBullets++;
            }

            //Enemies will shoot bullets if the player is in front of them
            //AND if there is no enemy in front of them
            for(size_t i = 0; i < game.numEnemies; i++){
                Enemy& currEnemy = game.enemies[i];

                if (currEnemy.enemyType == enemyDead){
                    continue;
                }
                SpriteAnimation& anim = alienAnimations[currEnemy.enemyType - 1];
                size_t currFrame = anim.timeElapsed / anim.frameTime;
                Sprite& sprite = *anim.animationFrames[currFrame];

                if (enemyBulletCooldowns[i] == 0
                    && game.player.x <= currEnemy.x + 4*sprite.w
                    && game.player.x >= currEnemy.x
                    && ((i >= enemyArrWidths[game.level]*(enemyArrHeights[game.level]-1)
                    && i < enemyArrHeights[game.level]*enemyArrWidths[game.level]) || game.enemies[i + enemyArrWidths[game.level]].enemyType == enemyDead)){

                    //initialise the bullet position
                    game.enemyBullets[game.numEnemyBullets].x = game.enemies[i].x + 2*sprite.w;
                    game.enemyBullets[game.numEnemyBullets].y = game.enemies[i].y + 4*sprite.h;
                    game.enemyBullets[game.numEnemyBullets].bulletColour = enemyColours[currEnemy.enemyType-1];
                    game.enemyBullets[game.numEnemyBullets].enemyType = currEnemy.enemyType;
                    enemyBulletCooldowns[i] = 20;

                    //set bullet speed
                    game.enemyBullets[game.numEnemyBullets].bulletSpeed = 3;
                    game.numEnemyBullets++;
                }
            }

            bulletFired = false;
            break;

        //##############GAME OVER SEQUENCE##############
        case GameState::gameOver:
            //game over sequence

            /*if (currentMusic != gameOverMusic){
                if (currentMusic){
                    currentMusic->drop();
                }
                currentMusic = soundEngine->play2D(gameOverMusic, true);
            }*/

            drawText("GAME OVER", &buffer, textSpritesheet, 0, 300, textColour, 6, true);
            drawText("PRESS ESC TO QUIT", &buffer, textSpritesheet, 0, 400, textColour, 3, true);

            break;
        }

        glTexSubImage2D(GL_TEXTURE_2D, 0,0,0, buffer.w, buffer.h, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, buffer.data);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);   //Set glClear colour to buffered colour
        glfwSwapBuffers(window);    //Swap front (visible) and back (hidden) buffers
        glfwPollEvents();   //Process any pending events
    }

    //Once the game loop is finished, end the program
    glfwDestroyWindow(window);
    glfwTerminate();
    glDeleteVertexArrays(1, &quadVao);
    soundEngine->drop();
    return 0;
}

//Set up GLFW error callback function
void errorCallback(int error, const char* error_disc){
    fprintf(stderr, "Error %s\n", error_disc);
}

void inputCallback(GLFWwindow* window, int key, int _, int action, int mods){
    switch (gameState){
    case GameState::titleScreen:
        //title screen controls
        switch(key){
        case GLFW_KEY_ESCAPE:
            if (action == GLFW_PRESS){
                gameRunning = false;
            }
            break;
        case GLFW_KEY_ENTER:
            if (action == GLFW_PRESS){
                soundEngine->play2D("sounds/Arcade Echo FX 001.wav");
                if (!titleDelayTimer.isRunning){
                    titleDelayTimer.start();
                }
            }
        }
        break;
    case GameState::inGame:
        //main game controls
        switch(key){
        case GLFW_KEY_ESCAPE:
            if (action == GLFW_PRESS){
                gameRunning = false;
            }
            break;
        case GLFW_KEY_RIGHT:
            if (action == GLFW_PRESS){
                playerMoveDirection++;
            }
            else if (action == GLFW_RELEASE){
                playerMoveDirection--;
            }
            break;
        case GLFW_KEY_LEFT:
            if (action == GLFW_PRESS){
                playerMoveDirection--;
            }
            else if (action == GLFW_RELEASE){
                playerMoveDirection++;
            }
            break;
        case GLFW_KEY_Z:
            if (action == GLFW_RELEASE && bulletCooldown == 0 && !enemyRespawnTimer.isRunning){
                bulletFired = true;
                bulletCooldown = maxBulletCooldown;
            }
            break;
        }
        break;
    case GameState::gameOver:
        //game over controls
        switch(key){
        case GLFW_KEY_ESCAPE:
            if (action == GLFW_PRESS){
                gameRunning = false;
            }
            break;
        }
        break;
    }
}

//Takes three unsigned ints for RGB and converts to unsigned 32 bit
uint32_t rgbTo32(uint8_t R, uint8_t G, uint8_t B){
    return (R << 24) | (G << 16) | (B << 8) | 255;
}

//Clear buffer to some colour
void clearBuffer(Buffer* buffer, uint32_t colour){
    for (size_t i = 0; i < buffer->w * buffer->h; i++){
        buffer->data[i] = colour;
    }
}

//Draw sprite data to buffer with a scaling factor added
//(x,y) is the top left corner of the sprite image

void drawSpriteToBuffer(Buffer* buffer, const Sprite& sprite, uint32_t colour, size_t x, size_t y, size_t scale){
    for (size_t i = 0; i < sprite.h; i++){
        for (size_t j = 0; j < sprite.w; j++){
            if (sprite.data[i * sprite.w + j]){
                // Draw a block of pixels for each sprite pixel
                for (size_t dy = 0; dy < scale; dy++){
                    for (size_t dx = 0; dx < scale; dx++){
                        size_t X = x + j * scale + dx;
                        size_t Y = y + i * scale + dy;
                        if (X < buffer->w && Y < buffer->h){
                            buffer->data[Y * buffer->w + X] = colour;
                        }
                    }
                }
            }
        }
    }
}

bool spritesIntersect(Sprite& sprite1, size_t x1, size_t y1, Sprite& sprite2, size_t x2, size_t y2){
    bool intersectX = x1 >= x2 - 4*sprite1.w && x1 <= x2 + 4*sprite2.w;
    bool intersectY = y1 >= y2 - 4*sprite1.h && y1 <= y2 + 4*sprite2.h;
    return intersectX && intersectY;
}

void drawText(const char* text, Buffer* buffer, Sprite& spriteSheet, size_t x, size_t y, uint32_t textColour, size_t scale, bool centred){
    size_t stride = spriteSheet.w * spriteSheet.h;    //the text is 1D to the computer so we need to go forward this number of bits to the next char
    Sprite sprite = spriteSheet;
    size_t xp;
    if (centred){
        xp = (GAME_WIDTH - scale*(spriteSheet.w+1)*strlen(text)) / 2;
    }
    else{
        xp = x;
    }

    for(int i = 0; text[i] != '\0'; i++){
        //Do alphabetical rendering
        char letter = text[i] - 32;
        if (letter < 0 || letter >= 65){
            continue;
        }
        sprite.data = spriteSheet.data + letter*stride;
        drawSpriteToBuffer(buffer, sprite, textColour, xp, y, scale);
        xp += scale*sprite.w + scale;
    }
}


