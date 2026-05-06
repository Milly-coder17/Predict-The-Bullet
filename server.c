/*
 * Filename:
 *      server.c
 *
 * Description:
 *      This program is used in tandem with client.c demonstrating the use of sockets to create a TCP server.
 *
 * Compile Instructions:
 *      `gcc -o server server.c'
 *
 * Author:
 *      Raphael Garay
 *      rgaray@gbox.adnu.edu.ph
 *      Ateneo de Naga University
 *
 * Notes:
 *      This is a handout for CSMC312: Operating Systems
 *      First Sem S/Y 2024-2025
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdbool.h>
#include <fcntl.h>
#include <errno.h>
#include "raylib.h"

typedef enum {
    STATE_INPUT,
    STATE_WAIT_SEND,
    STATE_WAIT_OPPONENT,
    STATE_ANIMATE,
    STATE_RESOLVE,
    STATE_CANCEL_ANIM,
    STATE_CANCEL_INPUT,
    STATE_CANCEL_WAIT
} GameState;

#define SHOOT_FRAMES 9
#define LOADANDHIT_FRAMES 14
#define LOAD_FRAMES 15
#define BLOCK_FRAMES 10
#define DEFLECT_FRAMES 13
#define DEFLECTHIT_FRAMES 16
#define SHOOTANDSHOT_FRAMES 20
#define BLOCKANDSHOT_FRAMES 16
#define DEFLECTANDSHOT_FRAMES 18
#define DOUBLESHOT_FRAMES 12
#define INTRO_FRAMES 48
#define CANCEL_FRAMES 19
#define HIT_FRAMES 14


typedef struct{
    int health;
    int energy;
    int bullet;
    bool canBlock;
    bool canDeflect;
} Player;

void die_with_error(char *error_msg){
    printf("%s", error_msg);
    exit(-1);
}

int recv_int(int sock, int *value){
    size_t total = 0;
    int bytes;

    while(total < sizeof(int)){
        bytes = recv(sock, ((char*)value) + total, sizeof(int) - total, 0);
        if(bytes < 0){
            if(errno == EAGAIN || errno == EWOULDBLOCK) continue;
            return -1;
        }
        if(bytes == 0) return -1;
        total += bytes;
    }
    return 0;
}

int send_int(int sock, int value){
    size_t total = 0;
    int bytes;

    while(total < sizeof(int)){
        bytes = send(sock, ((char*)&value) + total, sizeof(int) - total, 0);
        if(bytes < 0){
            if(errno == EAGAIN || errno == EWOULDBLOCK) continue;
            return -1;
        }
        if(bytes == 0) return -1;
        total += bytes;
    }
    return 0;
}

int main(int argc, char *argv[]){

    int server_sock, client_sock, port_no;
    socklen_t client_size;
    struct sockaddr_in server_addr, client_addr;

    if(argc < 2){
        printf("Usage: %s port_no", argv[0]);
        exit(1);
    }

    printf("Server starting ...\n");

    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if(server_sock < 0)
        die_with_error("Error: socket() Failed.");

    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    bzero((char*)&server_addr, sizeof(server_addr));

    port_no = atoi(argv[1]);

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port_no);

    if(bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0)
        die_with_error("Error: bind() Failed.");

    listen(server_sock, 5);

    printf("Server listening to port %d ...\n", port_no);

    client_size = sizeof(client_addr);
    client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &client_size);

    if(client_sock < 0) {
        die_with_error("Error: accept() Failed.");
    }

    printf("Client successfully connected ...\n\n");

    int flags = fcntl(client_sock, F_GETFL, 0);
    fcntl(client_sock, F_SETFL, flags | O_NONBLOCK);

    Player player1;
    Player player2;
    player1.health = 5;
    player1.energy = 2;
    player1.bullet = 1;
    player1.canBlock = true;
    player1.canDeflect = true;
    player2.health = 5;

    InitWindow(1280, 720, "Predict The Bullet - Server");
    SetTargetFPS(60);
    InitAudioDevice();
    Music music = LoadMusicStream("assets/music.wav");
    SetMusicVolume(music, 0.6f);
    PlayMusicStream(music);

    int selectedIndex = 1;
    int player1move = 0;
    int player2move = -1;
    GameState gameState = STATE_INPUT;
    const char *inputMessage = "";
    Color inputMessageColor = WHITE;

    int animFrame = 0;
    float animTimer = 0.0f;
    int holdTimer = 0;
    int cancelMode = 0;
    int cancelAnimFrame = 0;
    float cancelAnimTimer = 0.0f;
    const float FRAME_DURATION = 0.08f;
    Texture2D background = LoadTexture("assets/background.png");
    Texture2D player1Tex = LoadTexture("assets/player1.png");
    Texture2D player2Tex = LoadTexture("assets/player2.png");
    Texture2D heartTex   = LoadTexture("assets/heart.png");
    Texture2D energyTex  = LoadTexture("assets/energy.png");
    Texture2D bulletTex  = LoadTexture("assets/bullet.png");
    Texture2D youWinTex  = LoadTexture("assets/youwin.png");
    Texture2D youLostTex = LoadTexture("assets/youlost.png");
    SetTextureFilter(heartTex,  TEXTURE_FILTER_POINT);
    SetTextureFilter(energyTex, TEXTURE_FILTER_POINT);
    SetTextureFilter(bulletTex, TEXTURE_FILTER_POINT);
    Sound shootSound = LoadSound("assets/shoot.ogg");
    Sound blockSound = LoadSound("assets/block.wav");
    Sound deflectSound = LoadSound("assets/deflect.wav");
    Sound loadSound = LoadSound("assets/load.wav");
    Sound doubleShotSound = LoadSound("assets/doubleshot.wav");
    Sound cancelSound = LoadSound("assets/cancel.wav");
    Sound winnerSound = LoadSound("assets/winnersound.wav");
    Sound loserSound = LoadSound("assets/losersound.wav");
    SetSoundVolume(blockSound, 2.0f);
    SetSoundVolume(deflectSound, 2.0f);

    Texture2D player1ShootFrames[SHOOT_FRAMES];
    for (int i = 0; i < SHOOT_FRAMES; i++) {
        player1ShootFrames[i] = LoadTexture(TextFormat("assets/player1Shoot/shootframe%d.png", i + 1));
    }

    Texture2D player1LoadFrames[LOAD_FRAMES];
    for (int i = 0; i < LOAD_FRAMES; i++) {
        player1LoadFrames[i] = LoadTexture(TextFormat("assets/player1Load/loadframe%d.png", i + 1));
    }

    Texture2D player1BlockFrames[BLOCK_FRAMES];
    for (int i = 0; i < BLOCK_FRAMES; i++) {
        player1BlockFrames[i] = LoadTexture(TextFormat("assets/player1Block/blockframe%d.png", i + 1));
    }

    Texture2D player1DeflectFrames[DEFLECT_FRAMES];
    for (int i = 0; i < DEFLECT_FRAMES; i++) {
        player1DeflectFrames[i] = LoadTexture(TextFormat("assets/player1Deflect/deflectframe%d.png", i + 1));
    }

    Texture2D player1LoadAndHitFrames[LOADANDHIT_FRAMES];
    for (int i = 0; i < LOADANDHIT_FRAMES; i++) {
        player1LoadAndHitFrames[i] = LoadTexture(TextFormat("assets/player1LoadAndHit/loadandhitframe%d.png", i + 1));
    }

    Texture2D player1DeflectHitFrames[DEFLECTHIT_FRAMES];
    for (int i = 0; i < DEFLECTHIT_FRAMES; i++) {
        player1DeflectHitFrames[i] = LoadTexture(TextFormat("assets/player1DeflectHit/deflecthitframe%d.png", i + 1));
    }

    Texture2D player1ShootAndShotFrames[SHOOTANDSHOT_FRAMES];
    for (int i = 0; i < SHOOTANDSHOT_FRAMES; i++) {
        player1ShootAndShotFrames[i] = LoadTexture(TextFormat("assets/player1ShootAndShot/shootandshotframe%d.png", i + 1));
    }

    Texture2D player1BlockAndShotFrames[BLOCKANDSHOT_FRAMES];
    for (int i = 0; i < BLOCKANDSHOT_FRAMES; i++) {
        player1BlockAndShotFrames[i] = LoadTexture(TextFormat("assets/player1BlockAndShot/blockandshotframe%d.png", i + 1));
    }

    Texture2D player1DeflectAndShotFrames[DEFLECTANDSHOT_FRAMES];
    for (int i = 0; i < DEFLECTANDSHOT_FRAMES; i++) {
        player1DeflectAndShotFrames[i] = LoadTexture(TextFormat("assets/player1DeflectAndShot/deflectandshotframe%d.png", i + 1));
    }



    Texture2D player2ShootFrames[SHOOT_FRAMES];
    for (int i = 0; i < SHOOT_FRAMES; i++) {
        player2ShootFrames[i] = LoadTexture(TextFormat("assets/player2Shoot/shootframe%d.png", i + 1));
    }

    Texture2D player2LoadFrames[LOAD_FRAMES];
    for (int i = 0; i < LOAD_FRAMES; i++) {
        player2LoadFrames[i] = LoadTexture(TextFormat("assets/player2Load/loadframe%d.png", i + 1));
    }

    Texture2D player2BlockFrames[BLOCK_FRAMES];
    for (int i = 0; i < BLOCK_FRAMES; i++) {
        player2BlockFrames[i] = LoadTexture(TextFormat("assets/player2Block/blockframe%d.png", i + 1));
    }

    Texture2D player2DeflectFrames[DEFLECT_FRAMES];
    for (int i = 0; i < DEFLECT_FRAMES; i++) {
        player2DeflectFrames[i] = LoadTexture(TextFormat("assets/player2Deflect/deflectframe%d.png", i + 1));
    }

    Texture2D player2LoadAndHitFrames[LOADANDHIT_FRAMES];
    for (int i = 0; i < LOADANDHIT_FRAMES; i++) {
        player2LoadAndHitFrames[i] = LoadTexture(TextFormat("assets/player2LoadAndHit/loadandhitframe%d.png", i + 1));
    }

    Texture2D player2DeflectHitFrames[DEFLECTHIT_FRAMES];
    for (int i = 0; i < DEFLECTHIT_FRAMES; i++) {
        player2DeflectHitFrames[i] = LoadTexture(TextFormat("assets/player2DeflectHit/deflecthitframe%d.png", i + 1));
    }

    Texture2D player1DoubleShotFrames[DOUBLESHOT_FRAMES];
    for (int i = 0; i < DOUBLESHOT_FRAMES; i++) {
        player1DoubleShotFrames[i] = LoadTexture(TextFormat("assets/player1DoubleShot/doubleshotframe%d.png", i + 1));
    }

    Texture2D player2DoubleShotFrames[DOUBLESHOT_FRAMES];
    for (int i = 0; i < DOUBLESHOT_FRAMES; i++) {
        player2DoubleShotFrames[i] = LoadTexture(TextFormat("assets/player2DoubleShot/doubleshotframe%d.png", i + 1));
    }

    Texture2D player2ShootAndShotFrames[SHOOTANDSHOT_FRAMES];
    for (int i = 0; i < SHOOTANDSHOT_FRAMES; i++) {
        player2ShootAndShotFrames[i] = LoadTexture(TextFormat("assets/player2ShootAndShot/shootandshotframe%d.png", i + 1));
    }

    Texture2D player2BlockAndShotFrames[BLOCKANDSHOT_FRAMES];
    for (int i = 0; i < BLOCKANDSHOT_FRAMES; i++) {
        player2BlockAndShotFrames[i] = LoadTexture(TextFormat("assets/player2BlockAndShot/blockandshotframe%d.png", i + 1));
    }

    Texture2D player2DeflectAndShotFrames[DEFLECTANDSHOT_FRAMES];
    for (int i = 0; i < DEFLECTANDSHOT_FRAMES; i++) {
        player2DeflectAndShotFrames[i] = LoadTexture(TextFormat("assets/player2DeflectAndShot/deflectandshotframe%d.png", i + 1));
    }

    Texture2D player1HitFrames[HIT_FRAMES];
    for (int i = 0; i < HIT_FRAMES; i++) {
        player1HitFrames[i] = LoadTexture(TextFormat("assets/player1Hit/hitframe%d.png", i + 1));
    }

    Texture2D player2HitFrames[HIT_FRAMES];
    for (int i = 0; i < HIT_FRAMES; i++) {
        player2HitFrames[i] = LoadTexture(TextFormat("assets/player2Hit/hitframe%d.png", i + 1));
    }

    Texture2D cancelFrames[CANCEL_FRAMES];
    for (int i = 0; i < CANCEL_FRAMES; i++) {
        cancelFrames[i] = LoadTexture(TextFormat("assets/cancelAnimation/cancelanimationframe%d.png", i + 1));
    }

    Texture2D introFrames[INTRO_FRAMES];
    for (int i = 0; i < INTRO_FRAMES; i++) {
        introFrames[i] = LoadTexture(TextFormat("assets/battleIntro/battleintroframe%d.png", i + 1));
    }

    int introFrame = 0;
    float introTimer = 0.0f;
    const float INTRO_FRAME_DURATION = 0.05f;

    while (introFrame < INTRO_FRAMES) {
        UpdateMusicStream(music);
        BeginDrawing();
        ClearBackground(BLACK);
        DrawTextureEx(introFrames[introFrame], (Vector2){0, 0}, 0.0f, (float)GetScreenWidth() / 1280.0f, WHITE);
        introTimer += GetFrameTime();
        if (introTimer >= INTRO_FRAME_DURATION) {
            introTimer = 0.0f;
            introFrame++;
        }
        EndDrawing();
    }

    bool gameOver = true;
    while(gameOver){
        UpdateMusicStream(music);
        BeginDrawing();
        if (IsKeyPressed(KEY_F11)) {
            ToggleFullscreen();
        }

        float bgScale = (float)GetScreenWidth() / 1920.0f;
        DrawTextureEx(background, (Vector2){0, 0}, 0.0f, bgScale, WHITE);
        float p1X = 300.0f * bgScale;
        float p1Y = 450.0f * bgScale;
        float p2X = 1200.0f * bgScale;
        float p2Y = 450.0f * bgScale;

        if (gameState != STATE_ANIMATE) {
            DrawTextureEx(player1Tex, (Vector2){p1X, p1Y}, 2.0f, bgScale * 4.0f, WHITE);
            DrawTextureEx(player2Tex, (Vector2){p2X, p2Y}, 2.0f, bgScale * 4.0f, WHITE);
        }

            int uiX = (int)(40 * bgScale);
            int labelSize = (int)(18 * bgScale);
            int titleSize = (int)(32 * bgScale);
            int labelGap = (int)(8 * bgScale);
            int sectionGap = (int)(20 * bgScale);
            int iconSpacing = (int)(8 * bgScale);

            DrawText("Player 1 Status", uiX, (int)(10 * bgScale), titleSize, WHITE);

            float heartScale = 7.0f * bgScale;
            int heartH = (int)(heartTex.height * heartScale);
            int heartW = (int)(heartTex.width  * heartScale);
            int heartY = (int)(50 * bgScale) + titleSize;
            for (int i = 0; i < player1.health; i++) {
                int x = uiX + i * (heartW + iconSpacing);
                DrawTextureEx(heartTex, (Vector2){x, heartY},
                              0.0f, heartScale, WHITE);
            }

            int   iconTargetH  = heartH;
            float energyScale  = (float)iconTargetH / energyTex.height;
            int   energyW      = (int)(energyTex.width * energyScale);
            int   energyLabelY = heartY + heartH + sectionGap;
            int   energyY      = energyLabelY + labelSize + labelGap;
            int   maxEnergy    = 6;
            for (int i = 0; i < maxEnergy; i++) {
                int ex = uiX + i * (energyW + iconSpacing);
                Color tint = (i < player1.energy) ? WHITE : Fade(WHITE, 0.25f);
                DrawTextureEx(energyTex, (Vector2){ex, energyY},
                              0.0f, energyScale, tint);
            }

            float bulletScale  = (float)iconTargetH / bulletTex.height;
            int   bulletW      = (int)(bulletTex.width * bulletScale);
            int   bulletLabelY = energyY + iconTargetH + sectionGap;
            int   bulletY      = bulletLabelY + labelSize + labelGap;
            int   maxBullets   = 6;
            for (int i = 0; i < maxBullets; i++) {
                int bx = uiX + i * (bulletW + iconSpacing);
                Color tint = (i < player1.bullet) ? WHITE : Fade(WHITE, 0.25f);
                DrawTextureEx(bulletTex, (Vector2){bx, bulletY},
                              0.0f, bulletScale, tint);
            }

            // Player 2 health (right side)
            {
                int p2RightMargin = (int)(40 * bgScale);
                const char *p2Title = "Player 2 Health";
                int p2TitleW = MeasureText(p2Title, titleSize);
                int p2TitleX = GetScreenWidth() - p2RightMargin - p2TitleW;
                DrawText(p2Title, p2TitleX, (int)(10 * bgScale), titleSize, WHITE);

                int p2HeartY = (int)(50 * bgScale) + titleSize;
                int maxP2Health = 5;
                int totalHeartsW = maxP2Health * heartW + (maxP2Health - 1) * iconSpacing;
                int p2HeartsStartX = GetScreenWidth() - p2RightMargin - totalHeartsW;
                for (int i = 0; i < maxP2Health; i++) {
                    int hx = p2HeartsStartX + i * (heartW + iconSpacing);
                    Color tint = (i < player2.health) ? WHITE : Fade(WHITE, 0.25f);
                    DrawTextureEx(heartTex, (Vector2){hx, p2HeartY},
                                  0.0f, heartScale, tint);
                }
            }

            const char *moves[] = {
                "Shoot",
                "Load",
                "Block",
                "Deflect",
                "Cancel (Special)",
                "Double Shot (Special)"
            };

            int boxW = 760;
            int boxH = 140;
            int boxX = (GetScreenWidth() - boxW) / 2;
            int boxY = GetScreenHeight() - boxH - 30;

            Color boxFill    = (Color){ 30,  30,  50, 220};
            Color bevelLight = (Color){130, 130, 170, 255};
            Color bevelDark  = (Color){  5,   5,  15, 255};

            DrawRectangle(boxX, boxY, boxW, boxH, boxFill);
            DrawRectangle(boxX, boxY, boxW, 4, bevelLight);
            DrawRectangle(boxX, boxY, 4, boxH, bevelLight);
            DrawRectangle(boxX, boxY + boxH - 4, boxW, 4, bevelDark);
            DrawRectangle(boxX + boxW - 4, boxY, 4, boxH, bevelDark);

            int promptSize = 22;
            const char *prompt = (gameState == STATE_CANCEL_INPUT) ? "Choose follow-up move:" : "Choose your move:";
            DrawText(prompt,
                     boxX + (boxW - MeasureText(prompt, promptSize)) / 2,
                     boxY + 14,
                     promptSize, YELLOW);

            int gridTopY = boxY + 60;
            int gridH = boxH - 70;
            int cellW = boxW / 3;
            int cellH = gridH / 2;
            int moveFontSize = 22;

            for (int i = 0; i < 6; i++) {
                int r = i / 3;
                int c = i % 3;
                int cellCx = boxX + c * cellW + cellW / 2;
                int cellCy = gridTopY + r * cellH + cellH / 2;

                bool onCooldown = (i == 2 && !player1.canBlock) || (i == 3 && !player1.canDeflect);
                const char *label = (i == selectedIndex)
                    ? TextFormat("> %s <", moves[i])
                    : onCooldown ? TextFormat("%s (cooldown)", moves[i]) : moves[i];
                Color color = (i == selectedIndex) ? YELLOW : onCooldown ? GRAY : WHITE;

                int tw = MeasureText(label, moveFontSize);
                DrawText(label,
                         cellCx - tw / 2,
                         cellCy - moveFontSize / 2,
                         moveFontSize, color);
            }

            if (inputMessage[0] != '\0') {
                int msgW = MeasureText(inputMessage, 20);
                DrawText(inputMessage,
                         (GetScreenWidth() - msgW) / 2,
                         boxY - 30,
                         20, inputMessageColor);
            }
        if (gameState == STATE_INPUT)
        {
            int row = selectedIndex / 3;
            int col = selectedIndex % 3;

            if (IsKeyPressed(KEY_RIGHT)) col = (col + 1) % 3;
            if (IsKeyPressed(KEY_LEFT))  col = (col + 2) % 3;
            if (IsKeyPressed(KEY_DOWN))  row = (row + 1) % 2;
            if (IsKeyPressed(KEY_UP))    row = (row + 1) % 2;

            int newIndex = row * 3 + col;
            if (newIndex > 5) newIndex = 5;
            selectedIndex = newIndex;

            if (IsKeyPressed(KEY_ENTER))
            {
                int chosenMove = selectedIndex + 1;

                if (chosenMove == 1 && player1.bullet <= 0)
                {
                    inputMessage = "No bullets! Cannot shoot.";
                    inputMessageColor = RED;
                }
                else if (chosenMove == 3 && player1.energy < 1)
                {
                    inputMessage = "Not enough energy to block.";
                    inputMessageColor = RED;
                }
                else if (chosenMove == 3 && !player1.canBlock)
                {
                    inputMessage = "Block is on cooldown!";
                    inputMessageColor = RED;
                }
                else if (chosenMove == 4 && player1.energy < 3)
                {
                    inputMessage = "Not enough energy to deflect.";
                    inputMessageColor = RED;
                }
                else if (chosenMove == 4 && !player1.canDeflect)
                {
                    inputMessage = "Deflect is on cooldown!";
                    inputMessageColor = RED;
                }
                else if (chosenMove == 5 && player1.energy < 5)
                {
                    inputMessage = "Not enough energy for Special.";
                    inputMessageColor = RED;
                }
                else if (chosenMove == 6 && (player1.energy < 3 || player1.bullet < 2))
                {
                    inputMessage = "Need 3 energy & 2 bullets for Double Shot.";
                    inputMessageColor = RED;
                }
                else
                {
                    player1move = chosenMove;
                    inputMessage = "";
                    gameState = STATE_WAIT_SEND;
                }
            }
        }

        if (gameState == STATE_WAIT_SEND)
        {
            if(send_int(client_sock, player1move) < 0){
                die_with_error("Error: send() Failed.");
            }
            gameState = STATE_WAIT_OPPONENT;
        }

        if (gameState == STATE_WAIT_OPPONENT)
        {
            DrawText("Waiting for Player 2...", 40, 500, 20, RED);

            static char wait_buf[sizeof(int)];
            static int wait_total = 0;

            int bytes = recv(client_sock,
                             wait_buf + wait_total,
                             sizeof(int) - wait_total,
                             0);

            if (bytes > 0) {
                wait_total += bytes;
                if (wait_total == (int)sizeof(int)) {
                    int temp;
                    memcpy(&temp, wait_buf, sizeof(int));
                    player2move = temp;
                    wait_total = 0;
                    int cancelType = 0;
                    if (player1move == 5 && player2move == 5) cancelType = 3;
                    else if (player1move == 5) cancelType = 1;
                    else if (player2move == 5) cancelType = 2;

                    if (send_int(client_sock, cancelType) < 0)
                        die_with_error("Error: send() Failed.");

                    if (cancelType > 0) {
                        if (cancelType == 1 || cancelType == 3) {
                            player1.energy -= 5;
                        }
                        cancelMode = cancelType;
                        cancelAnimFrame = 0;
                        cancelAnimTimer = 0.0f;
                        PlaySound(cancelSound);
                        gameState = STATE_CANCEL_ANIM;
                    } else {
                        animFrame = 0;
                        animTimer = 0.0f;
                        holdTimer = 0;
                        gameState = STATE_ANIMATE;
                    }
                }
            } else if (bytes == 0) {
                die_with_error("Error: client disconnected.");
            } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
                die_with_error("Error: recv() Failed.");
            }
        }

        if (gameState == STATE_ANIMATE)
        {
            animTimer += GetFrameTime();
            if (animTimer >= FRAME_DURATION) {
                animTimer = 0.0f;
                bool shouldHold =
                    (player1move == 1 && player2move == 1 && animFrame >= 4) ||
                    (player1move == 3 && player2move != 0 && player2move != 1 && player2move != 5 && player2move != 6 && animFrame >= 3) ||
                    (player1move == 4 && player2move != 0 && player2move != 1 && player2move != 5 && player2move != 6 && animFrame >= 4) ||
                    (player2move == 3 && player1move != 0 && player1move != 1 && player1move != 5 && player1move != 6 && animFrame >= 3) ||
                    (player2move == 4 && player1move != 0 && player1move != 1 && player1move != 5 && player1move != 6 && animFrame >= 4);
                if (shouldHold) {
                    holdTimer++;
                } else {
                    animFrame++;
                }
            }

            if (animFrame == 5 && (player1move == 1 || player2move == 1)) {
                if (!IsSoundPlaying(shootSound)) {
                    PlaySound(shootSound);
                }
            }

            if (animFrame == 6 && ((player1move == 1 && player2move == 3) || (player1move == 3 && player2move == 1))) {
                if (!IsSoundPlaying(blockSound)) {
                    PlaySound(blockSound);
                }
            }

            if (animFrame == 6 && ((player1move == 1 && player2move == 4) || (player1move == 4 && player2move == 1))) {
                if (!IsSoundPlaying(deflectSound)) {
                    PlaySound(deflectSound);
                }
            }

            if (animFrame == 5 && (player1move == 2 || player2move == 2)) {
                if (!IsSoundPlaying(loadSound)) {
                    PlaySound(loadSound);
                }
            }

            if (animFrame == 5 && (player1move == 6 || player2move == 6)) {
                if (!IsSoundPlaying(doubleShotSound)) {
                    PlaySound(doubleShotSound);
                }
            }

            int p1TotalFrames = 0;
            if      (cancelMode == 2 && player2move == 1) p1TotalFrames = HIT_FRAMES;
            else if (cancelMode == 2 && player2move == 6) p1TotalFrames = 9;
            else if (player1move == 6 && player2move == 4) p1TotalFrames = SHOOTANDSHOT_FRAMES;
            else if (player1move == 3 && player2move == 6) p1TotalFrames = BLOCKANDSHOT_FRAMES;
            else if (player1move == 4 && player2move == 6) p1TotalFrames = DEFLECTANDSHOT_FRAMES;
            else if (player1move == 6) p1TotalFrames = DOUBLESHOT_FRAMES;
            else if (player1move == 3) p1TotalFrames = BLOCK_FRAMES;
            else if (player1move == 4) p1TotalFrames = DEFLECT_FRAMES;
            else if (player1move == 1 && player2move != 4) p1TotalFrames = SHOOT_FRAMES;
            else if (player1move == 2 && player2move == 6) p1TotalFrames = LOADANDHIT_FRAMES;
            else if (player1move == 2 && player2move != 1) p1TotalFrames = LOAD_FRAMES;
            else if (player1move == 2 && player2move == 1) p1TotalFrames = LOADANDHIT_FRAMES;
            else if (player1move == 1 && player2move == 4) p1TotalFrames = DEFLECTHIT_FRAMES;

            int p1Frame = animFrame;
            if (p1TotalFrames > 0 && p1Frame >= p1TotalFrames) p1Frame = p1TotalFrames - 1;

            if (cancelMode == 2 && player2move == 1) {
                int hf = p1Frame;
                if (hf >= HIT_FRAMES) hf = HIT_FRAMES - 1;
                DrawTextureEx(player1HitFrames[hf],
                              (Vector2){p1X, p1Y},
                              2.0f, bgScale * 4.0f, WHITE);
            } else if (cancelMode == 2 && player2move == 6) {
                int hitFrame = p1Frame + 5;
                if (hitFrame >= LOADANDHIT_FRAMES) hitFrame = LOADANDHIT_FRAMES - 1;
                DrawTextureEx(player1LoadAndHitFrames[hitFrame],
                              (Vector2){p1X, p1Y},
                              2.0f, bgScale * 4.0f, WHITE);
            } else if (player1move == 6 && player2move == 4) {
                DrawTextureEx(player1ShootAndShotFrames[p1Frame],
                              (Vector2){p1X, p1Y},
                              2.0f, bgScale * 4.0f, WHITE);
            } else if (player1move == 3 && player2move == 6) {
                DrawTextureEx(player1BlockAndShotFrames[p1Frame],
                              (Vector2){p1X, p1Y},
                              2.0f, bgScale * 4.0f, WHITE);
            } else if (player1move == 4 && player2move == 6) {
                DrawTextureEx(player1DeflectAndShotFrames[p1Frame],
                              (Vector2){p1X, p1Y},
                              2.0f, bgScale * 4.0f, WHITE);
            } else if (player1move == 6) {
                DrawTextureEx(player1DoubleShotFrames[p1Frame],
                              (Vector2){p1X, p1Y},
                              2.0f, bgScale * 4.0f, WHITE);
            } else if (player1move == 1 && player2move != 4) {
                DrawTextureEx(player1ShootFrames[p1Frame],
                              (Vector2){p1X, p1Y},
                              2.0f, bgScale * 4.0f, WHITE);
            } else if (player1move == 2 && player2move == 1) {
                DrawTextureEx(player1LoadAndHitFrames[p1Frame],
                              (Vector2){p1X, p1Y},
                              2.0f, bgScale * 4.0f, WHITE);
            } else if (player1move == 2 && player2move == 6) {
                DrawTextureEx(player1LoadAndHitFrames[p1Frame],
                              (Vector2){p1X, p1Y},
                              2.0f, bgScale * 4.0f, WHITE);
            } else if (player1move == 2 && player2move != 1) {
                DrawTextureEx(player1LoadFrames[p1Frame],
                              (Vector2){p1X, p1Y},
                              2.0f, bgScale * 4.0f, WHITE);
            } else if (player1move == 1 && player2move == 4) {
                DrawTextureEx(player1DeflectHitFrames[p1Frame],
                              (Vector2){p1X, p1Y},
                              2.0f, bgScale * 4.0f, WHITE);
            } else if (player1move == 3) {
                DrawTextureEx(player1BlockFrames[p1Frame],
                              (Vector2){p1X, p1Y},
                              2.0f, bgScale * 4.0f, WHITE);
            } else if (player1move == 4) {
                DrawTextureEx(player1DeflectFrames[p1Frame],
                              (Vector2){p1X, p1Y},
                              2.0f, bgScale * 4.0f, WHITE);
            } else {
                DrawTextureEx(player1Tex, (Vector2){p1X, p1Y},
                              2.0f, bgScale * 4.0f, WHITE);
            }

            int p2TotalFrames = 0;
            if      (cancelMode == 1 && player1move == 1) p2TotalFrames = HIT_FRAMES;
            else if (cancelMode == 1 && player1move == 6) p2TotalFrames = 9;
            else if (player2move == 6 && player1move == 4) p2TotalFrames = SHOOTANDSHOT_FRAMES;
            else if (player2move == 3 && player1move == 6) p2TotalFrames = BLOCKANDSHOT_FRAMES;
            else if (player2move == 4 && player1move == 6) p2TotalFrames = DEFLECTANDSHOT_FRAMES;
            else if (player2move == 6) p2TotalFrames = DOUBLESHOT_FRAMES;
            else if (player2move == 3) p2TotalFrames = BLOCK_FRAMES;
            else if (player2move == 4) p2TotalFrames = DEFLECT_FRAMES;
            else if (player2move == 1 && player1move != 4) p2TotalFrames = SHOOT_FRAMES;
            else if (player2move == 2 && player1move == 6) p2TotalFrames = LOADANDHIT_FRAMES;
            else if (player2move == 2 && player1move != 1) p2TotalFrames = LOAD_FRAMES;
            else if (player2move == 2 && player1move == 1) p2TotalFrames = LOADANDHIT_FRAMES;
            else if (player2move == 1 && player1move == 4) p2TotalFrames = DEFLECTHIT_FRAMES;

            int p2Frame = animFrame;
            if (p2TotalFrames > 0 && p2Frame >= p2TotalFrames) p2Frame = p2TotalFrames - 1;

            if (cancelMode == 1 && player1move == 1) {
                int hf = p2Frame;
                if (hf >= HIT_FRAMES) hf = HIT_FRAMES - 1;
                DrawTextureEx(player2HitFrames[hf],
                              (Vector2){p2X, p2Y},
                              2.0f, bgScale * 4.0f, WHITE);
            } else if (cancelMode == 1 && player1move == 6) {
                int hitFrame = p2Frame + 5;
                if (hitFrame >= LOADANDHIT_FRAMES) hitFrame = LOADANDHIT_FRAMES - 1;
                DrawTextureEx(player2LoadAndHitFrames[hitFrame],
                              (Vector2){p2X, p2Y},
                              2.0f, bgScale * 4.0f, WHITE);
            } else if (player2move == 6 && player1move == 4) {
                DrawTextureEx(player2ShootAndShotFrames[p2Frame],
                              (Vector2){p2X, p2Y},
                              2.0f, bgScale * 4.0f, WHITE);
            } else if (player2move == 3 && player1move == 6) {
                DrawTextureEx(player2BlockAndShotFrames[p2Frame],
                              (Vector2){p2X, p2Y},
                              2.0f, bgScale * 4.0f, WHITE);
            } else if (player2move == 4 && player1move == 6) {
                DrawTextureEx(player2DeflectAndShotFrames[p2Frame],
                              (Vector2){p2X, p2Y},
                              2.0f, bgScale * 4.0f, WHITE);
            } else if (player2move == 6) {
                DrawTextureEx(player2DoubleShotFrames[p2Frame],
                              (Vector2){p2X, p2Y},
                              2.0f, bgScale * 4.0f, WHITE);
            } else if (player2move == 1 && player1move != 4) {
                DrawTextureEx(player2ShootFrames[p2Frame],
                              (Vector2){p2X, p2Y},
                              2.0f, bgScale * 4.0f, WHITE);
            } else if (player2move == 2 && player1move == 1) {
                DrawTextureEx(player2LoadAndHitFrames[p2Frame],
                              (Vector2){p2X, p2Y},
                              2.0f, bgScale * 4.0f, WHITE);
            } else if (player2move == 2 && player1move == 6) {
                DrawTextureEx(player2LoadAndHitFrames[p2Frame],
                              (Vector2){p2X, p2Y},
                              2.0f, bgScale * 4.0f, WHITE);
            } else if (player2move == 2 && player1move != 1) {
                DrawTextureEx(player2LoadFrames[p2Frame],
                              (Vector2){p2X, p2Y},
                              2.0f, bgScale * 4.0f, WHITE);
            } else if (player2move == 1 && player1move == 4) {
                DrawTextureEx(player2DeflectHitFrames[p2Frame],
                              (Vector2){p2X, p2Y},
                              2.0f, bgScale * 4.0f, WHITE);
            } else if (player2move == 3) {
                DrawTextureEx(player2BlockFrames[p2Frame],
                              (Vector2){p2X, p2Y},
                              2.0f, bgScale * 4.0f, WHITE);
            } else if (player2move == 4) {
                DrawTextureEx(player2DeflectFrames[p2Frame],
                              (Vector2){p2X, p2Y},
                              2.0f, bgScale * 4.0f, WHITE);
            } else {
                DrawTextureEx(player2Tex, (Vector2){p2X, p2Y},
                              2.0f, bgScale * 4.0f, WHITE);
            }

            bool isHoldCase =
                (player1move == 1 && player2move == 1) ||
                (player1move == 3 && player2move != 0 && player2move != 1 && player2move != 5 && player2move != 6) ||
                (player1move == 4 && player2move != 0 && player2move != 1 && player2move != 5 && player2move != 6) ||
                (player2move == 3 && player1move != 0 && player1move != 1 && player1move != 5 && player1move != 6) ||
                (player2move == 4 && player1move != 0 && player1move != 1 && player1move != 5 && player1move != 6);

            if (isHoldCase) {
                if (holdTimer >= 6) {
                    animFrame = 0;
                    animTimer = 0.0f;
                    holdTimer = 0;
                    gameState = STATE_RESOLVE;
                }
            } else {
                int longest = p1TotalFrames > p2TotalFrames ? p1TotalFrames : p2TotalFrames;
                if (longest == 0) longest = BLOCK_FRAMES;

                if (animFrame >= longest) {
                    animFrame = 0;
                    animTimer = 0.0f;
                    gameState = STATE_RESOLVE;
                }
            }
        }

        if (gameState == STATE_CANCEL_ANIM)
        {
            cancelAnimTimer += GetFrameTime();
            if (cancelAnimTimer >= FRAME_DURATION) {
                cancelAnimTimer -= FRAME_DURATION;
                cancelAnimFrame++;
            }

            int drawFrame = cancelAnimFrame;
            if (drawFrame >= CANCEL_FRAMES) drawFrame = CANCEL_FRAMES - 1;

            if (cancelMode == 1 || cancelMode == 3) {
                DrawTextureEx(cancelFrames[drawFrame], (Vector2){p2X, p2Y},
                              0.0f, bgScale * 4.0f, WHITE);
            }
            if (cancelMode == 2 || cancelMode == 3) {
                DrawTextureEx(cancelFrames[drawFrame], (Vector2){p1X, p1Y},
                              0.0f, bgScale * 4.0f, WHITE);
            }

            if (cancelAnimFrame >= CANCEL_FRAMES) {
                cancelAnimFrame = 0;
                cancelAnimTimer = 0.0f;
                if (cancelMode == 2) {
                    gameState = STATE_CANCEL_WAIT;
                } else {
                    selectedIndex = 0;
                    inputMessage = "";
                    gameState = STATE_CANCEL_INPUT;
                }
            }
        }

        if (gameState == STATE_CANCEL_INPUT)
        {
            int row = selectedIndex / 3;
            int col = selectedIndex % 3;

            if (IsKeyPressed(KEY_RIGHT)) col = (col + 1) % 3;
            if (IsKeyPressed(KEY_LEFT))  col = (col + 2) % 3;
            if (IsKeyPressed(KEY_DOWN))  row = (row + 1) % 2;
            if (IsKeyPressed(KEY_UP))    row = (row + 1) % 2;

            int newIndex = row * 3 + col;
            if (newIndex > 5) newIndex = 5;
            selectedIndex = newIndex;

            if (IsKeyPressed(KEY_ENTER))
            {
                int chosenMove = selectedIndex + 1;

                if (chosenMove == 5) {
                    inputMessage = "Cannot cancel again!";
                    inputMessageColor = RED;
                } else if (chosenMove == 1 && player1.bullet <= 0) {
                    inputMessage = "No bullets! Cannot shoot.";
                    inputMessageColor = RED;
                } else if (chosenMove == 3 && player1.energy < 1) {
                    inputMessage = "Not enough energy to block.";
                    inputMessageColor = RED;
                } else if (chosenMove == 3 && !player1.canBlock) {
                    inputMessage = "Block is on cooldown!";
                    inputMessageColor = RED;
                } else if (chosenMove == 4 && player1.energy < 3) {
                    inputMessage = "Not enough energy to deflect.";
                    inputMessageColor = RED;
                } else if (chosenMove == 4 && !player1.canDeflect) {
                    inputMessage = "Deflect is on cooldown!";
                    inputMessageColor = RED;
                } else if (chosenMove == 6 && (player1.energy < 3 || player1.bullet < 2)) {
                    inputMessage = "Need 3 energy & 2 bullets for Double Shot.";
                    inputMessageColor = RED;
                } else {
                    player1move = chosenMove;
                    inputMessage = "";
                    if (cancelMode == 1) {
                        player2move = 0;
                        if (send_int(client_sock, player1move) < 0)
                            die_with_error("Error: send() Failed.");
                        animFrame = 0; animTimer = 0.0f; holdTimer = 0;
                        gameState = STATE_ANIMATE;
                    } else {
                        gameState = STATE_CANCEL_WAIT;
                    }
                }
            }
        }

        if (gameState == STATE_CANCEL_WAIT)
        {
            DrawText("Waiting for Player 2 follow-up...", 40, 500, 20, RED);

            static char cw_buf[sizeof(int)];
            static int cw_total = 0;

            int cw_bytes = recv(client_sock,
                                cw_buf + cw_total,
                                sizeof(int) - cw_total,
                                0);

            if (cw_bytes > 0) {
                cw_total += cw_bytes;
                if (cw_total == (int)sizeof(int)) {
                    int temp;
                    memcpy(&temp, cw_buf, sizeof(int));
                    player2move = temp;
                    cw_total = 0;

                    if (cancelMode == 3) {
                        if (send_int(client_sock, player1move) < 0)
                            die_with_error("Error: send() Failed.");
                    } else {
                        player1move = 0;
                    }
                    animFrame = 0; animTimer = 0.0f; holdTimer = 0;
                    gameState = STATE_ANIMATE;
                }
            } else if (cw_bytes == 0) {
                die_with_error("Error: client disconnected.");
            } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
                die_with_error("Error: recv() Failed.");
            }
        }

        if (gameState == STATE_RESOLVE)
        {
            if (cancelMode == 1) {
                printf("You cancelled Player 2's move and followed up!\n");
                switch(player1move) {
                    case 1:
                        printf("You shot! Player 2 was hit!\n\n");
                        player1.bullet--;
                        player2.health--;
                        break;
                    case 2:
                        printf("You loaded.\n\n");
                        if(player1.bullet < 6) player1.bullet++;
                        break;
                    case 3:
                        printf("You blocked (no threat).\n\n");
                        player1.energy--;
                        break;
                    case 4:
                        printf("You deflected (no threat).\n\n");
                        player1.energy -= 3;
                        break;
                    case 6:
                        printf("Double Shot! Player 2 takes 2 damage!\n\n");
                        player1.energy -= 3;
                        player1.bullet -= 2;
                        player2.health -= 2;
                        break;
                }
                cancelMode = 0;
            }
            else if (cancelMode == 2) {
                printf("Player 2 cancelled your move and followed up!\n");
                switch(player2move) {
                    case 1:
                        printf("Player 2 shot! You were hit!\n\n");
                        player1.health--;
                        break;
                    case 2:
                        printf("Player 2 loaded.\n\n");
                        break;
                    case 3:
                        printf("Player 2 blocked.\n\n");
                        break;
                    case 4:
                        printf("Player 2 deflected.\n\n");
                        break;
                    case 6:
                        printf("Player 2 Double Shot! You take 2 damage!\n\n");
                        player1.health -= 2;
                        break;
                }
                cancelMode = 0;
            }
            else {
                if (cancelMode == 3) {
                    cancelMode = 0;
                    printf("Both cancelled! Resolving second moves:\n");
                }
                if(player1move == 5 && player2move == 5){
                    printf("Both used Special Skill! Both turns were cancelled!\n\n");
                    player1.energy -= 5;
                }
                else if(player1move == 5){
                    printf("You used Special Skill!\n");
                    printf("Player 2's move was cancelled!\n\n");
                    player1.energy -= 5;
                }
                else if(player2move == 5){
                    printf("Player 2 used Special Skill!\n");
                    printf("Your move was cancelled!\n\n");
                }
                else{
                    switch(player1move){
                        case 1:
                            switch(player2move){
                                case 1:
                                    printf("Draw!\n\n");
                                    break;
                                case 2:
                                    printf("Player 2 loads!\n");
                                    printf("You shot!\n");
                                    printf("Player 2 lost 1 health!\n\n");
                                    player1.bullet--;
                                    player2.health--;
                                    break;
                                case 3:
                                    printf("Player 2 blocked your shot!\n\n");
                                    player1.bullet--;
                                    break;
                                case 4:
                                    printf("Player 2 deflected your shot!\n");
                                    printf("You got hit!\n\n");
                                    player1.bullet--;
                                    player1.health--;
                                    break;
                                case 6:
                                    printf("Player 2 used Double Shot! Your shot was cancelled!\n\n");
                                    player1.bullet--;
                                    break;
                            }
                            break;
                        case 2:
                            switch(player2move){
                                case 1:
                                    printf("You loaded!\n");
                                    printf("Player 2 shoots!\n");
                                    printf("You lost 1 health!\n\n");
                                    if(player1.bullet < 6) player1.bullet++;
                                    player1.health--;
                                    break;
                                case 2:
                                    printf("You loaded!\n");
                                    printf("Player 2 loads!\n\n");
                                    if(player1.bullet < 6) player1.bullet++;
                                    break;
                                case 3:
                                    printf("You loaded!\n");
                                    printf("Player 2 blocked, nothing happened.\n\n");
                                    if(player1.bullet < 6) player1.bullet++;
                                    break;
                                case 4:
                                    printf("You loaded!\n");
                                    printf("Player 2 deflected, nothing happened.\n\n");
                                    if(player1.bullet < 6) player1.bullet++;
                                    break;
                                case 6:
                                    printf("You loaded!\n");
                                    printf("Player 2 used Double Shot!\n\n");
                                    if(player1.bullet < 6) player1.bullet++;
                                    break;
                            }
                            break;
                        case 3:
                            player1.energy--;
                            switch(player2move){
                                case 1:
                                    printf("Player 2 shoots!\n");
                                    printf("You blocked the shot!\n\n");
                                    break;
                                case 2:
                                    printf("Player 2 loads!\n");
                                    printf("You blocked, nothing happened.\n\n");
                                    break;
                                case 3:
                                    printf("Player 2 blocked, nothing happened.\n");
                                    printf("You blocked, nothing happened.\n\n");
                                    break;
                                case 4:
                                    printf("Player 2 deflected, nothing happened.\n");
                                    printf("You blocked, nothing happened.\n\n");
                                    break;
                                case 6:
                                    printf("Player 2 used Double Shot! Your block stopped 1 bullet!\n\n");
                                    break;
                            }
                            break;
                        case 4:
                            player1.energy -= 3;
                            switch(player2move){
                                case 1:
                                    printf("You deflected the shot!\n");
                                    printf("Player 2 got hit!\n\n");
                                    player2.health--;
                                    break;
                                case 2:
                                    printf("Player 2 loads!\n");
                                    printf("You deflected, nothing happened.\n\n");
                                    break;
                                case 3:
                                    printf("Player 2 blocked, nothing happened.\n");
                                    printf("You deflected, nothing happened.\n\n");
                                    break;
                                case 4:
                                    printf("Player 2 deflected, nothing happened.\n");
                                    printf("You deflected, nothing happened.\n\n");
                                    break;
                                case 6:
                                    printf("Player 2 used Double Shot! 1 deflected!\n\n");
                                    break;
                            }
                            break;
                        case 6:
                            player1.energy -= 3;
                            player1.bullet -= 2;
                            switch(player2move){
                                case 1:
                                    printf("Double Shot hit! P2's shot cancelled! 2 DMG!\n\n");
                                    player2.health -= 2;
                                    break;
                                case 2:
                                    printf("Double Shot hit P2 for 2 damage!\n\n");
                                    player2.health -= 2;
                                    break;
                                case 3:
                                    printf("P2 blocked 1 bullet! 1 DMG dealt!\n\n");
                                    player2.health -= 1;
                                    break;
                                case 4:
                                    printf("1 deflected! Both took 1 DMG!\n\n");
                                    player2.health -= 1;
                                    player1.health -= 1;
                                    break;
                                case 6:
                                    printf("Collision! Both Double Shots cancelled!\n\n");
                                    break;
                            }
                            break;
                    }

                    if(player2move == 6 && player1move != 5 && player1move != 6) {
                        if(player1move == 1)      player1.health -= 2;
                        else if(player1move == 2) player1.health -= 2;
                        else if(player1move == 3) player1.health -= 1;
                        else if(player1move == 4) { player1.health -= 1; player2.health -= 1; }
                    }
                    }
            }

                    // CHECK IF GAME IS OVER :3
                    if(player1.energy < 6) {
                        player1.energy++;
                    }
                    player1.canBlock   = (player1move != 3);
                    player1.canDeflect = (player1move != 4);

                    int round_done = 1;

                    if (player1.health <= 0 || player2.health <= 0)
                    {
                        printf("Game Over\n");
                        gameOver = false;
                        round_done = 2;
                    }

                    // SEND ROUND STATUS TO PLAYER 2(to avoid being stuck on "Waiting for server...")
                    if(send_int(client_sock, round_done) < 0){
                        die_with_error("Error: send() Failed.");
                    }

                    // SYNC player2 health from client
                    int p2health_sync;
                    if(recv_int(client_sock, &p2health_sync) >= 0){
                        player2.health = p2health_sync;
                    }

                    // SYNC: send our actual health to client for display
                    if(send_int(client_sock, player1.health) < 0){
                        die_with_error("Error: send() Failed.");
                    }

                    selectedIndex = 1;

                    gameState = STATE_INPUT;
                }

        EndDrawing();
    }
    StopMusicStream(music);
    if (player2.health <= 0 && player1.health > 0) {
        PlaySound(winnerSound);
    } else {
        PlaySound(loserSound);
    }
    double endScreenStart = GetTime();
    while (!WindowShouldClose() && (GetTime() - endScreenStart) < 5.0) {
        UpdateMusicStream(music);
        BeginDrawing();
        ClearBackground(BLACK);
        float sw = (float)GetScreenWidth();
        float sh = (float)GetScreenHeight();
        if (player2.health <= 0 && player1.health > 0) {
            float scaleX = sw / (float)youWinTex.width;
            float scaleY = sh / (float)youWinTex.height;
            float scale = scaleX < scaleY ? scaleX : scaleY;
            DrawTextureEx(youWinTex,
                (Vector2){(sw - youWinTex.width * scale) / 2.0f,
                          (sh - youWinTex.height * scale) / 2.0f},
                0.0f, scale, WHITE);
        } else {
            float scaleX = sw / (float)youLostTex.width;
            float scaleY = sh / (float)youLostTex.height;
            float scale = scaleX < scaleY ? scaleX : scaleY;
            DrawTextureEx(youLostTex,
                (Vector2){(sw - youLostTex.width * scale) / 2.0f,
                          (sh - youLostTex.height * scale) / 2.0f},
                0.0f, scale, WHITE);
        }
        EndDrawing();
    }

    UnloadTexture(youWinTex);
    UnloadTexture(youLostTex);
    UnloadSound(winnerSound);
    UnloadSound(loserSound);
    UnloadMusicStream(music);
    UnloadSound(blockSound);
    UnloadSound(shootSound);
    UnloadSound(deflectSound);
    UnloadSound(loadSound);
    UnloadTexture(background);
    UnloadTexture(player1Tex);
    UnloadTexture(player2Tex);
    UnloadTexture(heartTex);
    UnloadTexture(energyTex);
    UnloadTexture(bulletTex);
    for (int i = 0; i < SHOOT_FRAMES; i++) {
        UnloadTexture(player1ShootFrames[i]);
    }
    for (int i = 0; i < LOAD_FRAMES; i++) {
        UnloadTexture(player1LoadFrames[i]);
    }
    for (int i = 0; i < BLOCK_FRAMES; i++) {
        UnloadTexture(player1BlockFrames[i]);
    }
    for (int i = 0; i < DEFLECT_FRAMES; i++) {
        UnloadTexture(player1DeflectFrames[i]);
    }
    for (int i = 0; i < LOADANDHIT_FRAMES; i++) {
        UnloadTexture(player1LoadAndHitFrames[i]);
    }
    for (int i = 0; i < SHOOTANDSHOT_FRAMES; i++) {
        UnloadTexture(player1ShootAndShotFrames[i]);
    }
    for (int i = 0; i < BLOCKANDSHOT_FRAMES; i++) {
        UnloadTexture(player1BlockAndShotFrames[i]);
    }
    for (int i = 0; i < DEFLECTANDSHOT_FRAMES; i++) {
        UnloadTexture(player1DeflectAndShotFrames[i]);
    }
    for (int i = 0; i < DEFLECT_FRAMES; i++) {
        UnloadTexture(player2DeflectFrames[i]);
    }
    for (int i = 0; i < BLOCK_FRAMES; i++) {
        UnloadTexture(player2BlockFrames[i]);
    }
    for (int i = 0; i < SHOOT_FRAMES; i++) {
        UnloadTexture(player2ShootFrames[i]);
    }
    for (int i = 0; i < LOAD_FRAMES; i++) {
        UnloadTexture(player2LoadFrames[i]);
    }
    for (int i = 0; i < LOADANDHIT_FRAMES; i++) {
        UnloadTexture(player2LoadAndHitFrames[i]);
    }
    for (int i = 0; i < HIT_FRAMES; i++) {
        UnloadTexture(player1HitFrames[i]);
    }
    for (int i = 0; i < HIT_FRAMES; i++) {
        UnloadTexture(player2HitFrames[i]);
    }
    for (int i = 0; i < DOUBLESHOT_FRAMES; i++) {
        UnloadTexture(player1DoubleShotFrames[i]);
    }
    for (int i = 0; i < DOUBLESHOT_FRAMES; i++) {
        UnloadTexture(player2DoubleShotFrames[i]);
    }
    for (int i = 0; i < SHOOTANDSHOT_FRAMES; i++) {
        UnloadTexture(player2ShootAndShotFrames[i]);
    }
    for (int i = 0; i < BLOCKANDSHOT_FRAMES; i++) {
        UnloadTexture(player2BlockAndShotFrames[i]);
    }

    for (int i = 0; i < DEFLECTANDSHOT_FRAMES; i++) {
        UnloadTexture(player2DeflectAndShotFrames[i]);
    }
    for (int i = 0; i < CANCEL_FRAMES; i++) {
        UnloadTexture(cancelFrames[i]);
    }
    for (int i = 0; i < INTRO_FRAMES; i++) {
        UnloadTexture(introFrames[i]);
    }

    CloseAudioDevice();
    CloseWindow();
    printf("Closing connection ...\n");

    close(client_sock);
    close(server_sock);

    return 0;
}