#pragma once
#include <gtk/gtk.h>

namespace anto {
// A visual surface below the menu, so compositor glass samples the preview.
// It has no keyboard focus, input region, application slot or system effects.
class PreviewBackdrop {
public:
  explicit PreviewBackdrop(GtkWindow *owner);
  ~PreviewBackdrop();
  void show(GdkPaintable *image);
  void hide();
private:
  GtkWindow *window_ = nullptr;
  GtkWidget *frames_ = nullptr;
  GtkWidget *pictures_[2] = {};
  int frame_ = 0;
};
}
