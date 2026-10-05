#include "internal.h"
char *anto_widgets_status(void) {
  return menu_capture("$HOME/.config/anto426/widgets.sh status 2>/dev/null");
}
