#ifndef SCORE_H
#define SCORE_H

#define MAX_NBR_NAME_CHARS 3

typedef struct scoreS
{
  char name[MAX_NBR_NAME_CHARS + 1];
  int score;
} scoreS;

#endif
