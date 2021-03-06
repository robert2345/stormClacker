#include <SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <render.h>

// DEFINES
#define PIXEL_SIZE 5
#define SHINE 0xA0
#define SHADE 0xA0
#define STRING_LENGTH (WIN_WIDTH + 2 * PIXEL_SIZE) // One datapoint before visible and one after
#define CYLINDER_HEIGHT 150
#define CYLINDER_HEIGHT 150
#define WIN_WIDTH 640
#define WIN_HEIGHT 480
#define WIN_FLAGS 0 //SDL_WINDOW_FULLSCREEN
#define FIRST_AVAILABLE_RENDERER -1
#define RENDERER_FLAGS SDL_RENDERER_ACCELERATED
#define MAX_NUM_LEAVES 1000
#define NUM_LEAF_STATES 4
#define TREE_X (WIN_WIDTH - 200)
#define TREE_Y (WIN_HEIGHT - 175)
#define GROUND_LEVEL 40
#define MOUNTAIN_LEVEL 85
#define CLOUD_MAX_SPEED 10
#define MAX_NUM_CLOUDS 20
#define NUM_LOOP_TYPES 3
#define MAX_LOOP_LENGTH 17
#define MAX_PHI 2.5

#define FONT_WIDTH 35
#define FONT_HEIGHT 75
#define FONT_SIZE_RATIO ((float)FONT_WIDTH / (float)FONT_HEIGHT)
typedef struct leaf
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

struct coord {
    double x;
    double y;
    double z;
};
struct face {
    struct coord *coords[4];
};

struct cube {
    struct face faces[6];
    struct face *sortedFaces[6];
};

typedef struct string
{
  int state;
  int x;
  double y;
  double last_y;
  double phi;
  double last_phi;
} stringS;

typedef struct loopS
{
  int x;
  int y;
} loopS;

leafS leaves[MAX_NUM_LEAVES];
stringS strings[STRING_LENGTH];
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

int bumpMap[WIN_WIDTH][WIN_HEIGHT];

cloudS clouds[MAX_NUM_CLOUDS];
int numClouds = 0;

int cylinderTextureHeight;
int cylinderTextureWidth;

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
static SDL_Texture* cylinderTexture_p;
static int gridSize = 0;
static int windSpeed = 0;
static double lightAngle = 0;
static struct cube cube;

static void initLens(void);
static void updateLens(void);
static void drawLens(void);
static void calcSourceRect(SDL_Rect* rect, char inputChar);
static void drawScore(int score, int intervalMs);
static void drawLeaves();
static void drawString();
static void drawTree();
static void drawGround();
static void drawSky();
static void drawClouds();
static void removeCloud();
static void createCloud();
static void drawText(char* string, int charSize, int x, int y);
static void initLoops();
static void stringWarpInit();
static void initStrings();
static void initCube();
static void updateCube();
static void drawCube();
static void faceSortInsert(struct face *thisFace, struct face **sortedFaces);

uint32_t updateBackground(uint32_t interval, void* parameters);
static void updateString();

void renderDestroy(void)
{
  SDL_DestroyTexture(asciiTexture_p);
  SDL_DestroyTexture(leavesTexture_p);
  SDL_DestroyTexture(cylinderTexture_p);
}

int renderInit(int gridSizeInput)
{
  initLoops();
  initStrings();
  stringWarpInit();
  initLens();
  initCube();

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

  /* Create the cylinder texture! */
  if (NULL == (surface = SDL_LoadBMP("./src/cylinder.bmp"))) printf("Error when loading BMP: %s\n", SDL_GetError());
  
  cylinderTextureHeight = surface->h;
  cylinderTextureWidth = surface->w;
  if (NULL == (cylinderTexture_p = SDL_CreateTextureFromSurface(myRenderer_p,
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
  //drawClouds();
  //drawLeaves();
  //updateString();
  //drawString();
  drawTree();
  //updateLens();
  //drawLens();
  drawCube();
  
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
static void initLens(void)
{
    memset(&bumpMap[0][0], 128, WIN_WIDTH*WIN_HEIGHT*sizeof(int));
}

static void initStrings(void)
{

  for (int i = 0; i < WIN_WIDTH; i++)
  {
  }

}

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
  sourceRect.x = 62;
  sourceRect.y = 17;
  sourceRect.h = 64-17;
  sourceRect.w = 318-62;
  SDL_Rect destRect;
  destRect.x = 0;
  destRect.y = WIN_HEIGHT - GROUND_LEVEL - MOUNTAIN_LEVEL;
  destRect.w = WIN_WIDTH;
  destRect.h = GROUND_LEVEL + MOUNTAIN_LEVEL;

  if (SDL_RenderCopy(myRenderer_p, leavesTexture_p, &sourceRect, &destRect)) printf("Error when RenderCopy: %s\n", SDL_GetError());
}

static void drawTree()
{
  int spriteY = 1;
  int spriteX = 1;
  if (windSpeed == 4)
  {
    spriteY = 53;
  }
  else if (windSpeed == 8)
  {
    spriteY = 105;
  }

  SDL_Rect sourceRect;
  sourceRect.x = spriteX;
  sourceRect.y = spriteY;
  sourceRect.h = 44;
  sourceRect.w = 44;
  SDL_Rect destRect;
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
      destRect.x = leaves[i].x;
      destRect.y = leaves[i].y; 
      destRect.w = 3;
      destRect.h = 3;

      if (SDL_RenderCopy(myRenderer_p, leavesTexture_p, &sourceRect, &destRect)) printf("Error when RenderCopy: %s\n", SDL_GetError());
    }
  }
}

double y_over_cylinder[CYLINDER_HEIGHT];
static double phi_over_cylinder[CYLINDER_HEIGHT];
char color_over_cylinder[CYLINDER_HEIGHT];
char highlight_over_cylinder[CYLINDER_HEIGHT];
static double power(double in)
{
    return in*in*in*in*in*in*in*in;
}

static void stringWarpInit() 
{
    const double d_cylinderHeight = 1.0 * CYLINDER_HEIGHT;
    phi_over_cylinder[0] = -1.57079633;
    for (int y = 1; y < CYLINDER_HEIGHT; y+=1) { 
            phi_over_cylinder[y] = 2*M_PI*asin(y/(2.0*d_cylinderHeight) - 0.25);
    }

    for (int y = 0; y < CYLINDER_HEIGHT; y+=1) { 
            y_over_cylinder[y] = ((d_cylinderHeight/2.0 + phi_over_cylinder[y]/2.0/3.14 * d_cylinderHeight));
            //printf("phi %f, cos %f, y %f\n", phi_over_cylinder[y], cos(phi_over_cylinder[y]), y_over_cylinder[y]);
    }

    for (int y = 0; y < CYLINDER_HEIGHT; y+=1) { 
            color_over_cylinder[y] = (char)round(SHADE * cos(phi_over_cylinder[y]));
            highlight_over_cylinder[y] = (char)round(SHINE * power(cos(phi_over_cylinder[y])));
            printf("highlight %hhu\n", highlight_over_cylinder[y]);
    }
}

static double fsqr(double in) 
{
    return in*in*in;
}

static double calcTwistFactor(double phiDiff)
{
    if (phiDiff > 6.28 || phiDiff < -6.28) printf("phiDiff= %f\n", phiDiff);

    double out = 0.5*(1 + cos(7*fabs(phiDiff)));

    if (out < 0.1) printf("twist = %f\n", out);

    return out;
}

static void drawString()
{
    const double src_x_step = cylinderTextureWidth/WIN_WIDTH;
    const double src_y_step = cylinderTextureHeight/2/CYLINDER_HEIGHT;
    double twistScaleFactor;
    const double dstPixelsPerRad = CYLINDER_HEIGHT/M_PI;
    for (int x = PIXEL_SIZE; x < WIN_WIDTH + PIXEL_SIZE; x+=PIXEL_SIZE)
    {

        twistScaleFactor = calcTwistFactor(strings[x - PIXEL_SIZE].phi - strings[x + PIXEL_SIZE].phi);

        for (int y = 0; y < CYLINDER_HEIGHT; y+=PIXEL_SIZE)
        {
            const double d_cylinderHeight = 1.0 * CYLINDER_HEIGHT;
            SDL_Rect sourceRect;
            sourceRect.x = (x-PIXEL_SIZE) * src_x_step;
            sourceRect.y = (strings[x].y + y_over_cylinder[y]);
            if (sourceRect.y < 0) sourceRect.y = cylinderTextureHeight - (abs(sourceRect.y) % cylinderTextureHeight);
            else sourceRect.y %= cylinderTextureHeight;
            sourceRect.h = src_x_step;
            sourceRect.w = src_y_step;
            SDL_Rect destRect;
            destRect.x = (x-PIXEL_SIZE);
            destRect.y = round(WIN_HEIGHT/2 + ((y - CYLINDER_HEIGHT/2)*twistScaleFactor));
            destRect.w = PIXEL_SIZE;
            destRect.h = PIXEL_SIZE * twistScaleFactor *2;
            unsigned int current_color_y = y - round(dstPixelsPerRad * lightAngle);
            if (current_color_y > CYLINDER_HEIGHT) current_color_y = CYLINDER_HEIGHT-1;
            unsigned char color_mod = color_over_cylinder[current_color_y];
            SDL_SetTextureColorMod(cylinderTexture_p,
                           color_mod,
                           color_mod,
                           color_mod);
            if (SDL_RenderCopy(myRenderer_p, cylinderTexture_p, &sourceRect, &destRect)) printf("Error when RenderCopy: %s\n", SDL_GetError());

            /* Highlights */

            color_mod = highlight_over_cylinder[current_color_y];
            //printf("lightmod %hhu\n", color_mod);
            if (color_mod > 1 ) {
                sourceRect.x = 100;
                sourceRect.y = 87;
                SDL_SetTextureBlendMode(cloudTexture_p, SDL_BLENDMODE_ADD);
                SDL_SetTextureColorMod(cloudTexture_p,
                        color_mod,
                        color_mod,
                        color_mod);
                if (SDL_RenderCopy(myRenderer_p, cloudTexture_p, &sourceRect, &destRect)) printf("Error when RenderCopy: %s\n", SDL_GetError());
                SDL_SetTextureBlendMode(cloudTexture_p, SDL_BLENDMODE_NONE);
            }
        }
    }
}

static void drawScore(int score, int intervalMs)
{
#define STRING_SIZE 100
#define SCORE_CHAR_SIZE 25;
  char scoreString[STRING_SIZE];
  SDL_Rect destRect;
  destRect.x = 0;
  destRect.y = 0;
  destRect.h = SCORE_CHAR_SIZE;
  destRect.w = destRect.h * FONT_SIZE_RATIO;
  int numChars = snprintf(scoreString, STRING_SIZE, "Score: %6d @ %3.2f chars per minute.", score, 1000.0/intervalMs);
  for (int i = 0; i < numChars; i++)
  {
   SDL_Rect sourceRect;
   calcSourceRect(&sourceRect, scoreString[i]);
   destRect.x += destRect.w;   

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

static void drawLens()
{
    for (int x = 0; x < WIN_WIDTH; x++) {
        for (int y = 0; y < WIN_HEIGHT; y++) {
            char red = bumpMap[x][y] & 0xFF;
            char green = ( bumpMap[x][y]>>8) & 0xFF;
            char blue = (bumpMap[x][y]>>16) & 0xFF;
            SDL_SetRenderDrawColor(myRenderer_p, red, green, blue, SDL_ALPHA_OPAQUE);
            if (SDL_RenderDrawPoint(myRenderer_p, x, y) ) printf("Failed to draw point\n");

        }
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

static void updateString()
{
#define MAX_FORCE 75
    static int count=0;
    double phaseshift;
    const double dampening = 0.990;
    lightAngle = sin(count/50.0);
    phaseshift =(MAX_PHI * sin(count/40.0) + MAX_PHI * sin(count/28.0));
    count++;
    stringS *this = &strings[0];
    this->last_phi = phaseshift;
    this->last_y = (phaseshift/2/3.14 * cylinderTextureHeight);

    for (int i = PIXEL_SIZE; i < STRING_LENGTH; i+=PIXEL_SIZE) {
        this = &strings[i];
        stringS *prev = &strings[i - PIXEL_SIZE];
        this->last_phi = (prev->phi)*dampening;
        this->last_y = (prev->y)*dampening;
        prev->phi = prev->last_phi;
        prev->y = prev->last_y;
    }
    this = &strings[STRING_LENGTH];
    stringS *prev = &strings[STRING_LENGTH - PIXEL_SIZE];
    this->phi = this->last_phi;
    this->y = this->last_y;
    this->last_phi = (prev->phi)*dampening;
    this->last_y = (prev->y)*dampening;
    prev->phi = prev->last_phi;
    prev->y = prev->last_y;
}

static void updateLens(void)
{
    int bumpRadius = 10;
    int start_x = rand()%(WIN_WIDTH - bumpRadius * 2);
    int start_y = rand()%(WIN_HEIGHT - bumpRadius * 2);

    for (int x = 0; x < 2*bumpRadius; x++) {
        for (int y = 0; y < 2*bumpRadius; y++) {
            bumpMap[x+start_x][y+start_y]+=10;
        }
    }

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

#define DISPLAY_Z 100
#define DISPLAY_X WIN_WIDTH/2
#define DISPLAY_Y WIN_HEIGHT/2

#define CUBE_SIDE 200
#define CUBE_CENTER 0
#define CUBE_CENTER_Z -300
struct coord corners[8] =
{
    //FRONT CORNERS
    {CUBE_CENTER - CUBE_SIDE/2, CUBE_CENTER - CUBE_SIDE/2, .z = CUBE_CENTER_Z - CUBE_SIDE/2},
    {CUBE_CENTER - CUBE_SIDE/2, CUBE_CENTER + CUBE_SIDE/2, .z = CUBE_CENTER_Z - CUBE_SIDE/2},
    {CUBE_CENTER + CUBE_SIDE/2, CUBE_CENTER + CUBE_SIDE/2, .z = CUBE_CENTER_Z - CUBE_SIDE/2},
    {CUBE_CENTER + CUBE_SIDE/2, CUBE_CENTER - CUBE_SIDE/2, .z = CUBE_CENTER_Z - CUBE_SIDE/2},

    //BACK CORNERS
    {CUBE_CENTER - CUBE_SIDE/2, CUBE_CENTER - CUBE_SIDE/2, .z = CUBE_CENTER_Z + CUBE_SIDE/2},
    {CUBE_CENTER - CUBE_SIDE/2, CUBE_CENTER + CUBE_SIDE/2, .z = CUBE_CENTER_Z + CUBE_SIDE/2},
    {CUBE_CENTER + CUBE_SIDE/2, CUBE_CENTER + CUBE_SIDE/2, .z = CUBE_CENTER_Z + CUBE_SIDE/2},
    {CUBE_CENTER + CUBE_SIDE/2, CUBE_CENTER - CUBE_SIDE/2, .z = CUBE_CENTER_Z + CUBE_SIDE/2},
};

static void faceSortClear(struct face **sortedFaces, int numberOfFaces) {
    memset(sortedFaces, 0, numberOfFaces * sizeof(struct face *));
}


/**
 * @brief Returns returns true if faceOne is closest to cam, otherwise positive
 *
 * @param faceOne
 * @param faceTwo
 *
 * @return 
 */
static bool compareFaces(struct face *faceOne, struct face *faceTwo)
{
    int coord = 0;
    double faceOneMin = faceOne->coords[coord]->z;
    double faceTwoMin = faceTwo->coords[coord]->z;
    for (; coord < 4; coord++){
        faceOneMin = fmin(faceOneMin, faceOne->coords[coord]->z);
        faceTwoMin = fmin(faceTwoMin, faceTwo->coords[coord]->z);
    }
    return (faceOneMin < faceTwoMin);


}

static void mat_mul(double Rout[3][3], double A[3][3], double B[3][3]) {

    // Rout = AB;
    for (int a = 0; a < 3; a++)
        for (int b = 0; b < 3; b++)
            Rout[a][b] = A[0][a]*B[b][0] +A[1][a]*B[b][1] +A[2][a]*B[b][2];
}

static void rotateCube(double phi_x, double phi_y, double phi_z) {
    double Ry[3][3] = 
    {
        {cos(phi_x), 0, sin(phi_x)},
        {0,1,0},
        {-sin(phi_x), 0, cos(phi_x)}
    };
    double Rx[3][3] = 
    {
        {1,0,0},
        {0,cos(phi_y),-sin(phi_y)},
        {0,sin(phi_y),cos(phi_y)}
    };
    double Rz[3][3] = 
    {
        {cos(phi_z),-sin(phi_z), 0},
        {sin(phi_z),cos(phi_z), 0},
        {0,0,1},
    };

    double Rinter[3][3];
    double Rtot[3][3];

    mat_mul(Rinter, Rx, Ry);
    mat_mul(Rtot, Rinter, Rz);


    for (int i = 0; i < 8; i++) {
        double x = corners[i].x;
        double y = corners[i].y;
        double z = corners[i].z - CUBE_CENTER_Z;
        corners[i].x = Rtot[0][0] * x + Rtot[0][1] * y + Rtot[0][2] * z;
        corners[i].y = Rtot[1][0] * x + Rtot[1][1] * y + Rtot[1][2] * z;
        corners[i].z =+ CUBE_CENTER_Z + Rtot[2][0] * x  + Rtot[2][1] * y  + Rtot[2][2] * z;
    }

}

static void faceSortInsert(struct face *thisFace, struct face **sortedFaces)
{
    struct face ** nextFaceSlot = sortedFaces;
    int count =0;

    while (*nextFaceSlot != NULL) {
       count++;
        if (compareFaces(*nextFaceSlot, thisFace)) {
            faceSortInsert(*nextFaceSlot,nextFaceSlot++);
            break;
        }
        nextFaceSlot++;
    }
    *nextFaceSlot = thisFace;
}

static void initCube()
{
    // init faces
    // FRONT
    cube.faces[0].coords[0] = &corners[0];
    cube.faces[0].coords[1] = &corners[1];
    cube.faces[0].coords[2] = &corners[2];
    cube.faces[0].coords[3] = &corners[3];

    //LEFT SIDE
    cube.faces[1].coords[0] = &corners[0];
    cube.faces[1].coords[1] = &corners[1];
    cube.faces[1].coords[2] = &corners[5];
    cube.faces[1].coords[3] = &corners[4];

    //UPSIDE
    cube.faces[2].coords[0] = &corners[1];
    cube.faces[2].coords[1] = &corners[2];
    cube.faces[2].coords[2] = &corners[6];
    cube.faces[2].coords[3] = &corners[5];

    //RIGTH SIDE
    cube.faces[3].coords[0] = &corners[2];
    cube.faces[3].coords[1] = &corners[3];
    cube.faces[3].coords[2] = &corners[7];
    cube.faces[3].coords[3] = &corners[6];

    //DOWN SIDE
    cube.faces[4].coords[0] = &corners[3];
    cube.faces[4].coords[1] = &corners[0];
    cube.faces[4].coords[2] = &corners[4];
    cube.faces[4].coords[3] = &corners[7];

    //BACK SIDE
    cube.faces[5].coords[0] = &corners[4];
    cube.faces[5].coords[1] = &corners[5];
    cube.faces[5].coords[2] = &corners[6];
    cube.faces[5].coords[3] = &corners[7];

    printf("about to clear\n");
    faceSortClear(cube.sortedFaces, 6);
    for (int i = 0; i < 6; i++) {
    printf("sort %d\n", i);
        faceSortInsert(&cube.faces[i], cube.sortedFaces);
    }
}

static void drawCube()
{
    double phi_x = 0.0001;
    double phi_y = 0.0005;
    double phi_z = 0.0003;
    rotateCube(phi_x, phi_y, phi_z);

    SDL_SetRenderDrawColor(myRenderer_p, 255, 255, 255, SDL_ALPHA_OPAQUE);

    // for each sorted face
    for (int face = 0; face < 6; face ++) {
        // for each coord draw line to next
        for (int coord = 0; coord < 3; coord++) {
            struct coord *start_corner = cube.sortedFaces[face]->coords[coord];
            struct coord *end_corner = cube.sortedFaces[face]->coords[coord + 1];
//            printf("Drawing %dx%d to %dx%d\n", (int)start_corner->x, (int)start_corner->y, (int)end_corner->x, (int)end_corner->y); 
//
            if( SDL_RenderDrawLine(myRenderer_p,
                    DISPLAY_Z*start_corner->x/start_corner->z + DISPLAY_X,
                    DISPLAY_Z*start_corner->y/start_corner->z + DISPLAY_Y,
                    DISPLAY_Z*end_corner->x/end_corner->z + DISPLAY_X,
                    DISPLAY_Z*end_corner->y/end_corner->z + DISPLAY_Y)) printf("Failed to draw line\n");
        }
    }
}

