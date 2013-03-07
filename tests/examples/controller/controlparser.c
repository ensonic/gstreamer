/* test application for parsing controller expressions
 *
 * syntax:
 * - we use a function syntax with named parameters     
 *   volume="(ctrl)direct(control-source=add(direct(control-source=const(value=0.5)),direct(control-source=lfo(waveform='sine',frequency=50.0))))"
 *   volume="(ctrl)direct(control-source=lfo(waveform='sine',direct(control-source=mul(direct(const(value=50.0)),direct(control-source=lfo(waveform='sine',frequency=50.0))))))"
 *   color="(ctrl)argb(control-source-r=lfo(waveform='sine',frequency=50.0),control-source-g=lfo(waveform='sine',frequency=20.0))
 *
 * symbols / grammar:
 *   function
 *     name \( named_parameter_list \)
 *  
 *   named_parameter_list
 *     named_parameter (,named_parameter)*
 *     
 *   named_parameter
 *     name = function|value
 *  
 *   name
 *      \s*([a-z_-]+)\s*
 *  
 *   value
 *     numeric-value|string-value
 *  
 *   numeric-value
 *     \d+\.?\d+
 *  
 *   string-value
 *     '.*' 
 *
 * - name lookups
 *   - we will look up bindings and sources from the registry
 *   - we use named parameters in the expressions to map to gobject properties
 *   - having short names would be nice
 *     - if we use registry feature, we can register the functions with short names
 *     - can we use "nick" for properties?
 *
 * TODO:
 * - real registry support for Control{Binding,Source}
 * - the control-formatter
 * - preset integration
 * - parse-launch integration
 *
 * API:
 * GstControlBinding *
 * gst_parse_control_binding_from_description(const gchar *description);
 * gchar *
 * gst_describe_control_binding(GstControlBinding *);
 *
 * GstControlBinding *
 * gst_control_binding_factory_make(const gchar *name);
 *
 * GstControlSource *
 * gst_control_source_factory_make(const gchar *name);
 *
 * Testing:
 * - dummy registry with fake control-bindings and sources?
 * - ideally we'd like to assert on the parse-tree that was generated
 */

#include <stdlib.h>
#include <string.h>

#include <glib.h>
#include <gst/gst.h>

#include <gst/controller/gstlfocontrolsource.h>
#include <gst/controller/gstdirectcontrolbinding.h>
#include <gst/controller/gstargbcontrolbinding.h>


typedef struct
{
  const gchar *expr;
  const gchar *pipeline;
  const gchar *elem_name;
  const gchar *prop_name;
} Setup;
/* Example expressions */
static const Setup setup[] = {
  {
        "direct(control-source=lfo(waveform='sine',offset=0.5,amplitude=0.3,frequency=0.5))",
        "audiotestsrc name=src ! autoaudiosink",
        "src",
      "freq"},
  {
        "direct(control-source=lfo(waveform='sine',offset=0.5,amplitude=0.3,frequency=direct(control-source=lfo(waveform='sine',frequency=0.05,offset=0.005,amplitude=0.01))))",
        "audiotestsrc name=src ! autoaudiosink",
        "src",
      "freq"},
  {                             /* this uses a 'add' control-source which we don't have yet */
        "direct(control-source=add(v1=0.5,v2=direct(control-source=lfo(waveform='sine',frequency=0.5))))",
        "audiotestsrc name=src ! autoaudiosink",
        "src",
      "freq"},
  {
        "argb(control-source-b=lfo(waveform='sine',offset=0.5,amplitude=0.5,frequency=1.0),control-source-g=lfo(waveform='sine',offset=0.5,amplitude=0.5,frequency=0.2))",
        "videotestsrc name=src pattern=17 ! autovideosink",
        "src",
      "foreground-color"},
};

gint
main (gint argc, gchar ** argv)
{
  gint ix = 0;
  GError *error = NULL;
  GstElement *pipeline, *src;

  gst_init (&argc, &argv);
  gst_control_parser_init ();
  gst_control_binding_register ("argb", GST_TYPE_ARGB_CONTROL_BINDING);
  gst_control_binding_register ("direct", GST_TYPE_DIRECT_CONTROL_BINDING);
  gst_control_source_register ("lfo", GST_TYPE_LFO_CONTROL_SOURCE);

  if (argc > 1) {
    gint max_ix = G_N_ELEMENTS (setup) - 1;
    ix = atoi (argv[1]);
    ix = MIN (ix, max_ix);
  }

  /* TODO(ensonic): need different pipeline for #3
   * need add + const control-sources for #2
   */
  if (!(pipeline = gst_parse_launch (setup[ix].pipeline, &error))) {
    if (error) {
      g_printerr ("pipeline has errors\n%s\n", error->message);
      g_error_free (error);
    }
    return EXIT_FAILURE;
  }
  if (!(src = gst_bin_get_by_name (GST_BIN (pipeline), setup[ix].elem_name))) {
    GST_DEBUG_BIN_TO_DOT_FILE (GST_BIN (pipeline), GST_DEBUG_GRAPH_SHOW_ALL,
        "controlparser");
    g_printerr ("can't lookup element named '%s'\n", setup[ix].elem_name);
    return EXIT_FAILURE;
  }

  if (!gst_control_parser_parse (setup[ix].expr, GST_OBJECT_CAST (src),
          setup[ix].prop_name, &error)) {
    if (error) {
      g_printerr ("expression has errors\n%s\n", error->message);
      g_error_free (error);
    }
    return EXIT_FAILURE;
  }
  gst_element_set_state (pipeline, GST_STATE_PLAYING);

  /* run until Ctrl-C */
  g_usleep (G_USEC_PER_SEC * 5);

  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_object_unref (pipeline);
  return EXIT_SUCCESS;
}
