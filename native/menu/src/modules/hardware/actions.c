#include "internal.h"

void anto_hardware_profile_action_free(gpointer data) {
    HardwareProfileAction *action = data;
    if (!action) return;
    g_free(action->profile);
    g_free(action);
}
