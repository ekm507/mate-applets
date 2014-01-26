/* command.c:
 *
 * Copyright (C) 2013-2014 Stefano Karapetsas
 *
 *  This file is part of MATE Applets.
 *
 *  MATE Applets is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  MATE Applets is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with MATE Applets.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *      Stefano Karapetsas <stefano@karapetsas.com>
 */

#include <config.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gtk/gtk.h>

#include <mate-panel-applet.h>
#include <mate-panel-applet-gsettings.h>

#define COMMAND_SCHEMA "org.mate.panel.applet.command"
#define COMMAND_KEY    "command"
#define INTERVAL_KEY   "interval"
#define SHOW_ICON_KEY  "show-icon"

typedef struct
{
    MatePanelApplet   *applet;
    GSettings         *settings;
    GtkLabel          *label;
    GtkImage          *image;
    GtkHBox           *hbox;
    guint              timeout_id;
} CommandApplet;

static void command_about_callback (GtkAction *action, CommandApplet *command_applet);
static void command_settings_callback (GtkAction *action, CommandApplet *command_applet);

static const GtkActionEntry applet_menu_actions [] = {
    { "Properties", GTK_STOCK_PROPERTIES, NULL, NULL, NULL, G_CALLBACK (command_settings_callback) },
    { "About", GTK_STOCK_ABOUT, NULL, NULL, NULL, G_CALLBACK (command_about_callback) }
};

static char *ui = "<menuitem name='Item 1' action='Properties' />"
                  "<menuitem name='Item 2' action='About' />";

static void
command_applet_destroy (MatePanelApplet *applet_widget, CommandApplet *command_applet)
{
    g_assert (command_applet);

    if (command_applet->timeout_id != 0)
    {
        g_source_remove(command_applet->timeout_id);
        command_applet->timeout_id = 0;
    }

    g_object_unref (command_applet->settings);
}

static void
command_about_callback (GtkAction *action, CommandApplet *command_applet)
{
    const char* authors[] = { "Stefano Karapetsas <stefano@karapetsas.com>", NULL };

    gtk_show_about_dialog(NULL,
                          "version", VERSION,
                          "copyright", "Copyright © 2013-2014 Stefano Karapetsas",
                          "authors", authors,
                          "comments", _("Shows the output of a command"),
                          "translator-credits", _("translator-credits"),
                          "logo-icon-name", "terminal",
    NULL );
}

static void
command_settings_callback (GtkAction *action, CommandApplet *command_applet)
{
    GtkDialog *dialog;
    GtkTable *table;
    GtkWidget *widget;
    GtkWidget *command;
    GtkWidget *interval;
    GtkWidget *showicon;

    dialog = GTK_DIALOG (gtk_dialog_new_with_buttons(_("Command Applet Preferences"),
                                                     NULL,
                                                     GTK_DIALOG_MODAL,
                                                     GTK_STOCK_CLOSE,
                                                     GTK_RESPONSE_CLOSE,
                                                     NULL));
    table = gtk_table_new (3, 2, FALSE);

    gtk_window_set_default_size (GTK_WINDOW (dialog), 350, 150);

    widget = gtk_label_new (_("Command:"));
    gtk_table_attach (table, widget, 1, 2, 0, 1,
                      GTK_FILL, GTK_FILL,
                      0, 0);

    command = gtk_entry_new ();
    gtk_table_attach (table, command, 2, 3, 0, 1,
                      GTK_EXPAND | GTK_FILL | GTK_SHRINK, GTK_FILL,
                      0, 0);

    widget = gtk_label_new (_("Interval:"));
    gtk_table_attach (table, widget, 1, 2, 1, 2,
                      GTK_FILL, GTK_FILL,
                      0, 0);

    interval = gtk_spin_button_new_with_range (1.0, 600.0, 1.0);
    gtk_table_attach (table, interval, 2, 3, 1, 2,
                      GTK_EXPAND | GTK_FILL | GTK_SHRINK, GTK_FILL,
                      0, 0);

    showicon = gtk_check_button_new_with_label (_("Show icon"));
    gtk_table_attach (table, showicon, 2, 3, 3, 4,
                      GTK_EXPAND | GTK_FILL | GTK_SHRINK, GTK_FILL,
                      0, 0);

    gtk_box_pack_start_defaults (GTK_BOX (dialog->vbox), table);

    g_signal_connect (dialog, "response", G_CALLBACK (gtk_widget_destroy), dialog);

    /* use g_settings_bind to manage settings */
    g_settings_bind (command_applet->settings, COMMAND_KEY, command, "text", G_SETTINGS_BIND_DEFAULT);
    g_settings_bind (command_applet->settings, INTERVAL_KEY, interval, "value", G_SETTINGS_BIND_DEFAULT);
    g_settings_bind (command_applet->settings, SHOW_ICON_KEY, showicon, "active", G_SETTINGS_BIND_DEFAULT);

    gtk_widget_show_all (GTK_WIDGET (dialog));
}

static gboolean
command_execute (CommandApplet *command_applet)
{
    GError *error = NULL;
    gchar *command = NULL;
    gchar *output = NULL;
    gint interval = 0;
    gint ret = 0;

    /* FIXME: use GSettings changed event */
    interval = g_settings_get_int (command_applet->settings, INTERVAL_KEY);
    command = g_settings_get_string (command_applet->settings, COMMAND_KEY);

    g_return_if_fail (command != NULL);
    g_return_if_fail (command[0] != 0);

    /* minimum interval */
    if (interval <= 0)
        interval = 1;

    if (g_spawn_command_line_sync (command, &output, NULL, &ret, &error))
    {
        if ((output != NULL) && (output[0] != 0))
        {
            if (g_str_has_suffix (output, "\n")) {
                output[strlen(output) - 1] = 0;
            }
            gtk_label_set_text (command_applet->label, output);
        }
        else
            gtk_label_set_text (command_applet->label, "#");
    }
    else
        gtk_label_set_text (command_applet->label, "#");

    g_free (output);
    g_free (command);

    /* start timer for next execution */
    command_applet->timeout_id = g_timeout_add_seconds (interval,
                                                        (GSourceFunc) command_execute,
                                                        command_applet);

    return FALSE;
}

static gboolean
command_applet_fill (MatePanelApplet* applet)
{
    CommandApplet *command_applet;

    g_set_application_name (_("Command Applet"));
    gtk_window_set_default_icon_name ("terminal");

    mate_panel_applet_set_flags (applet, MATE_PANEL_APPLET_EXPAND_MINOR);
    mate_panel_applet_set_background_widget (applet, GTK_WIDGET (applet));

    command_applet = g_malloc0(sizeof(CommandApplet));
    command_applet->applet = applet;
    command_applet->settings = mate_panel_applet_settings_new (applet, COMMAND_SCHEMA);
    command_applet->hbox = gtk_hbox_new (FALSE, 0);
    command_applet->image = gtk_image_new_from_icon_name ("terminal", 24);
    command_applet->label = gtk_label_new ("#");
    command_applet->timeout_id = 0;

    /* we add the Gtk label into the applet */
    gtk_box_pack_start (GTK_BOX (command_applet->hbox),
                        GTK_WIDGET (command_applet->image),
                        TRUE, TRUE, 0);
    gtk_box_pack_start (GTK_BOX (command_applet->hbox),
                        GTK_WIDGET (command_applet->label),
                        TRUE, TRUE, 0);

    gtk_container_add (GTK_CONTAINER (applet),
                       GTK_WIDGET (command_applet->hbox));

    gtk_widget_show_all (GTK_WIDGET (command_applet->applet));

    g_settings_bind (command_applet->settings,
                     SHOW_ICON_KEY,
                     command_applet->image,
                     "visible",
                     G_SETTINGS_BIND_DEFAULT);

    g_signal_connect(G_OBJECT (command_applet->applet), "destroy",
                     G_CALLBACK (command_applet_destroy),
                     command_applet);

    /* set up context menu */
    GtkActionGroup *action_group = gtk_action_group_new ("Command Applet Actions");
    gtk_action_group_add_actions (action_group, applet_menu_actions,
                                  G_N_ELEMENTS (applet_menu_actions), command_applet);
    mate_panel_applet_setup_menu (command_applet->applet, ui, action_group);

    /* first command execution */
    command_execute (command_applet);

    return TRUE;
}

/* this function, called by mate-panel, will create the applet */
static gboolean
command_factory (MatePanelApplet* applet, const char* iid, gpointer data)
{
    gboolean retval = FALSE;

    if (!g_strcmp0 (iid, "CommandApplet"))
        retval = command_applet_fill (applet);

    return retval;
}

/* needed by mate-panel applet library */
MATE_PANEL_APPLET_OUT_PROCESS_FACTORY("CommandAppletFactory",
                                      PANEL_TYPE_APPLET,
                                      "Command applet",
                                      command_factory,
                                      NULL)