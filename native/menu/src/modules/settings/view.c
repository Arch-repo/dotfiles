#include "internal.h"
#include "primitives.h"

void anto_settings_add_preference(MenuApp *app, const char *title, const char *subtitle, const char *key, gboolean fallback, json_object *preferences) {
    json_object *setting = NULL;
    gboolean enabled = json_object_object_get_ex(preferences, key, &setting) && json_object_is_type(setting, json_type_boolean)
        ? json_object_get_boolean(setting) : fallback;
    GtkWidget *control = NULL;
    GtkWidget *row = anto_ui_toggle_row(title, subtitle, enabled, &control);
    g_autofree char *search = g_strdup_printf("%s %s", title, subtitle);
    g_object_set_data_full(G_OBJECT(row), "menu-search", g_utf8_strdown(search, -1), g_free);
    Preference *value = g_new0(Preference, 1);value->app = app;value->key = g_strdup(key);
    g_object_set_data_full(G_OBJECT(control), "preference", value, anto_settings_preference_free);
    g_signal_connect(control, "state-set", G_CALLBACK(anto_settings_toggle), value);
    menu_append_widget(app, row);
}

void menu_show_settings(MenuApp *app) {
    menu_page_begin(app, "emblem-system-symbolic", "Impostazioni", "Preferenze del desktop e colori ricavati dagli sfondi", "Cerca un’impostazione…");
    json_object *preferences = anto_settings_preferences();
    menu_add_section(app, "Palette · applicata al prossimo cambio sfondo");
    anto_settings_add_preference(app, "Applicazioni", "GTK, Qt, Kvantum e browser", "apps", TRUE, preferences);
    anto_settings_add_preference(app, "Editor", "Tema di Visual Studio Code", "vscode", TRUE, preferences);
    anto_settings_add_preference(app, "Obsidian", "Tema Monet e colori della raccolta di note", "obsidian", TRUE, preferences);
    anto_settings_add_preference(app, "Icone", "Colori della cartella e tema icone", "icons", TRUE, preferences);
    anto_settings_add_preference(app, "GRUB e login", "Aggiorna automaticamente sfondo e colori a ogni cambio sfondo", "boot", TRUE, preferences);
    json_object_put(preferences);
    menu_add_section(app, "Desktop");
    menu_add_nav(app, "preferences-desktop-wallpaper-symbolic", "Sfondi", "Raccolta e anteprima · applica a tutti gli schermi", NULL, "wallpaper");
    menu_add_nav(app, "view-grid-symbolic", "Widget", "Abilitazione, avvio automatico e disposizione", NULL, "widgets");
    menu_add_nav(app, "video-display-symbolic", "Schermi", "Profili, mirroring e schermi virtuali", NULL, "display");
    menu_add_nav(app, "input-keyboard-symbolic", "Tastiera", "Dispositivi, layout e scorciatoie", NULL, "keyboard");
    menu_add_nav(app, "office-calendar-symbolic", "Calendario", "Eventi e sincronizzazione", NULL, "calendar");
    menu_add_section(app, "Configurazione personale");
    menu_add_shell_item(app, "accessories-text-editor-symbolic", "Editor delle note", "Apri le preferenze del comando delle note", NULL, "\"$HOME/.local/libexec/anto-menu/anto-menu-backend\" notes init && xdg-open \"$HOME/.config/anto426-local/notes/notes.env\"", TRUE);
    menu_add_shell_item(app, "folder-symbolic", "Cartella delle impostazioni", "Preferenze salvate su questa macchina", NULL, "xdg-open \"$HOME/.config/anto426-local\"", TRUE);
    menu_set_footer(app, "Preferenze salvate automaticamente · Esc chiude · Ctrl+Backspace pagina precedente");
}
