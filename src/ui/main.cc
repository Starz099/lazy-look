#include <adwaita.h>

#include "lazyfinder.hh"

static void on_activate(GtkApplication *app, gpointer)
{
  GtkWidget *window = adw_application_window_new(app);
  gtk_window_set_title(GTK_WINDOW(window), "LazyFinder");
  gtk_window_set_default_size(GTK_WINDOW(window), 600, 400);
  gtk_window_present(GTK_WINDOW(window));
}

int main(int argc, char *argv[])
{
  AdwApplication *app = adw_application_new("dev.lazyfinder.app",
                                            G_APPLICATION_DEFAULT_FLAGS);
  g_signal_connect(app, "activate", G_CALLBACK(on_activate), nullptr);
  int status = g_application_run(G_APPLICATION(app), argc, argv);
  g_object_unref(app);
  return status;
}
