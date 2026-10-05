#include "internal.h"

void menu_show_capture(MenuApp *app) {
    menu_page_begin(app, "camera-photo-symbolic", "Cattura",
                    "Screenshot, appunti, OCR e registrazione",
                    "Cerca un tipo di cattura…");
    menu_set_layout(app, MENU_LAYOUT_GRID);
    menu_set_grid_columns(app, 3);
    menu_add_backend_tile(
        app, "camera-photo-symbolic", "Seleziona area",
        "Salva un ritaglio nella cartella Screenshot", "PRINT",
        "capture", "area", NULL, NULL, TRUE);
    menu_add_backend_tile(
        app, "focus-windows-symbolic", "Finestra attiva",
        "Cattura una singola finestra", "SHIFT PRINT",
        "capture", "window", NULL, NULL, TRUE);
    menu_add_backend_tile(
        app, "video-display-symbolic", "Monitor attivo",
        "Cattura l’intero schermo corrente", "CTRL PRINT",
        "capture", "monitor", NULL, NULL, TRUE);
    menu_add_backend_tile(
        app, "edit-copy-symbolic", "Area negli appunti",
        "Non crea file: copia direttamente l’immagine", NULL,
        "capture", "clipboard-area", NULL, NULL, TRUE);
    menu_add_backend_tile(
        app, "insert-text-symbolic", "OCR area",
        "Riconosce testo italiano/inglese e lo copia", NULL,
        "capture", "ocr", NULL, NULL, TRUE);
    menu_add_nav_tile(app, "media-record-symbolic", "Registrazione schermo",
                      "Area, finestra, monitor e audio", NULL, "record");
    menu_add_backend_tile(
        app, "folder-pictures-symbolic", "Apri Screenshot",
        "Mostra tutte le catture salvate", NULL,
        "capture", "open", NULL, NULL, TRUE);
    menu_set_footer(app, "←↑↓→ naviga  ·  i badge ricordano le scorciatoie");
}
