#include <SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <render.h>

// DEFINES
#define WIN_WIDTH 640
#define WIN_HEIGHT 480
#define WIN_FLAGS 0 //SDL_WINDOW_FULLSCREEN
#define FIRST_AVAILABLE_RENDERER -1
#define RENDERER_FLAGS SDL_RENDERER_ACCELERATED
#define MAX_NUM_LEAVES 1000
#define NUM_LEAF_STATES 4
#define TREE_X (WIN_WIDTH - 200)
#define TREE_Y (WIN_HEIGHT - 200)
#define GROUND_LEVEL 70
#define CLOUD_MAX_SPEED 10
#define MAX_NUM_CLOUDS 20
#define NUM_LOOP_TYPES 3
#define MAX_LOOP_LENGTH 17

#define FONT_WIDTH 35
#define FONT_HEIGHT 75
#define FONT_SIZE_RATIO ((float)FONT_WIDTH / (float)FONT_HEIGHT)
typedef struct leafS
{
  int state;
  int x;
  int y;
  bool onGround;
  int lifetime;
  bool loop;
  int loopIndex;
  int loopType;
} leafS;

typedef struct loopS
{
  int x;
  int y;
} loopS;

leafS leaves[MAX_NUM_LEAVES];
int numLeaves = 0;

typedef struct cloudS
{
  int state;
  int x;
  int y;
  int height;
  int width;
  int spriteY;
  int speed;
} cloudS;

cloudS clouds[MAX_NUM_CLOUDS];
int numClouds = 0;

#define L_U 3 // the min unit of loop movement
loopS loops[NUM_LOOP_TYPES][MAX_LOOP_LENGTH] =
{
  {{.x = -2, .y= -1},{.x=0, .y=-4},{.x=4, .y=0},{.x=0, .y=-4}},
  {{},{},{},{},{},{},{},{}},
  {{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{}}
};

int maxLoopIndex[NUM_LOOP_TYPES] = {4, 8, 16};

static SDL_Window* myWindow_p;
static SDL_Renderer* myRenderer_p;
static SDL_Texture* asciiTexture_p;
static SDL_Texture* cloudTexture_p;
static SDL_Texture* leavesTexture_p;
static int gridSize = 0;
static int windSpeed = 0;

static void calcSourceRect(SDL_Rect* rect, char inputChar);
static void drawScore(int score, int intervalMs);
static void drawLeaves();
static void drawTree();
static void drawGround();
static void drawSky();
static void drawClouds();
static void removeCloud();
static void createCloud();
static void drawText(char* string, int charSize, int x, int y);
static void initLoops();

uint32_t updateBackground(uint32_t interval, void* parameters);

void renderDestroy(void)
{
  SDL_DestroyTexture(asciiTexture_p);
  SDL_DestroyTexture(leavesTexture_p);
}

int renderInit(int gridSizeInput)
{
  initLoops();
  gridSize = gridSizeInput;
  myWindow_p = SDL_CreateWindow("Storm Clacker - typing in the wind.", 0, 0, WIN_WIDTH, WIN_HEIGHT, WIN_FLAGS);
  if (myWindow_p == NULL)
  {
    printf("Could not create window.\n");
    return -1;
  }

  myRenderer_p = SDL_CreateRenderer(
      myWindow_p,
      FIRST_AVAILABLE_RENDERER,
      RENDERER_FLAGS);

  if (myRenderer_p == NULL)
  {
    printf("Could not create renderer.\n");
    return -1;
  }

  // Set size of renderer to the same as window
  SDL_RenderSetLogicalSize(myRenderer_p, WIN_WIDTH, WIN_HEIGHT);
  
  // Create the texture that will be used to print background.
  SDL_Surface* surface;
  if (NULL == (surface = SDL_LoadBMP("./src/leaves2.bmp"))) printf("Error when loading BMP: %s\n", SDL_GetError());
  // Set 100th pixel as the transparent color.
  if (SDL_SetColorKey(surface,
                      SDL_TRUE,
                      *(((uint32_t*)(surface->pixels)) + 100)) != 0){
    printf("Error when changing color key: %s\n", SDL_GetError());
  }

  if (NULL == (leavesTexture_p = SDL_CreateTextureFromSurface(myRenderer_p,
                                                              surface))){
    printf("Error when creating texture: %s\n", SDL_GetError());
  }


  // Set blue pixel as the transparent color.
  if (SDL_SetColorKey(surface,
                      SDL_TRUE,
                      *(((uint32_t*)(surface->pixels)) + 126)) != 0){
    printf("Error when changing color key: %s\n", SDL_GetError());
  }

  if (NULL == (cloudTexture_p = SDL_CreateTextureFromSurface(myRenderer_p,
                                                              surface))){
    printf("Error when creating texture: %s\n", SDL_GetError());
  }

  SDL_FreeSurface(surface);
  
  // Create the texture that will be used to print ASCII.
  if (NULL == (surface = SDL_LoadBMP("./src/consolas.bmp"))) printf("Error when loading BMP: %s\n", SDL_GetError());
  // Set first pixel as the transparent color.
  if (SDL_SetColorKey(surface,
                      SDL_TRUE,
                      *((uint32_t*)(surface->pixels)+100)) != 0)
  {
    printf("Error when changing color key: %s\n", SDL_GetError());
  }

  if (NULL == (asciiTexture_p = SDL_CreateTextureFromSurface(myRenderer_p,
                                                   surface))) printf("Error when creating texture: %s\n", SDL_GetError());
  SDL_FreeSurface(surface);


  // Create a timer that will move background now and then.
  const int newTargetTimeoutMs = 100;
  SDL_TimerID my_timer_id = SDL_AddTimer(newTargetTimeoutMs, updateBackground, 0);
  if (my_timer_id == 0) printf("Timer error: %s\n", SDL_GetError());
  return 0;
}

void render(char* input_p, int score, int intervalMs)
{
  if (SDL_SetRenderDrawColor(myRenderer_p, 20, 20, 255, 255) != 0) printf("Color error\n");
  SDL_RenderClear(myRenderer_p);

  drawSky();
  drawGround();
  drawClouds();
  drawLeaves();
  drawTree();
  
  for (int x = 0; x < gridSize; x++)
  {
    for (int y = 0; y < gridSize; y++)
    {
      int inputChar = (int)*(input_p + (y * gridSize) + x);
      SDL_Rect sourceRect;
      calcSourceRect(&sourceRect, inputChar);
      SDL_Rect destRect;
      int height = WIN_HEIGHT / gridSize; 
      int horizontalSpacing = WIN_WIDTH / gridSize;
      int width = height * FONT_SIZE_RATIO; 
      destRect.x = x * horizontalSpacing + (horizontalSpacing/2 - width);
      destRect.y = y * height;
      destRect.w = width;
      destRect.h = height;
      if (SDL_RenderCopy(myRenderer_p, asciiTexture_p, &sourceRect, &destRect)) printf("Error when RenderCopy: %s\n", SDL_GetError());
    }
  } 
  drawScore(score, intervalMs);
  SDL_RenderPresent(myRenderer_p);
}

void renderScoreBoard(scoreS* hiScoreList, int numberOfScores)
{
  if (SDL_SetRenderDrawColor(myRenderer_p, 255, 255, 255, 255) != 0) printf("Color error\n");
  SDL_RenderClear(myRenderer_p);

  int startY = 40;
  int charSize = (WIN_HEIGHT - 2 * startY) / 12; //TODO make this the same def or get real val from caller.
  char scoreString[255];
  int i;
  sprintf(scoreString, "PLAYER - SCORE");

  int startX = (WIN_WIDTH - strlen(scoreString) * charSize * FONT_SIZE_RATIO) / 2;

  drawText(scoreString, charSize, startX, startY);

  for (i = 0; i < numberOfScores; i++)
  {
    sprintf(scoreString, "%6s - %d", hiScoreList[i].name, hiScoreList[i].score);
    drawText(scoreString, charSize, startX, startY + ((i+1) * charSize));
  }

  sprintf(scoreString, "ENTER CONFIRMS AND RESTARTS");
  int infoCharSize = 20;
  startX = (WIN_WIDTH - strlen(scoreString) * infoCharSize * FONT_SIZE_RATIO) / 2;
  drawText(scoreString, infoCharSize, startX, startY + ((i+1) * charSize));
  SDL_RenderPresent(myRenderer_p);

}

/* LOCAL FUNCTIONS */

static void initLoops(void)
{
  for (int loopType = 0; loopType < NUM_LOOP_TYPES; loopType++)
  {
    loopS tmpLoops[MAX_LOOP_LENGTH + 1];
    tmpLoops[0].x = 0;
    tmpLoops[0].y = 0;
    int numberOfLoops = 1 + maxLoopIndex[loopType];
    for (int loopIndex = 0; loopIndex < numberOfLoops; loopIndex++)
    {
      loopS* loop_p = &loops[loopType][loopIndex];
      tmpLoops[loopIndex + 1].x = 3 * (loopType + 1) * (-sin(2*M_PI*loopIndex/numberOfLoops + (M_PI / numberOfLoops)));
      tmpLoops[loopIndex + 1].y = 3 * (loopType + 1) * (1-cos(2*M_PI*loopIndex/numberOfLoops + (M_PI / numberOfLoops)));
      loop_p->x = tmpLoops[loopIndex + 1].x - tmpLoops[loopIndex].x;
      loop_p->y = tmpLoops[loopIndex + 1].y - tmpLoops[loopIndex].y;
      //printf("Loop  %d, %d, %d, %d\n", loopType, loopIndex, loop_p->x,  loop_p->y); 
    }
  }
}

static void drawSky()
{
  SDL_Rect sourceRect;
  sourceRect.x = 95;
  sourceRect.y = 4;
  sourceRect.h = 1;
  sourceRect.w = 1;
  SDL_Rect destRect;
  destRect.x = 1;
  destRect.y = 1;
  destRect.w = WIN_WIDTH;
  destRect.h = WIN_HEIGHT;

  if (SDL_RenderCopy(myRenderer_p, leavesTexture_p, &sourceRect, &destRect)) printf("Error when RenderCopy: %s\n", SDL_GetError());
}


static void drawGround()
{
  SDL_Rect sourceRect;
  sourceRect.x = 94;
  sourceRect.y = 40;
  sourceRect.h = 9;
  sourceRect.w = 80;
  SDL_Rect destRect;
  destRect.x = 0;
  destRect.y = WIN_HEIGHT - GROUND_LEVEL;
  destRect.w = WIN_WIDTH;
  destRect.h = GROUND_LEVEL;

  if (SDL_RenderCopy(myRenderer_p, leavesTexture_p, &sourceRect, &destRect)) printf("Error when RenderCopy: %s\n", SDL_GetError());
}

static void drawTree()
{
  int spriteY = 3;
  int spriteX = 6;
  if (windSpeed == 4)
  {
    spriteY = 56;
    spriteX = 7;
  }
  else if (windSpeed == 8)
  {
    spriteY = 108;
    spriteX = 7;
  }

  SDL_Rect sourceRect;
  sourceRect.x = spriteX;
  sourceRect.y = spriteY;
  sourceRect.h = 44;
  sourceRect.w = 44;
  SDL_Rect destRect;
  int height = WIN_HEIGHT / gridSize; 
  int width = WIN_WIDTH / gridSize; 
  destRect.x = TREE_X;
  destRect.y = TREE_Y;
  destRect.w = 150;
  destRect.h = 150;

  if (SDL_RenderCopy(myRenderer_p, leavesTexture_p, &sourceRect, &destRect)) printf("Error when RenderCopy: %s\n", SDL_GetError());
}

static void drawLeaves()
{
  for (int i = 0; i < MAX_NUM_LEAVES; i++)
  {
    if (leaves[i].state != 0)
    {
      SDL_Rect sourceRect;
      sourceRect.x = 57;
      sourceRect.y = 4 + leaves[i].state;
      sourceRect.h = 1;
      sourceRect.w = 1;
      SDL_Rect destRect;
      int height = WIN_HEIGHT / gridSize; 
      int width = WIN_WIDTH / gridSize; 
      destRect.x = leaves[i].x;
      destRect.y = leaves[i].y; 
      destRect.w = 3;
      destRect.h = 3;

      if (SDL_RenderCopy(myRenderer_p, leavesTexture_p, &sourceRect, &destRect)) printf("Error when RenderCopy: %s\n", SDL_GetError());
    }
  }
}

static void drawScore(int score, int intervalMs)
{
#define STRING_SIZE 100
#define SCORE_CHAR_SIZE 16;
  char scoreString[STRING_SIZE];
  SDL_Rect destRect;
   destRect.x = 0;
   destRect.y = 0;
   destRect.w = SCORE_CHAR_SIZE;
   destRect.h = SCORE_CHAR_SIZE;
  int numChars = snprintf(scoreString, STRING_SIZE, "Score: %6d @ %3.2f chars per minute.", score, 1000.0/intervalMs);
  for (int i = 0; i < numChars; i++)
  {
   SDL_Rect sourceRect;
   calcSourceRect(&sourceRect, scoreString[i]);
   destRect.x += SCORE_CHAR_SIZE;   

   if (SDL_RenderCopy(myRenderer_p, asciiTexture_p, &sourceRect, &destRect)) printf("Error when RenderCopy: %s\n", SDL_GetError());
  } 
}

static void drawText(char* string, int charSize, int x, int y)
{
  int charWidth = (int)(charSize * FONT_SIZE_RATIO);
  SDL_Rect destRect;
   destRect.x = x;
   destRect.y = y;
   destRect.h = charSize;
   destRect.w = charWidth;
  int numChars = strlen(string);
  for (int i = 0; i < numChars; i++)
  {
   SDL_Rect sourceRect;
   calcSourceRect(&sourceRect, string[i]);
   destRect.x += charWidth;

   if (SDL_RenderCopy(myRenderer_p, asciiTexture_p, &sourceRect, &destRect)) printf("Error when RenderCopy: %s\n", SDL_GetError());
  }
}

static void calcSourceRect(SDL_Rect* rect, char inputChar)
{
  rect->w = FONT_WIDTH;//35;
  rect->h = FONT_HEIGHT; //75;
  int bitmapX = rect->w * (inputChar % 16);
  int bitmapY = rect->h * (inputChar / 16);
  rect->x = bitmapX;
  rect->y = bitmapY;
}

static void removeCloud(int i)
{
  memcpy(&clouds[i], &clouds[numClouds - 1], sizeof(cloudS));
  clouds[numClouds - 1].state = 0;
  numClouds--;
}

static void removeLeaf(int i)
{
  memcpy(&leaves[i], &leaves[numLeaves - 1], sizeof(leafS));
  leaves[numLeaves - 1].state = 0;
  numLeaves--;
}


uint32_t updateBackground(uint32_t interval, void* parameters)
{
#define MAX_SPEED 3
  windSpeed = (rand() % MAX_SPEED << 2);
  int newLeafRand = rand();
  if ((newLeafRand % 20 + windSpeed) > 18 && numLeaves < MAX_NUM_LEAVES)
  {
    leafS* newLeaf_p = &leaves[numLeaves];
    numLeaves++;
    newLeaf_p->x = TREE_X + 20 + rand()%60;
    newLeaf_p->y = TREE_Y + 20; 
    newLeaf_p->state = newLeafRand % 3 + 1;
    newLeaf_p->onGround = false;
    newLeaf_p->lifetime = 500;
  }
  else if (newLeafRand%20 > 16 && numLeaves < MAX_NUM_LEAVES)
  {
    leafS* newLeaf_p = &leaves[numLeaves];
    numLeaves++;
    newLeaf_p->x = WIN_WIDTH + 20;
    newLeaf_p->y = WIN_HEIGHT - GROUND_LEVEL - 10; 
    newLeaf_p->state = newLeafRand % 3 + 1;
    newLeaf_p->onGround = false;
    newLeaf_p->lifetime = 500;
  }

  for (int i = 0; i < MAX_NUM_LEAVES; i++)
  {
    leafS* leaf_p = &leaves[i];
    if (leaf_p->state != 0) // only active leaves
    {
      if (windSpeed  == ((MAX_SPEED-1) << 2) && (rand()%40) == 0)
      {
        leaf_p->loop = true;
        leaf_p->loopType = rand()%3;
        leaf_p->loopIndex = 0; 
        leaf_p->onGround = false;
      }
      if (leaf_p->onGround == false)
      {
        if (leaf_p->loop == true)
        {
         int loopType =  leaf_p->loopType;
         int loopIndex = leaf_p->loopIndex;
          leaf_p->x += loops[loopType][loopIndex].x - windSpeed;
          leaf_p->y -= loops[loopType][loopIndex].y;
          leaf_p->loopIndex++;
          if (leaf_p->loopIndex == maxLoopIndex[loopType]) leaf_p->loop = false;
        }
        else
        {
          int thisSpeed = windSpeed + rand()%(MAX_SPEED + 4) - 4; 
          int thisSpeedY = -2;
          leaf_p->x -= thisSpeed;
          leaf_p->y -= thisSpeedY;

          if (leaf_p->y > (WIN_HEIGHT - GROUND_LEVEL + thisSpeed))
          {
            leaf_p->onGround = true;
            leaf_p->y += rand()%5;
          }
        }
        if (leaf_p->x < 0) removeLeaf(i);
      }
      else // active leaf on ground
      {
        leaf_p->lifetime-=1;
        if (leaf_p->lifetime <= 0) removeLeaf(i);
      }
    }
  }


  if (rand()%(WIN_WIDTH>>4) == 0)
  {
    createCloud();
  }

  //printf("Moving clouds %d\n", numClouds);
  for (int i = 0; i < MAX_NUM_CLOUDS; i++)
  {
    if (clouds[i].state != 0) // only active clouds
    {
        clouds[i].x -= clouds[i].speed;

        if ((clouds[i].x + clouds[i].width) < 0) removeCloud(i);
      }
  }
  SDL_Event event;
  SDL_UserEvent userevent;

  userevent.type = SDL_USEREVENT;
  userevent.code = 0;
  userevent.data1 = &updateBackground;
  userevent.data2 = parameters;

  event.type = SDL_USEREVENT;
  event.user = userevent;

  SDL_PushEvent(&event);
  return interval;
}

static void createCloud()
{
#define CLOUD_MAX_HEIGHT 50
#define MAX_SCALE 4;
  if (numClouds < MAX_NUM_CLOUDS)
  {
    cloudS* cloud_p = &clouds[numClouds];
    cloud_p->state = 1;
    const int max_y = WIN_HEIGHT - GROUND_LEVEL - CLOUD_MAX_HEIGHT;
    cloud_p->y = rand()%(max_y) + 1;
    cloud_p->x = WIN_WIDTH; 
    float scale = 1 - ((float)cloud_p->y / max_y);
    cloud_p->height =  CLOUD_MAX_HEIGHT * scale;
    cloud_p->width = 200 * scale;
    cloud_p->speed = ((CLOUD_MAX_SPEED >> 2) * scale) + (CLOUD_MAX_SPEED >> 2);
    int spriteNum = rand()%3;
    switch (spriteNum)
    {
      case 0:
        cloud_p->spriteY = 80;
        break;
      case 1:
        cloud_p->spriteY = 133;
        break;
      case 2:
        cloud_p->spriteY = 190;
        break;
      default:
        printf("Wrong case in cloudCreation!");

    }
    numClouds++;
  }
}

static void drawClouds()
{
  for (int i = 0; i < MAX_NUM_CLOUDS; i++)
  {

    if (clouds[i].state != 0)
    {
      //printf("Cloud %d, x %d y %d\n", i, clouds[i].x, clouds[i].y);
      SDL_Rect sourceRect;
      sourceRect.x = 82;
      sourceRect.y = clouds[i].spriteY;
      sourceRect.h = 20;
      sourceRect.w = 149 - 82;
      SDL_Rect destRect;
      int height = clouds[i].height; 
      int width =  clouds[i].width;
      destRect.x = clouds[i].x;
      destRect.y = clouds[i].y; 
      destRect.w = width;
      destRect.h = height;

      if (SDL_RenderCopy(myRenderer_p, cloudTexture_p, &sourceRect, &destRect)) printf("Error when RenderCopy: %s\n", SDL_GetError());
    }
    else
    {
      break;
    }
  }
}
