/* GStreamer
 *
 * Copyright (C) 2011 Stefan Sauer <ensonic@users.sf.net>
 *
 * gstcontrolbindingdirect.h: Direct attachment for control sources
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GST_CONTROL_BINDING_DIRECT_H__
#define __GST_CONTROL_BINDING_DIRECT_H__

#include <gst/gstconfig.h>

#include <glib-object.h>

#include <gst/gstcontrolsource.h>

G_BEGIN_DECLS

#define GST_TYPE_CONTROL_BINDING_DIRECT \
  (gst_control_binding_direct_get_type())
#define GST_CONTROL_BINDING_DIRECT(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_CONTROL_BINDING_DIRECT,GstControlBindingDirect))
#define GST_CONTROL_BINDING_DIRECT_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_CONTROL_BINDING_DIRECT,GstControlBindingDirectClass))
#define GST_IS_CONTROL_BINDING_DIRECT(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_CONTROL_BINDING_DIRECT))
#define GST_IS_CONTROL_BINDING_DIRECT_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_CONTROL_BINDING_DIRECT))
#define GST_CONTROL_BINDING_DIRECT_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_CONTOL_SOURCE, GstControlBindingDirectClass))

typedef struct _GstControlBindingDirect GstControlBindingDirect;
typedef struct _GstControlBindingDirectClass GstControlBindingDirectClass;

/**
 * GstControlBindingDirectConvert:
 * @self: the #GstControlBindingDirect instance
 * @src_value: the value returned by the cotnrol source
 * @dest_value: the target GValue
 *
 * Function to map a control-value to the target GValue.
 */
typedef void (* GstControlBindingDirectConvert) (GstControlBindingDirect *self, gdouble src_value, GValue *dest_value);

/**
 * GstControlBindingDirect:
 * @name: name of the property of this binding
 *
 * The instance structure of #GstControlBindingDirect.
 */
struct _GstControlBindingDirect {
  GstControlBinding parent;
  
  /*< private >*/
  GstControlSource *cs;    /* GstControlSource for this property */
  GValue cur_value;
  gdouble last_value;

  GstControlBindingDirectConvert convert;

  gpointer _gst_reserved[GST_PADDING];
};

/**
 * GstControlBindingDirectClass:
 * @parent_class: Parent class
 * @convert: Class method to convert control-values
 *
 * The class structure of #GstControlBindingDirect.
 */

struct _GstControlBindingDirectClass
{
  GstControlBindingClass parent_class;

  /*< private >*/
  gpointer _gst_reserved[GST_PADDING];
};

GType gst_control_binding_direct_get_type (void);

/* Functions */

GstControlBinding * gst_control_binding_direct_new (GstObject * object, const gchar * property_name,
                                                    GstControlSource * cs);
G_END_DECLS

#endif /* __GST_CONTROL_BINDING_DIRECT_H__ */