#pragma once
#ifdef __cplusplus
extern "C" {
#endif
typedef struct { int width; int height; int context_visible; } AntoLayout;
AntoLayout anto_menu_layout(int monitor_width, int monitor_height);
#ifdef __cplusplus
}
#endif
