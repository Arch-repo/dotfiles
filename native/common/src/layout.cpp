#include "layout.h"
#include "design_tokens.h"
#include <algorithm>
AntoLayout anto_menu_layout(int width, int height) {
    const int availableWidth = std::max(240, width - ANTO_SIZE_MONITOR_MARGIN_X);
    const int availableHeight = std::max(240, height - ANTO_SIZE_MONITOR_MARGIN_Y);
    const int panelWidth = std::min(ANTO_SIZE_MENU_WIDTH, availableWidth);
    return {panelWidth, std::min(ANTO_SIZE_MENU_HEIGHT, availableHeight), panelWidth >= ANTO_SIZE_CONTEXT_BREAKPOINT};
}
