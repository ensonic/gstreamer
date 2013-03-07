/* GStreamer
 *
 * Copyright (C) 2013 Stefan Sauer <ensonic@users.sf.net>
 *
 * gstcontrolparser.c: Parser for textual controller setup descriptions
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
/**
 * SECTION:gstcontrolparser
 * @short_description: parser for textual controller setup descriptions
 *
 * A parser that creates a #GstControlBinding from a textual description. This
 * allows to include controller setups in presets and gst-launch command lines.
 *
 * TODO(ensonic): describe the syntax
 */

#include "gst_private.h"

#include <stdlib.h>
#include <string.h>

#include <glib.h>
#include <gst/gst.h>

#define GST_CAT_DEFAULT control_parser_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

typedef enum
{
  FUNCTION = 0,
  NAMED_PARAMETERS,
  NAMED_PARAMETER,
  VALUE,
  NUM_SYMBOLS
} Symbol;

typedef enum
{
  NAME = 0,
  NUMERIC_VALUE,
  STRING_VALUE,
  OPENING_PAREN,
  CLOSING_PAREN,
  COMMA,
  ASSIGNMENT,
  NUM_TERMINAL_SYMBOLS
} TerminalSymbol;

/* we compile them at first use and never free them explicitly */
static GRegex *regex[NUM_TERMINAL_SYMBOLS] = { NULL, };

static const gchar *regex_str[NUM_TERMINAL_SYMBOLS] = {
  /* name */
  "^[\\s\\n]*([a-z][a-z0-9_-]*)[\\s\\n]*",
  /* numeric_value */
  "^[\\s\\n]*(\\d*\\.\\d*)[\\s\\n]*",
  /* string_value */
  "^[\\s\\n]*'(.*?)'[\\s\\n]*",
  /* ( */
  "^[\\s\\n]*\\([\\s\\n]*",
  /* ) */
  "^[\\s\\n]*\\)[\\s\\n]*",
  /* , */
  "^[\\s\\n]*,[\\s\\n]*",
  /* = */
  "^[\\s\\n]*=[\\s\\n]*",
};

typedef struct
{
  /* the expression */
  const gchar *expr;
  /* parsing position */
  gint pos;
  /* error code */
  gint error_code;
  /* the current object and property */
  GstObject *object;
  const gchar *prop_name;
} GstControlParserContext;

/* TODO(ensonic): turn these into registry features */

static GHashTable *bindings = NULL, *sources = NULL;

void
gst_control_binding_register (const gchar * name, GType type)
{
  if (!bindings) {
    bindings = g_hash_table_new (g_str_hash, g_str_equal);
  }
  g_hash_table_insert (bindings, (gpointer) name, GINT_TO_POINTER (type));
}

void
gst_control_source_register (const gchar * name, GType type)
{
  if (!sources) {
    sources = g_hash_table_new (g_str_hash, g_str_equal);
  }
  g_hash_table_insert (sources, (gpointer) name, GINT_TO_POINTER (type));
}

static GstObject *
gst_control_binding_facory_make (const gchar * name, GstObject * parent,
    const gchar * prop_name)
{
  GType type;

  if (!bindings)
    return NULL;

  type =
      (GType) GPOINTER_TO_INT (g_hash_table_lookup (bindings, (gpointer) name));
  if (type != G_TYPE_INVALID) {
    return g_object_new (type, "object", parent, "name", prop_name, NULL);
  }
  GST_WARNING ("no gtype found for '%s': %d (in %d entries)", name, (gint) type,
      g_hash_table_size (bindings));
  return NULL;
}

static GstObject *
gst_control_source_facory_make (const gchar * name)
{
  GType type;

  if (!sources)
    return NULL;

  type =
      (GType) GPOINTER_TO_INT (g_hash_table_lookup (sources, (gpointer) name));
  if (type != G_TYPE_INVALID) {
    return g_object_new (type, NULL);
  }
  GST_WARNING ("no gtype found for '%s': %d (in %d entries)", name, (gint) type,
      g_hash_table_size (sources));
  return NULL;
}

/* TODO(ensonic): move into gsterror.c ? */
static GQuark error_quark = 0;

static const gchar *
get_error_string (GstControlParserContext * ctx)
{
  switch (ctx->error_code) {
    case GST_CONTROL_PARSER_ERROR_EXPECT_FUNCTION_NAME:
      return "expect <name> for function";
    case GST_CONTROL_PARSER_ERROR_EXPECT_OPENING_PAREN:
      return "expect <(>";
    case GST_CONTROL_PARSER_ERROR_EXPECT_COMMA_OR_CLOSING_PAREN:
      return "expect <,> or <)>";
    case GST_CONTROL_PARSER_ERROR_EXPECT_PARAMETER_NAME:
      return "expect <name> for parameter";
    case GST_CONTROL_PARSER_ERROR_EXPECT_ASSIGNMENT:
      return "expect <=>";
    case GST_CONTROL_PARSER_ERROR_EXPECT_VALUE:
      return "expect <value>";
    case GST_CONTROL_PARSER_ERROR_EXPECT_FUNCTION_OR_VALUE:
      return "expect <function> or <value> for parameter";
    case GST_CONTROL_PARSER_ERROR_UNSUPPORTED_CONTROL_SOURCE:
      return "unsupported control-source";
    case GST_CONTROL_PARSER_ERROR_UNSUPPORTED_CONTROL_BINDING:
      return "unsupported control-binding";
    default:
      return "";
  }
}

static gchar *
format_error (GstControlParserContext * ctx)
{
  gchar *error_msg;
  /* newlines to spaces */
  gchar *one_line = g_strdup (ctx->expr);
  gchar *s = one_line;
  while (*s) {
    if (*s == '\n') {
      *s = ' ';
    }
    s++;
  };

  error_msg = g_strdup_printf ("Syntax error: %s at pos: %d\n  %s\n   %*s",
      get_error_string (ctx), ctx->pos, one_line, ctx->pos, "^");
  g_free (one_line);
  return error_msg;
}

static gboolean
parse_terminal (TerminalSymbol type, GstControlParserContext * ctx,
    gchar ** value)
{
  GMatchInfo *mi = NULL;
  const gchar *expr = &ctx->expr[ctx->pos];

  if (g_regex_match_full (regex[type], expr, -1, 0, 0, &mi, NULL)) {
    gint consumed;
    if (value) {
      *value = g_match_info_fetch (mi, 1);
    }
    g_match_info_fetch_pos (mi, 0, NULL, &consumed);
    g_match_info_free (mi);
    ctx->pos += consumed;
    return TRUE;
  }
  return FALSE;
}

static gboolean
parse (Symbol type, GstControlParserContext * _ctx, gboolean fatal)
{
  GstControlParserContext ctx = *_ctx;

  GST_TRACE ("scanning: %d at pos: %d", type, ctx.pos);
  GST_TRACE ("  %s", ctx.expr);
  GST_TRACE ("   %*s", ctx.pos, "^");

  switch (type) {
    case FUNCTION:{
      gchar *name;
      GstObject *object;

      if (!parse_terminal (NAME, &ctx, &name)) {
        ctx.error_code = GST_CONTROL_PARSER_ERROR_EXPECT_FUNCTION_NAME;
        goto error;
      }
      if (!parse_terminal (OPENING_PAREN, &ctx, NULL)) {
        ctx.error_code = GST_CONTROL_PARSER_ERROR_EXPECT_OPENING_PAREN;
        goto error;
      }

      GST_DEBUG ("function: name='%s'", name);

      /* create new object */
      if (GST_IS_CONTROL_BINDING (ctx.object)) {
        if (!(object = gst_control_source_facory_make (name))) {
          GST_WARNING ("no control-source: '%s'", name);
          ctx.error_code = GST_CONTROL_PARSER_ERROR_UNSUPPORTED_CONTROL_SOURCE;
          goto error;
        }
      } else {
        if (!(object =
                gst_control_binding_facory_make (name, ctx.object,
                    ctx.prop_name))) {
          GST_WARNING ("no control-binding: '%s'", name);
          ctx.error_code = GST_CONTROL_PARSER_ERROR_UNSUPPORTED_CONTROL_BINDING;
          goto error;
        }
      }
      GST_DEBUG ("object = %p", object);
      /* set object on parent */
      if (GST_IS_CONTROL_BINDING (object)) {
        GST_DEBUG ("gst_object_add_control_binding (%p, %p)", ctx.object,
            object);
        gst_object_add_control_binding (ctx.object,
            (GstControlBinding *) object);
      } else {
        GST_DEBUG ("g_object_set (%p, %s, %p, NULL)", ctx.object, ctx.prop_name,
            object);
        g_object_set (ctx.object, ctx.prop_name, object, NULL);
        gst_object_unref (object);
      }
      ctx.object = object;

      if (!parse_terminal (CLOSING_PAREN, &ctx, NULL)) {
        if (!parse (NAMED_PARAMETERS, &ctx, fatal)) {
          return FALSE;
        }
      }
      _ctx->object = object;
      break;
    }
    case NAMED_PARAMETERS:{
      gboolean more_parameters = FALSE;
      do {
        if (!parse (NAMED_PARAMETER, &ctx, fatal)) {
          goto error;
        }
        if (parse_terminal (COMMA, &ctx, NULL)) {
          more_parameters = TRUE;
        } else if (parse_terminal (CLOSING_PAREN, &ctx, NULL)) {
          more_parameters = FALSE;
        } else {
          ctx.error_code =
              GST_CONTROL_PARSER_ERROR_EXPECT_COMMA_OR_CLOSING_PAREN;
          goto error;
        }
      } while (more_parameters);
      break;
    }
    case NAMED_PARAMETER:{
      gchar *name;

      if (!parse_terminal (NAME, &ctx, &name)) {
        ctx.error_code = GST_CONTROL_PARSER_ERROR_EXPECT_PARAMETER_NAME;
        goto error;
      }
      if (!parse_terminal (ASSIGNMENT, &ctx, NULL)) {
        ctx.error_code = GST_CONTROL_PARSER_ERROR_EXPECT_ASSIGNMENT;
        goto error;
      }

      GST_DEBUG ("parameter: name='%s'", name);
      ctx.prop_name = name;

      /* try a function or value, don't store error, so that we can backtrack */
      if (!parse (FUNCTION, &ctx, FALSE)
          && !parse (VALUE, &ctx, FALSE)) {
        ctx.error_code = GST_CONTROL_PARSER_ERROR_EXPECT_FUNCTION_OR_VALUE;
        goto error;
      }
      break;
    }
    case VALUE:{
      gchar *value = NULL;

      // set property
      if (parse_terminal (NUMERIC_VALUE, &ctx, &value)) {
        GST_DEBUG ("numeric: value='%s'", value);
      } else if (parse_terminal (STRING_VALUE, &ctx, &value)) {
        GST_DEBUG ("string: value='%s'", value);
      } else {
        ctx.error_code = GST_CONTROL_PARSER_ERROR_EXPECT_VALUE;
        goto error;
      }
      GST_DEBUG ("g_object_set (%p, %s, %s, NULL)", ctx.object, ctx.prop_name,
          value);
      gst_util_set_object_arg ((GObject *) ctx.object, ctx.prop_name, value);
    }
    default:
      break;
  }
  // update consumed for parent
  _ctx->pos = ctx.pos;
  return TRUE;
error:
  if (fatal) {
    // update consumed for parent in error case and return error_code
    _ctx->pos = ctx.pos;
    _ctx->error_code = ctx.error_code;
    GST_INFO ("parsing failed: %d: %s", ctx.error_code,
        get_error_string (&ctx));
  }
  return FALSE;
}

/* we need to recursively parse sub-sexpressions) */
gboolean
gst_control_parser_parse (const gchar * expr, GstObject * parent,
    const gchar * prop_name, GError ** error)
{
  GstControlParserContext ctx;
  ctx.pos = 0;
  ctx.expr = expr;
  ctx.error_code = 0;
  ctx.object = parent;
  ctx.prop_name = prop_name;

  g_return_val_if_fail (expr, FALSE);
  g_return_val_if_fail (GST_IS_OBJECT (parent), FALSE);
  g_return_val_if_fail (prop_name, FALSE);

  GST_INFO ("parsing: '%s'", expr);
  if (parse (FUNCTION, &ctx, TRUE)) {
    GST_INFO ("controller expression parsed, object = %p", ctx.object);
    return TRUE;
  } else {
    GST_WARNING ("controller expression has errors, code=%d", ctx.error_code);
    if (error) {
      gchar *error_msg = format_error (&ctx);
      /* TODO(ensonic): gsterror (domain) integration, use code for different parser
       * errors */
      g_set_error_literal (error, error_quark, ctx.error_code, error_msg);
      g_free (error_msg);
    }
    return FALSE;
  }
}

void
gst_control_parser_init (void)
{
  if (!regex[0]) {
    gint i;

    GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "gstcontrolparser", 0,
        "parser for controller setup");

    error_quark = g_quark_from_static_string ("controlparser");

    /* init regex for terminal symbols */
    for (i = 0; i < NUM_TERMINAL_SYMBOLS; i++) {
      regex[i] = g_regex_new (regex_str[i], 0, 0, NULL);
    }
  }
}
