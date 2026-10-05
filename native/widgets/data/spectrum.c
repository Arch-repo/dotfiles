#include "widgets.h"
#include <glib/gstdio.h>
#include <math.h>
void widget_spectrum_parse(WidgetStore *s, const char *line) {
  g_auto(GStrv) values = g_strsplit(line, ";", WIDGET_BARS + 1);
  memset(s->bars, 0, sizeof(s->bars));
  for (int i = 0; i < WIDGET_BARS; i++) {
    if (!values[i])
      break;
    char *end = NULL;
    double amplitude = g_ascii_strtod(values[i], &end);
    if (end != values[i] && !*end && isfinite(amplitude))
      s->bars[i] = CLAMP(amplitude / 1000.0, 0, 1);
  }
  widget_store_changed(s, W_CHANGED_SPECTRUM);
}
static void next_frame(WidgetStore *s);
static void frame_read(GObject *stream, GAsyncResult *result, gpointer data) {
  WidgetStore *s = data;
  g_autoptr(GError) error = NULL;
  g_autofree char *line = g_data_input_stream_read_line_finish(
      G_DATA_INPUT_STREAM(stream), result, NULL, &error);
  if (!s->stopped && line) {
    widget_spectrum_parse(s, line);
    next_frame(s);
  } else if (!s->stopped) {
    s->audio_available = FALSE;
    memset(s->bars, 0, sizeof(s->bars));
    widget_store_changed(s, W_CHANGED_SPECTRUM);
  }
  g_object_unref(s);
}
static void next_frame(WidgetStore *s) {
  g_data_input_stream_read_line_async(s->spectrum_stream, G_PRIORITY_DEFAULT,
                                      s->cancel, frame_read, g_object_ref(s));
}
void widget_spectrum_start(WidgetStore *s) {
  g_autofree char *cava = g_find_program_in_path("cava");
  if (!cava)
    return;
  g_autofree char *directory = g_build_filename(g_get_user_runtime_dir(),
                                                "anto426-native-widgets", NULL);
  g_mkdir_with_parents(directory, 0700);
  g_autofree char *path = g_build_filename(directory, "spectrum.conf", NULL);
  const char *config =
      "[general]\nbars=32\nframerate=30\nautosens=1\nsleep_timer=2\n[input]"
      "\nmethod=pipewire\nsource=auto\n[output]\nmethod=raw\nraw_target=/dev/"
      "stdout\ndata_format=ascii\nascii_max_range=1000\nchannels=mono\nbar_"
      "delimiter=59\nframe_delimiter=10\n[smoothing]\nnoise_reduction=77\n";
  if (!g_file_set_contents(path, config, -1, NULL))
    return;
  g_chmod(path, 0600);
  s->spectrum_process = g_subprocess_new(G_SUBPROCESS_FLAGS_STDOUT_PIPE |
                                             G_SUBPROCESS_FLAGS_STDERR_SILENCE,
                                         NULL, cava, "-p", path, NULL);
  if (!s->spectrum_process)
    return;
  s->audio_available = TRUE;
  s->spectrum_stream = g_data_input_stream_new(
      g_subprocess_get_stdout_pipe(s->spectrum_process));
  next_frame(s);
}
