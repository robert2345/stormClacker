/**
 * renderDestroy will free all the allocated resources of the renderer.
 */
void renderDestroy(void);

/* render() will draw all the objects onto the canvas and flip the screen.
 */
void render(char* input, int score);

/* renderInit() will initialize the renderer.
 */
int renderInit(int gridSize);
