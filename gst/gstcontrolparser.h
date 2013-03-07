/* GStreamer
 *
 * Copyright (C) 2013 Stefan Sauer <ensonic@users.sf.net>
 *
 * gstcontrolparser.h: Parser for textual controller setup descriptions
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

#ifndef __GST_CONTROL_PARSER_H__
#define __GST_CONTROL_PARSER_H__

#include <gst/gstconfig.h>

#include <glib-object.h>

#include <gst/gstobject.h>

G_BEGIN_DECLS

typedef enum {
  GST_CONTROL_PARSER_ERROR_EXPECT_FUNCTION_NAME = 1,
  GST_CONTROL_PARSER_ERROR_EXPECT_OPENING_PAREN,
  GST_CONTROL_PARSER_ERROR_EXPECT_COMMA_OR_CLOSING_PAREN,
  GST_CONTROL_PARSER_ERROR_EXPECT_PARAMETER_NAME,
  GST_CONTROL_PARSER_ERROR_EXPECT_ASSIGNMENT,
  GST_CONTROL_PARSER_ERROR_EXPECT_VALUE,
  GST_CONTROL_PARSER_ERROR_EXPECT_FUNCTION_OR_VALUE,
  GST_CONTROL_PARSER_ERROR_UNSUPPORTED_CONTROL_SOURCE,
  GST_CONTROL_PARSER_ERROR_UNSUPPORTED_CONTROL_BINDING
} GstControlParserError;

void gst_control_parser_init (void);

gboolean gst_control_parser_parse (const gchar * expr, GstObject *parent, const gchar *prop_name, GError **error);

/* temporary */
void gst_control_binding_register (const gchar *name, GType type);
void gst_control_source_register (const gchar *name, GType type);

G_END_DECLS

#endif /* __GST_CONTROL_PARSER_H__ */
