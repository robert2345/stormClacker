#include <SDL.h>
#include <stdio.h>
#include <stdbool.h>
#include <render.h>

#define VALUE_FOR_MISS -1;
#define VALUE_FOR_HIT 2;
#define GRID_SIZE 4
#define START_CHAR 33
#define END_CHAR 126
#define INVALID_CHAR ' '
#define INVALID_POS (-1)
#define NUMBER_OF_CHARS (END_CHAR - START_CHAR)
static int score = 0;
SDL_mutex* myMutex_p;
static int charPlacementTable[END_CHAR - START_CHAR][2];
static char grid[GRID_SIZE][GRID_SIZE] = {0};

static int shoot(char inputChar);
static void gameInputKey(SDL_KeyboardEvent* key_p);
static Uint32 placeChar(Uint32 interval, void *param);

static char applyShift(char input);
static bool getEmptyPos(int* x_p, int* y_p, char inputChar);

int main(void)
{
  // Initialize the placement of the digits to invalid.
  for (int i = 0; i < (END_CHAR - START_CHAR); i++) 
  {
    charPlacementTable[i][0] = INVALID_POS;
    charPlacementTable[i][1] = INVALID_POS;
  }

  // Initialize the content of the grid  
  for (int i = 0; i < (GRID_SIZE * GRID_SIZE); i++) 
  {
    grid[i / GRID_SIZE][i % GRID_SIZE] = INVALID_CHAR;
  }

  
  if (SDL_Init(SDL_INIT_EVERYTHING) != 0)
  {
    printf("Could not initialize SDL Video");
  }
  else {
    printf("SDL Initialized\n");
  }

  renderInit(GRID_SIZE);
  bool escaped = false;
  int intensity = 1;
  int error = 0;
  myMutex_p = SDL_CreateMutex();
  const int newTargetTimeoutMs = 1000;
  SDL_TimerID my_timer_id = SDL_AddTimer(newTargetTimeoutMs, placeChar, 0);
  while (escaped != true)
  {
    SDL_Event event;
    const int timeout = 1 / 60;
    // Check if we are going to abort.
    while (SDL_PollEvent(&event))
    {
      if (event.type == SDL_KEYDOWN)
      {
        if (event.key.keysym.sym == SDLK_ESCAPE) escaped = true;
        else
        {
          gameInputKey(&event.key);
        }
      }
    }
    // Render the view to update the playing field and score.
    render(&grid[0][0], score);
  }
  printf("Seems like it's ok. Time to quit.\n");

  renderDestroy();
  SDL_Quit();
  return 0;
}

static void gameInputKey(SDL_KeyboardEvent* key_p)
{
  int symbol = key_p->keysym.sym;
  // check if this is a character and that it can be represented by ascii.
  if ((symbol | 0x40000000) != 0 && symbol < 0x80)
  {
    // Check if shift is pressed. I so, modify entry.
    if (key_p->keysym.mod & KMOD_SHIFT)
    {
      symbol = applyShift(symbol);
    } 

    // Check if correct symbol and modify score.
    score += shoot(symbol);
  }
}

static char applyShift(char input)
{
  switch(input)
  {
    case ' ':
      return ' ';
    case '1':
      return '!';
    case '2':
      return '@';
    case '3':
      return '#';
    case '4':
      return '$';
    case '5':
      return '%';
    case '6':
      return '^';
    case '7':
      return '&';
    case '8':
      return '*';
    case '9':
      return '(';
    case '0':
      return ')';
    case '-':
      return '_';
    case '=':
      return '+';
    case '[':
      return '{';
    case ']':
      return '}';
    case '\\':
      return '|';
    case ';':
      return ':';
    case '\'':
      return '\"';
    case ',':
      return '<';
    case '.':
      return '>';
    case '/':
      return '?';
    default:
      return input + ('A' - 'a');
  }
}

/*
 * shoot() will check if the inputChar is drawn onto the screen. If so it will erase it and give 
 * points. if not, negative points is returned.
 */
static int shoot(char inputChar)
{
  if (inputChar < START_CHAR || inputChar > END_CHAR) return 0; // Check if this char is in the range we are playing with.
  int charIndex = inputChar - START_CHAR;
    
  // The tables are accessed from event and timer interrupt, so a mutex is used to protect the data
  SDL_LockMutex(myMutex_p);
  if (charPlacementTable[charIndex][0] != INVALID_POS)
  {
    int x = charPlacementTable[charIndex][0];
    int y = charPlacementTable[charIndex][1];
    charPlacementTable[charIndex][0] = INVALID_POS;
    charPlacementTable[charIndex][1] = INVALID_POS;
    grid[x][y] = INVALID_CHAR;
    SDL_UnlockMutex(myMutex_p);
    return VALUE_FOR_HIT;
  }

  SDL_UnlockMutex(myMutex_p);
  return VALUE_FOR_MISS;
}

/*
 * placeChar() will pick a random character that is not already on the playing field and then try
 * to put that in the grid. If it fails it will cause a game ending event. Otherwise a new timer 
 * interval will be set up to call back the function again.
 */
uint32_t placeChar(uint32_t interval, void *param)
{
  int randomNumber = rand();

  // The tables are accessed from event and timer interrupt, so a mutex is used to protect the data
  SDL_LockMutex(myMutex_p);

  // Loop through all positions starting with the random one.
  for (int i = 0; i < NUMBER_OF_CHARS; i++)
  {
    int charToPlace = (randomNumber + i)% NUMBER_OF_CHARS;
    if (charPlacementTable[charToPlace][0] == INVALID_POS)
    {
      int x, y;
      // Check if the playing field has an ampty position.
      // If so, claim it.
      if (getEmptyPos(&x, &y, (char)(charToPlace + START_CHAR)))
      {
        charPlacementTable[charToPlace][0] = x;
        charPlacementTable[charToPlace][1] = y;
        break;
      }
      else // No place to place char. You have lost. not implemented yet though.
      {
        break;
      } 
    }
  }
  SDL_UnlockMutex(myMutex_p);

  SDL_Event event;
  SDL_UserEvent userevent;

  userevent.type = SDL_USEREVENT;
  userevent.code = 0;
  userevent.data1 = &placeChar;
  userevent.data2 = param;

  event.type = SDL_USEREVENT;
  event.user = userevent;

  SDL_PushEvent(&event);
  return(interval);
}

/*
 * getEmptyPos() finds an empty position in the "grid" and inserts the inputChar. It uses the
 * return pointers to deliver the coordinates of the empty position found, and if so TRUE in 
 * the return value. If no empty position is found the function returns FALSE.
 */
static bool getEmptyPos(int* x_p, int* y_p, char inputChar)
{
  for (int x = 0; x < GRID_SIZE; x++)
  {
    for (int y = 0; y < GRID_SIZE; y++)
    {
      if (grid[x][y] == INVALID_CHAR)
      {
        grid[x][y] = inputChar;
        *x_p = x;
        *y_p = y;
        return true;
      }
    }
  }
  return false;
}

