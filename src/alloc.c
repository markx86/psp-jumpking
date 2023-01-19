#include "alloc.h"
#include <pspgu.h>

unsigned int getVramMemorySize(unsigned int width, unsigned int height, unsigned int psm) {
    switch (psm) {
        case GU_PSM_T4:
            return (width * height) >> 1;
        case GU_PSM_T8:
            return width * height;
        case GU_PSM_5650:
        case GU_PSM_5551:
        case GU_PSM_4444:
        case GU_PSM_T16:
            return (width * height) << 1;
        case GU_PSM_8888:
        case GU_PSM_T32:
            return (width * height) << 2;
        default:
            return 0;
    }
}
