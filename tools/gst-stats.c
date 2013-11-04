/* GStreamer
 * Copyright (C) 2013 Stefan Sauer <ensonic@users.sf.net>
 *
 * gst-stats.c: statistics tracing front end
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "tools.h"

static GRegex *raw_log = NULL;
static GRegex *ansi_log = NULL;

static gboolean
init (void)
{
  /* compile the parser regexps */
  /* 0:00:00.004925027 31586      0x1c5c600 DEBUG           GST_REGISTRY gstregistry.c:463:gst_registry_add_plugin:<registry0> adding plugin 0x1c79160 for filename "/usr/lib/gstreamer-1.0/libgstxxx.so" */
  raw_log = g_regex_new (
      /* 1: ts */
      "^([0-9:.]+) +"
      /* 2: pid */
      "([0-9]+) +"
      /* 3: thread */
      "(0x[0-9a-fA-F]+) +"
      /* 4: level */
      "([A-Z]+) +"
      /* 5: category */
      "([a-zA-Z_-]+) +"
      /* 6: file:line:func: */
      "([^:]+:[0-9]+:[^:]+:)"
      /* 7: (obj)? log-text */
      "(.*)$", 0, 0, NULL);
  if (!raw_log) {
    GST_WARNING ("failed to compile the 'raw' parser");
    return FALSE;
  }

  ansi_log = g_regex_new (
      /* 1: ts */
      "^([0-9:.]+) +"
      /* 2: pid */
      "\\\e\\\[[0-9;]+m +([0-9]+)\\\e\\\[00m +"
      /* 3: thread */
      "(0x[0-9a-fA-F]+) +"
      /* 4: level */
      "(?:\\\e\\\[[0-9;]+m)?([A-Z]+) +\\\e\\\[00m +"
      /* 5: category */
      "\\\e\\\[[0-9;]+m +([a-zA-Z_-]+) +"
      /* 6: file:line:func: */
      "([^:]+:[0-9]+:[^:]+:)(?:\\\e\\\[00m)?"
      /* 7: (obj)? log-text */
      "(.*)$", 0, 0, NULL);
  if (!raw_log) {
    GST_WARNING ("failed to compile the 'ansi' parser");
    return FALSE;
  }

  return TRUE;
}

static void
done (void)
{
  if (raw_log)
    g_regex_unref (raw_log);
  if (ansi_log)
    g_regex_unref (ansi_log);
}

static void
stats (const gchar * filename)
{
  FILE *log;

  if ((log = fopen (filename, "rt"))) {
    gchar line[5001];

    // probe format
    if (fgets (line, 5000, log)) {
      GMatchInfo *match_info;
      GRegex *parser;
      guint lnr = 0;
      gchar *level, *data;

      if (strchr (line, 27)) {
        parser = ansi_log;
        GST_INFO ("format is 'ansi'");
      } else {
        parser = raw_log;
        GST_INFO ("format is 'raw'");
      }
      rewind (log);

      // parse the log
      while (!feof (log)) {
        if (fgets (line, 5000, log)) {
          if (g_regex_match (parser, line, 0, &match_info)) {
            // filter by level
            level = g_match_info_fetch (match_info, 4);
            if (!strcmp (level, "TRACE")) {
              data = g_match_info_fetch (match_info, 7);
              // TODO(ensonic): parse data
              // GstStructure *s = gst_structure_from_string (data, NULL);
              puts (data);
            }
          } else {
            if (*line) {
              GST_WARNING ("foreign log entry: %s:%d:'%s'", filename, lnr,
                  line);
            }
          }
          g_match_info_free (match_info);
          match_info = NULL;
          lnr++;
        } else {
          if (!feof (log)) {
            // TODO(ensonic): run wc -L on the log file
            fprintf (stderr, "line too long");
          }
        }
      }
      fclose (log);
      // TODO(ensonic): print stats
    } else {
      GST_WARNING ("empty log");
    }
  }
}

gint
main (gint argc, gchar * argv[])
{
  gchar **filenames = NULL;
  guint num;
  GError *err = NULL;
  GOptionContext *ctx;
  GOptionEntry options[] = {
    GST_TOOLS_GOPTION_VERSION,
    // TODO(ensonic): add a summary flag, if set read the whole thing, print
    // stats once, and exit
    {G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &filenames, NULL}
    ,
    {NULL}
  };

#ifdef ENABLE_NLS
  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);
#endif

  g_set_prgname ("gst-stats-" GST_API_VERSION);

  ctx = g_option_context_new ("FILE");
  g_option_context_add_main_entries (ctx, options, GETTEXT_PACKAGE);
  g_option_context_add_group (ctx, gst_init_get_option_group ());
  if (!g_option_context_parse (ctx, &argc, &argv, &err)) {
    g_print ("Error initializing: %s\n", GST_STR_NULL (err->message));
    exit (1);
  }
  g_option_context_free (ctx);

  gst_tools_print_version ();

  if (filenames == NULL || *filenames == NULL) {
    g_print ("Please give one filename to %s\n\n", g_get_prgname ());
    return 1;
  }
  num = g_strv_length (filenames);
  if (num == 0 || num > 1) {
    g_print ("Please give exactly one filename to %s (%d given).\n\n",
        g_get_prgname (), num);
    return 1;
  }

  if (init ()) {
    stats (filenames[0]);
  }
  done ();

  g_strfreev (filenames);
  return 0;
}
