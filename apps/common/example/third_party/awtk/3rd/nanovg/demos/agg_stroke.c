#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "agg/nanovg_agg.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../stb/stb_image_write.h"

#define STB_IMAGE_IMPLEMENTATION
#include "../stb/stb_image.h"

#include "stroke.inc"

static void run_test(int32_t w, int32_t h, int32_t BPP, const char *filename)
{
    int32_t size = w * h * BPP;
    uint8_t *data = (uint8_t *)malloc(size);
    NVGcontext *vg = nvgCreateAGG(w, h, w * BPP, BPP == 2 ? NVG_TEXTURE_BGR565 : NVG_TEXTURE_RGBA, data);

    memset(data, 0xff, size);
    do_stroke(vg, w, h);

    nvgDeleteAGG(vg);

    if (filename != NULL) {
        stbi_write_png(filename, w, h, BPP, data, 0);
    }

    free(data);
}

int main()
{
    run_test(400, 400, 4, "result32.png");
    run_test(400, 400, 2, "result16.png");

    return 0;
}
