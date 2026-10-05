#include "layout.h"
#include "design_tokens.h"
#include <cstdio>
#include <initializer_list>
int main() {
    for (int width : {480, 640, 800, 1024, 1600, 2560}) {
        for (int height : {400, 600, 720, 1000, 1440}) {
            auto layout = anto_menu_layout(width, height);
            if (layout.width > width - 40 || layout.height > height - 56 || layout.width > ANTO_SIZE_MENU_WIDTH || layout.height > ANTO_SIZE_MENU_HEIGHT ||
                (width < ANTO_SIZE_CONTEXT_BREAKPOINT && layout.context_visible)) {
                std::fprintf(stderr, "Invalid layout for %dx%d\n", width, height); return 1;
            }
        }
    }
    return 0;
}
