#include "../src/modules/bluetooth/internal.h"
#include "../src/modules/bluetooth_agent/internal.h"

#include <glib/gstdio.h>

/* A GTK window with the same page/search ownership contract as the deck.
 * No desktop actions or real Bluetooth objects are used by this fixture. */
static guint notifications;
static char *backend;
static char *state_file;

char *menu_backend_path(void) { return g_strdup(backend); }

gboolean menu_run_with_input(const char *const argv[], const char *input,
                             char **output, char **error_text) {
    g_autoptr(GSubprocess) process = g_subprocess_newv(argv,
        G_SUBPROCESS_FLAGS_STDIN_PIPE | G_SUBPROCESS_FLAGS_STDOUT_PIPE |
        G_SUBPROCESS_FLAGS_STDERR_PIPE, NULL);
    return process && g_subprocess_communicate_utf8(process, input, NULL,
        output, error_text, NULL) && g_subprocess_get_successful(process);
}

void menu_notify(const char *title, const char *body) {
    (void)title;
    (void)body;
    notifications++;
}

void menu_spawn(MenuApp *app, const char *const argv[], gboolean close_after) {
    (void)app;
    (void)argv;
    (void)close_after;
    g_assert_not_reached();
}

void menu_set_footer(MenuApp *app, const char *text) {
    gtk_label_set_text(GTK_LABEL(app->footer), text);
}

void menu_set_search_action(MenuApp *app, MenuSearchAction action,
                            gpointer data, GDestroyNotify destroy) {
    app->search_action = NULL;
    app->search_data = NULL;
    g_object_set_data_full(G_OBJECT(app->search), "search-data", NULL, NULL);
    app->search_action = action;
    app->search_data = data;
    g_object_set_data_full(G_OBJECT(app->search), "search-data", data, destroy);
}

void menu_page_begin(MenuApp *app, const char *icon, const char *title,
                     const char *subtitle, const char *placeholder) {
    (void)icon;
    (void)title;
    (void)placeholder;
    menu_set_search_action(app, NULL, NULL, NULL);
    GtkWidget *child;
    while ((child = gtk_widget_get_first_child(app->custom_holder)))
        gtk_box_remove(GTK_BOX(app->custom_holder), child);
    gtk_label_set_text(GTK_LABEL(app->page_subtitle), subtitle);
    gtk_editable_set_text(GTK_EDITABLE(app->search), "");
    gtk_widget_set_visible(app->search, TRUE);
}

void menu_set_custom_content(MenuApp *app, GtkWidget *widget) {
    gtk_box_append(GTK_BOX(app->custom_holder), widget);
}

static void settle(guint milliseconds) {
    gint64 until = g_get_monotonic_time() + milliseconds * 1000;
    while (g_get_monotonic_time() < until) {
        while (g_main_context_iteration(NULL, FALSE));
        g_usleep(1000);
    }
}

static void wait_snapshot(MenuApp *app) {
    BluetoothAsyncState *state = anto_bluetooth_async_state_get(app);
    gint64 until = g_get_monotonic_time() + 3000000;
    while (state->in_flight && g_get_monotonic_time() < until) settle(5);
    g_assert_false(state->in_flight);
    settle(150);
}

static void wait_action(GtkWidget *widget) {
    gint64 until = g_get_monotonic_time() + 3000000;
    while (g_object_get_data(G_OBJECT(widget), "bluetooth-operation-pending") &&
           g_get_monotonic_time() < until) settle(5);
    g_assert_null(g_object_get_data(G_OBJECT(widget), "bluetooth-operation-pending"));
}

static gboolean contains_label(GtkWidget *widget, const char *wanted) {
    if (GTK_IS_LABEL(widget) && g_strcmp0(gtk_label_get_text(GTK_LABEL(widget)), wanted) == 0)
        return TRUE;
    for (GtkWidget *child = gtk_widget_get_first_child(widget); child;
         child = gtk_widget_get_next_sibling(child))
        if (contains_label(child, wanted)) return TRUE;
    return FALSE;
}

static GtkWidget *button_with_label(GtkWidget *root, const char *wanted) {
    if (GTK_IS_BUTTON(root)) {
        if (g_strcmp0(gtk_button_get_label(GTK_BUTTON(root)), wanted) == 0) return root;
        if (contains_label(root, wanted)) return root;
        GtkWidget *label = g_object_get_data(G_OBJECT(root), "bluetooth-action-label");
        if (label && g_strcmp0(gtk_label_get_text(GTK_LABEL(label)), wanted) == 0) return root;
    }
    for (GtkWidget *child = gtk_widget_get_first_child(root); child;
         child = gtk_widget_get_next_sibling(child)) {
        GtkWidget *match = button_with_label(child, wanted);
        if (match) return match;
    }
    return NULL;
}

typedef struct { gboolean done; GVariant *reply; GError *error; } AgentReply;

static void agent_reply_finished(GObject *object, GAsyncResult *result, gpointer data) {
    AgentReply *reply = data;
    reply->reply = g_dbus_connection_call_finish(G_DBUS_CONNECTION(object), result, &reply->error);
    reply->done = TRUE;
}

static void request_agent(BluetoothAgent *agent, const char *method,
                           GVariant *parameters, AgentReply *reply) {
    g_dbus_connection_call(agent->bus, "org.bluez", "/test",
        "com.anto426.BluetoothTest", "Request", g_variant_new("(sv)", method, parameters),
        NULL, G_DBUS_CALL_FLAGS_NONE, 5000, NULL, agent_reply_finished, reply);
    gint64 until = g_get_monotonic_time() + 3000000;
    while (!agent->prompt && !reply->done && g_get_monotonic_time() < until) settle(5);
    g_assert_nonnull(agent->prompt);
}

static void wait_agent_reply(AgentReply *reply) {
    gint64 until = g_get_monotonic_time() + 3000000;
    while (!reply->done && g_get_monotonic_time() < until) settle(5);
    g_assert_true(reply->done);
}

int main(int argc, char **argv) {
    g_assert_cmpint(argc, ==, 2);
    backend = g_canonicalize_filename(argv[1], NULL);
    state_file = g_build_filename(g_get_user_runtime_dir(), "ui-snapshot.tsv", NULL);
    g_setenv("ANTO_BT_UI_SNAPSHOT", state_file, TRUE);
    const char *snapshot =
        "STATUS\tyes\tAA:BB:CC:DD:EE:02\tRadio Test\tyes\tno\tno\tno\t1\t0\t1\n"
        "CONTROLLER\tAA:BB:CC:DD:EE:02\tRadio Test\tRadio Test\tyes\tno\tno\tno\tyes\n"
        "DEVICE\t11:22:33:44:55:66\tHeadphones\tCuffie\taudio-headset\tyes\tyes\tno\tno\t85\t-42\n";
    g_assert_true(g_file_set_contents(state_file, snapshot, -1, NULL));
    gtk_init();
    MenuApp app = {0};
    app.window = GTK_WINDOW(gtk_window_new());
    g_object_ref_sink(app.window);
    app.panel = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_window_set_child(app.window, app.panel);
    gtk_window_set_default_size(app.window, 640, 360);
    app.page_subtitle = gtk_label_new("");
    app.search = gtk_search_entry_new();
    app.custom_holder = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_vexpand(app.custom_holder, TRUE);
    app.footer = gtk_label_new("");
    gtk_box_append(GTK_BOX(app.panel), app.page_subtitle);
    gtk_box_append(GTK_BOX(app.panel), app.search);
    gtk_box_append(GTK_BOX(app.panel), app.custom_holder);
    gtk_box_append(GTK_BOX(app.panel), app.footer);
    app.current_page = g_strdup("bluetooth");
    menu_show_bluetooth(&app);
    gtk_window_present(app.window);
    wait_snapshot(&app);
    BluetoothView *view = app.search_data;
    g_assert_cmpuint(view->cards->len, ==, 1);
    g_assert_true(gtk_widget_get_visible(app.search));
    GtkAdjustment *adjustment = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(view->scroll));
    g_assert_cmpfloat(gtk_adjustment_get_value(adjustment), ==, 0);
    gtk_editable_set_text(GTK_EDITABLE(app.search), "nessun risultato");
    anto_bluetooth_search(&app, "nessun risultato", view);
    g_assert_true(gtk_widget_get_visible(view->search_empty));
    gtk_editable_set_text(GTK_EDITABLE(app.search), "Cuffie");
    anto_bluetooth_search(&app, "Cuffie", view);
    BluetoothCardRef *card = g_ptr_array_index(view->cards, 0);
    g_assert_true(gtk_widget_get_visible(card->row));
    GtkWidget *refresh = button_with_label(view->controller_panel, "Aggiorna");
    g_assert_nonnull(refresh);
    for (guint i = 0; i < 2; i++) {
        g_signal_emit_by_name(refresh, "clicked");
        g_assert_false(gtk_widget_is_sensitive(refresh));
        wait_action(refresh);
        wait_snapshot(&app);
        g_assert_true(gtk_widget_is_sensitive(refresh));
        g_assert_cmpstr(gtk_editable_get_text(GTK_EDITABLE(app.search)), ==, "Cuffie");
    }
    g_signal_emit_by_name(card->primary_action, "clicked");
    g_assert_false(gtk_widget_get_sensitive(card->actions));
    menu_bluetooth_live_event(&app);
    settle(50);
    g_assert_false(gtk_widget_get_sensitive(card->actions));
    wait_action(card->primary_action);
    wait_snapshot(&app);
    g_assert_true(gtk_widget_get_sensitive(card->actions));
    BluetoothDevice connected_device = {.address = "11:22:33:44:55:66", .connected = TRUE, .paired = TRUE};
    BluetoothAudioProfile sbc = {.profile = "a2dp-sink-sbc", .available = TRUE};
    BluetoothAudioProfile active_codec = {.profile = "a2dp-sink-aac", .available = TRUE, .active = TRUE};
    BluetoothAudioProfile microphone = {.profile = "headset-head-unit", .available = TRUE};
    g_autoptr(GPtrArray) profiles = g_ptr_array_new();
    g_ptr_array_add(profiles, &sbc);
    g_ptr_array_add(profiles, &active_codec);
    g_ptr_array_add(profiles, &microphone);
    anto_bluetooth_device_actions_update(card, &connected_device, profiles);
    BluetoothAction *hifi = g_object_get_data(G_OBJECT(card->profile_actions[0]), "bluetooth-action");
    BluetoothAction *voice = g_object_get_data(G_OBJECT(card->profile_actions[1]), "bluetooth-action");
    g_assert_cmpstr(hifi->value, ==, "a2dp-sink-aac");
    g_assert_cmpstr(voice->value, ==, "headset-head-unit");
    g_assert_false(gtk_widget_get_sensitive(card->profile_actions[0]));
    g_assert_true(gtk_widget_get_sensitive(card->profile_actions[1]));
    g_autofree char *failure = g_strconcat(state_file, ".fail-action", NULL);
    g_file_set_contents(failure, "", -1, NULL);
    gtk_switch_set_active(GTK_SWITCH(view->pairable_switch), TRUE);
    wait_action(view->pairable_switch);
    wait_snapshot(&app);
    g_assert_false(gtk_switch_get_active(GTK_SWITCH(view->pairable_switch)));
    g_assert_true(gtk_widget_get_sensitive(view->pairable_switch));
    g_assert_cmpuint(notifications, ==, 1);
    g_unlink(failure);
    g_autofree char *snapshot_failure = g_strconcat(state_file, ".fail", NULL);
    g_file_set_contents(snapshot_failure, "", -1, NULL);
    menu_bluetooth_live_event(&app);
    wait_snapshot(&app);
    g_assert_nonnull(strstr(gtk_label_get_text(GTK_LABEL(app.footer)), "temporaneamente"));
    g_assert_true(gtk_widget_get_visible(card->row));
    g_unlink(snapshot_failure);
    menu_bluetooth_live_event(&app);
    wait_snapshot(&app);

    /* BlueZ calls the exported agent on the private system bus. Exercise
     * human confirmation, typed PIN/passkey, display and cancellation. */
    BluetoothAgent *agent = g_object_get_data(G_OBJECT(app.window), "anto-bluetooth-agent");
    gint64 until = g_get_monotonic_time() + 3000000;
    while (!agent->ready && g_get_monotonic_time() < until) settle(5);
    g_assert_true(agent->ready);
    const char *device = "/org/bluez/hci1/dev_11_22_33_44_55_66";
    AgentReply confirmation = {0};
    request_agent(agent, "RequestConfirmation", g_variant_new("(ou)", device, 123), &confirmation);
    g_assert_false(confirmation.done);
    GtkWidget *confirm = button_with_label(agent->prompt, "Conferma");
    g_assert_nonnull(confirm);
    g_signal_emit_by_name(confirm, "clicked");
    wait_agent_reply(&confirmation);
    g_assert_no_error(confirmation.error);
    g_clear_pointer(&confirmation.reply, g_variant_unref);
    for (guint passkey = 0; passkey < 2; passkey++) {
        AgentReply answer = {0};
        request_agent(agent, passkey ? "RequestPasskey" : "RequestPinCode",
                       g_variant_new("(o)", device), &answer);
        gtk_editable_set_text(GTK_EDITABLE(agent->entry), "");
        g_signal_emit_by_name(button_with_label(agent->prompt, "Associa"), "clicked");
        g_assert_false(answer.done);
        gtk_editable_set_text(GTK_EDITABLE(agent->entry), passkey ? "000123" : "AB12");
        g_signal_emit_by_name(button_with_label(agent->prompt, "Associa"), "clicked");
        wait_agent_reply(&answer);
        g_assert_no_error(answer.error);
        g_autoptr(GVariant) value = g_variant_get_child_value(answer.reply, 0);
        g_autoptr(GVariant) unboxed = g_variant_get_variant(value);
        if (passkey) {
            guint32 number;
            g_variant_get(unboxed, "(u)", &number);
            g_assert_cmpuint(number, ==, 123);
        } else {
            const char *pin;
            g_variant_get(unboxed, "(&s)", &pin);
            g_assert_cmpstr(pin, ==, "AB12");
        }
        g_clear_pointer(&answer.reply, g_variant_unref);
    }
    AgentReply displayed = {0};
    request_agent(agent, "DisplayPasskey", g_variant_new("(ouq)", device, 123, 2), &displayed);
    wait_agent_reply(&displayed);
    g_assert_no_error(displayed.error);
    g_clear_pointer(&displayed.reply, g_variant_unref);
    menu_bluetooth_agent_dismiss(&app);
    AgentReply rejected = {0};
    request_agent(agent, "RequestAuthorization", g_variant_new("(o)", device), &rejected);
    g_signal_emit_by_name(button_with_label(agent->prompt, "Annulla"), "clicked");
    wait_agent_reply(&rejected);
    g_assert_nonnull(rejected.error);
    g_clear_error(&rejected.error);
    g_assert_null(agent->prompt);
    anto_bluetooth_agent_agent_window_closed(app.window, agent);
    menu_set_search_action(&app, NULL, NULL, NULL);
    gtk_window_destroy(app.window);
    g_object_unref(app.window);
    settle(50);
    g_free(app.current_page);
    g_free(backend);
    g_free(state_file);
    g_print("bluetooth GTK fixture: ok (ricerca, scroll, aggiornamento, errori e agente)\n");
    return 0;
}
