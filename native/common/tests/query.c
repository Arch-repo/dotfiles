#include "query.h"
#include <glib/gstdio.h>

typedef struct { int calls, changes, errors; } Results;
typedef struct { AntoQuery *query; Results result; } Reentrant;
static void received(const char *output, const GError *error, gboolean changed, gpointer data) {
    Results *results = data;
    results->calls++; results->changes += changed; results->errors += error != NULL;
    if (!error) g_assert_cmpstr(output, ==, "valid");
}
static void received_and_refresh(const char *output, const GError *error, gboolean changed, gpointer data) {
    Reentrant *client = data;
    received(output, error, changed, &client->result);
    if (client->result.calls == 1) anto_query_request(client->query);
}
static void settle(AntoQuery *query) {
    gint64 deadline = g_get_monotonic_time() + 4000000;
    while (anto_query_busy(query) && g_get_monotonic_time() < deadline)
        g_main_context_iteration(NULL, TRUE);
    g_assert_false(anto_query_busy(query));
}
int main(void) {
    g_autoptr(GObject) owner = g_object_new(G_TYPE_OBJECT, NULL);
    g_autofree char *directory = g_dir_make_tmp("anto-query-XXXXXX", NULL);
    g_autofree char *program = g_build_filename(directory, "read.sh", NULL);
    g_autofree char *flag = g_build_filename(directory, "fail", NULL);
    const char *script = "#!/bin/sh\nsleep .05\n[ ! -f \"$1\" ] || exit 7\nprintf valid\n";
    g_assert_true(g_file_set_contents(program, script, -1, NULL));
    g_assert_cmpint(g_chmod(program, 0700), ==, 0);
    const char *argv[] = {program, flag, NULL};
    Results result = {0};
    g_autoptr(AntoQuery) query = anto_query_new(owner, argv, 2, received, &result);
    for (int i = 0; i < 100; i++) anto_query_request(query);
    settle(query);
    g_assert_cmpint(result.calls, ==, 2);
    g_assert_cmpint(result.changes, ==, 1);
    g_assert_cmpstr(anto_query_cached(query), ==, "valid");
    Reentrant client = {0};
    client.query = anto_query_new(owner, argv, 2, received_and_refresh, &client);
    anto_query_request(client.query);
    anto_query_request(client.query);
    settle(client.query);
    g_assert_cmpint(client.result.calls, ==, 2);
    g_object_unref(client.query);
    g_assert_true(g_file_set_contents(flag, "", 0, NULL));
    anto_query_request(query); settle(query);
    g_assert_cmpint(result.errors, ==, 1);
    g_assert_cmpstr(anto_query_cached(query), ==, "valid");
    anto_query_request(query);
    anto_query_close(query); settle(query);
    g_assert_cmpint(result.calls, ==, 3);
    g_unlink(flag); g_unlink(program); g_rmdir(directory);
    /* A destroyed owner may not receive a result or leave an active query. */
    const char *slow[] = {"/usr/bin/sleep", "2", NULL};
    g_autoptr(AntoQuery) orphan = anto_query_new(owner, slow, 1, received, &result);
    anto_query_request(orphan); g_clear_object(&owner); settle(orphan);
    g_assert_cmpint(result.calls, ==, 3);
    g_print("query: coalescing, cache preservation, cancellation and owner lifetime passed\n");
    return 0;
}
