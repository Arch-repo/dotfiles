#pragma once
#include <gio/gio.h>
G_BEGIN_DECLS
#define ANTO_TYPE_QUERY (anto_query_get_type())
G_DECLARE_FINAL_TYPE(AntoQuery, anto_query, ANTO, QUERY, GObject)
typedef void (*AntoQueryResult)(const char *output, const GError *error, gboolean changed, gpointer data);
/* The owner and argv are captured safely; callbacks run on the creating main context.
 * Close before releasing client data. No command ever runs through a shell here. */
AntoQuery *anto_query_new(GObject *owner, const char *const argv[], guint timeout_seconds,
                          AntoQueryResult result, gpointer data);
void anto_query_request(AntoQuery *query);
void anto_query_close(AntoQuery *query);
const char *anto_query_cached(AntoQuery *query);
gboolean anto_query_busy(AntoQuery *query);
G_END_DECLS
