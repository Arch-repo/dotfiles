#include "backend.h"
#include "service.h"

#include <stdio.h>
#include <string.h>

static char *screenshot_directory(void) {
    const char *override = g_getenv("ANTO_MENU_SCREENSHOT_DIR");
    if (override && *override) return g_strdup(override);
    const char *pictures = g_get_user_special_dir(G_USER_DIRECTORY_PICTURES);
    g_autofree char *fallback = NULL;
    if (!pictures || !*pictures) {
        fallback = g_build_filename(g_get_home_dir(), "Pictures", NULL);
        pictures = fallback;
    }
    return g_build_filename(pictures, "Screenshots", NULL);
}

static int ensure_directory(const char *path) {
    if (g_mkdir_with_parents(path, 0700) == 0) return 0;
    return backend_error(1, "mkdir",
                         "Impossibile creare la cartella Screenshot");
}

static int hyprshot(const char *mode, gboolean active) {
    g_autofree char *target = screenshot_directory();
    if (backend_dry_run()) {
        g_print("DRYRUN\tcapture\t%s\t%s\n", mode, target);
        return 0;
    }
    if (ensure_directory(target) != 0) return 1;
    const char *program =
        backend_program("ANTO_MENU_HYPRSHOT", "hyprshot");
    if (active) {
        const char *argv[] = {
            program, "-m", mode, "-m", "active", "-o", target, NULL,
        };
        return backend_command_forward(argv, NULL);
    }
    const char *argv[] = {
        program, "-m", mode, "-o", target, NULL,
    };
    return backend_command_forward(argv, NULL);
}

static char *select_geometry(void) {
    const char *program = backend_program("ANTO_MENU_SLURP", "slurp");
    const char *argv[] = {program, NULL};
    BackendCommand result = backend_command_run(argv, NULL);
    char *geometry =
        result.status == 0 ? g_strdup(result.stdout_text) : g_strdup("");
    g_strstrip(geometry);
    backend_command_clear(&result);
    return geometry;
}

static BackendCommand grim_area(const char *geometry, GBytes **image) {
    const char *program = backend_program("ANTO_MENU_GRIM", "grim");
    const char *argv[] = {program, "-g", geometry, "-", NULL};
    return backend_command_run_bytes(argv, NULL, image);
}

static int copy_bytes(GBytes *bytes) {
    const char *program = backend_program("ANTO_MENU_WL_COPY", "wl-copy");
    const char *argv[] = {program, NULL};
    BackendCommand result =
        backend_command_run_bytes(argv, bytes, NULL);
    if (result.status != 0 && result.stderr_text)
        fputs(result.stderr_text, stderr);
    int status = result.status;
    backend_command_clear(&result);
    return status;
}

static int clipboard_area(void) {
    if (backend_dry_run()) {
        puts("DRYRUN\tcapture\tclipboard-area\t-");
        return 0;
    }
    g_autofree char *geometry = select_geometry();
    if (!*geometry) return 0;
    g_autoptr(GBytes) image = NULL;
    BackendCommand grim = grim_area(geometry, &image);
    if (grim.status != 0) {
        if (grim.stderr_text) fputs(grim.stderr_text, stderr);
        backend_command_clear(&grim);
        return 1;
    }
    backend_command_clear(&grim);
    int status = copy_bytes(image);
    if (status == 0)
        backend_notify("Screenshot", "Area copiata negli appunti");
    return status;
}

static int ocr_area(void) {
    if (backend_dry_run()) {
        puts("DRYRUN\tcapture\tocr\t-");
        return 0;
    }
    g_autofree char *geometry = select_geometry();
    if (!*geometry) return 0;
    g_autoptr(GBytes) image = NULL;
    BackendCommand grim = grim_area(geometry, &image);
    if (grim.status != 0) {
        if (grim.stderr_text) fputs(grim.stderr_text, stderr);
        backend_command_clear(&grim);
        return 1;
    }
    backend_command_clear(&grim);
    const char *tesseract =
        backend_program("ANTO_MENU_TESSERACT", "tesseract");
    g_autofree char *data = g_build_filename(g_get_user_data_dir(), "anto-desktop", "tessdata", NULL);
    gboolean local_data = g_file_test(data, G_FILE_TEST_IS_DIR);
    const char *argv[] = {
        tesseract, "stdin", "stdout", "-l", "ita+eng",
        local_data ? "--tessdata-dir" : NULL, local_data ? data : NULL, NULL,
    };
    g_autoptr(GBytes) text = NULL;
    BackendCommand recognition =
        backend_command_run_bytes(argv, image, &text);
    if (recognition.status != 0) {
        if (recognition.stderr_text)
            fputs(recognition.stderr_text, stderr);
        backend_command_clear(&recognition);
        return 1;
    }
    backend_command_clear(&recognition);
    int status = copy_bytes(text);
    if (status == 0)
        backend_notify("OCR", "Testo riconosciuto e copiato");
    return status;
}

static int open_screenshots(void) {
    g_autofree char *target = screenshot_directory();
    if (backend_dry_run()) {
        g_print("DRYRUN\tcapture\topen\t%s\n", target);
        return 0;
    }
    if (ensure_directory(target) != 0) return 1;
    const char *program =
        backend_program("ANTO_MENU_XDG_OPEN", "xdg-open");
    const char *argv[] = {program, target, NULL};
    return backend_command_forward(argv, NULL);
}

int anto_capture_execute(int argc, char **argv) {
    (void)argc;

    extern const BackendService anto_service_capture;
    switch (backend_operation_index(&anto_service_capture, argv[0])) {
        case 0: return hyprshot("region", FALSE);
        case 1: return hyprshot("window", FALSE);
        case 2: return hyprshot("output", TRUE);
        case 3: return clipboard_area();
        case 4: return ocr_area();
        case 5: return open_screenshots();
        default: return 2;
    }
}
