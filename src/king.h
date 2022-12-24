#ifndef __KING_H__
#define __KING_H__

#include "level.h"

void kingCreate(void);
void kingUpdate(float delta, LevelScreen *screen, short *outSX, short *outSY);
void kingRender(void);
void kingDestroy(void);


#endif
