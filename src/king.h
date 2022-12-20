#ifndef __KING_H__
#define __KING_H__

typedef struct {
    void (*create)(void);
    void (*update)(float delta, int *currentScreen);
    void (*render)(void);
    void (*destroy)(void);
} King;

extern const King king;

#endif
