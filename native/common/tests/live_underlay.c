#include <gtk/gtk.h>
#include <stdlib.h>

static GtkWidget *drawing;
static int phase = -1;
static const char *control;
static int motion;
static void draw(GtkDrawingArea *area, cairo_t *canvas, int width, int height, gpointer data) {
    (void)area; (void)data;
    const double colours[3][3] = {{.85,.10,.13},{.07,.23,.85},{.12,.72,.21}};
    const double *colour = colours[MAX(phase,0)%3];
    cairo_set_source_rgb(canvas, colour[0], colour[1], colour[2]); cairo_paint(canvas);
    int offset=phase==3?motion%128:0;
    for (int x=-128+offset;x<width;x+=64) for (int y=0;y<height;y+=64) if ((x/64+y/64)%2) {
        cairo_set_source_rgba(canvas,1,1,1,.38);cairo_rectangle(canvas,x,y,64,64);cairo_fill(canvas);
    }
    g_autofree char *ready=g_strconcat(control,".ready",NULL);
    g_autofree char *value=g_strdup_printf("%d",phase);
    g_file_set_contents(ready,value,-1,NULL);
}
static gboolean update(gpointer data) {
    (void)data;g_autofree char *text=NULL;
    if(g_file_get_contents(control,&text,NULL,NULL)) {
        int next=atoi(text);if(next!=phase){phase=next;gtk_widget_queue_draw(drawing);}
    }
    if (phase==3) {motion+=5;gtk_widget_queue_draw(drawing);}
    return G_SOURCE_CONTINUE;
}
static void activate(GtkApplication *application,gpointer data) {
    (void)data;
    GtkWindow *window=GTK_WINDOW(gtk_application_window_new(application));
    gtk_window_set_title(window,"Anto Glass Underlay Fixture");
    drawing=gtk_drawing_area_new();gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(drawing),draw,NULL,NULL);
    gtk_window_set_child(window,drawing);gtk_window_fullscreen(window);gtk_window_present(window);
    g_timeout_add(50,update,NULL);
}
int main(int argc,char **argv) {
    control=g_getenv("ANTO426_UNDERLAY_CONTROL");if(!control)return 2;
    g_setenv("GDK_BACKEND","wayland",TRUE);g_setenv("GTK_THEME","Adwaita:dark",TRUE);
    GtkApplication *application=gtk_application_new("com.anto426.GlassUnderlay",G_APPLICATION_NON_UNIQUE);
    g_signal_connect(application,"activate",G_CALLBACK(activate),NULL);
    int result=g_application_run(G_APPLICATION(application),argc,argv);g_object_unref(application);return result;
}
