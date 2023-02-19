#include <Windows.h>

typedef struct {
    LONG left;
    LONG top;
} LARGESTNEGATIVECOORDINATES;

extern void enumerate_displays();

extern LARGESTNEGATIVECOORDINATES get_largest_negative_coordinates();
