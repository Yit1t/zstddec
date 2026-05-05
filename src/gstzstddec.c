/*
 * GStreamer
 * Copyright (C) 2005 Thomas Vander Stichele <thomas@apestaart.org>
 * Copyright (C) 2005 Ronald S. Bultje <rbultje@ronald.bitfreak.net>
 * Copyright (C) 2026  <<user@hostname.org>>
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Alternatively, the contents of this file may be used under the
 * GNU Lesser General Public License Version 2.1 (the "LGPL"), in
 * which case the following provisions apply instead of the ones
 * mentioned above:
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

/**
 * SECTION:element-zstddec
 *
 * FIXME:Describe zstddec here.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v -m fakesrc ! zstddec ! fakesink silent=TRUE
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/gst.h>
#include <gst/base/gsttypefindhelper.h>
#include <zstd.h>
#include <string.h>
#include "gstzstddec.h"


GST_DEBUG_CATEGORY_STATIC (gst_zstddec_debug);
#define GST_CAT_DEFAULT gst_zstddec_debug
#define DEFAULT_FIRST_BUFFER_SIZE 1024
#define DEFAULT_BUFFER_SIZE 1024

/* Filter signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_BUFFER_SIZE,
  PROP_FIRST_BUFFER_SIZE
};




/* the capabilities of the inputs and outputs.
 *
 * describe the real formats here.
 */
static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/zstd")
    );

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("ANY")
    );

#define gst_zstddec_parent_class parent_class
G_DEFINE_TYPE (Gstzstddec, gst_zstddec, GST_TYPE_ELEMENT);

GST_ELEMENT_REGISTER_DEFINE (zstddec, "zstddec", GST_RANK_NONE,
    GST_TYPE_ZSTDDEC);

/* Forward Declarations */    
static void gst_zstddec_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_zstddec_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);

static gboolean gst_zstddec_sink_event (GstPad * pad,
    GstObject * parent, GstEvent * event);
static GstFlowReturn gst_zstddec_chain (GstPad * pad,
    GstObject * parent, GstBuffer * buf);

static void gst_zstddec_finalize (GObject *object);
static GstStateChangeReturn gst_zstddec_change_state (GstElement *element, GstStateChange transition);


/* decompress end function
 * this function ends decompression and frees the decompressor
 */
static void
gst_zstddec_decompress_end (Gstzstddec *dec)
{
  if (dec -> ready) /* Only release decompressor if it's actually initialed*/
  {
    ZSTD_freeDCtx (dec->dctx);
    dec->dctx = NULL;
    dec->ready = FALSE;
  }
}

/* decompress initial function
 * this function initial decompressor
 */
static void
gst_zstddec_decompress_init (Gstzstddec *dec)
{
  /* Clean up old state*/
  gst_zstddec_decompress_end(dec);

  /* Create decompressor and check if creation succeeded*/
  dec->dctx = ZSTD_createDCtx();
  if (dec->dctx == NULL) /* ZSTD_createDCtx() return NULL when memory is not enough */
  {
    /* Creation can fail if system is out of memory */
    dec->ready = FALSE;
    GST_ELEMENT_ERROR (dec, CORE, FAILED, (NULL),
          ("Failed to create and initialize ZSTD decompression stream."));
    return;
  }

  /* Set ready and offset*/
  dec->ready = TRUE;
  dec->offset = 0; /* 0 means no output */

}


/* GObject vmethod implementations */

/* initialize the zstddec's class */
static void
gst_zstddec_class_init (GstzstddecClass * klass)
{
  /* Cast to parent class types to register callbacks at different layers */
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  
  /* Register callback functions */
  gobject_class->set_property = gst_zstddec_set_property;
  gobject_class->get_property = gst_zstddec_get_property;
  gobject_class->finalize = gst_zstddec_finalize;
  gstelement_class->change_state = gst_zstddec_change_state; 

  /* Set first buffer size and buffer size properties */  
  g_object_class_install_property (gobject_class, PROP_BUFFER_SIZE,
      g_param_spec_uint ("buffer-size", "Buffer Size", "Output buffer size for decompressed data",
          1, G_MAXUINT, DEFAULT_BUFFER_SIZE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  
  g_object_class_install_property (gobject_class, PROP_FIRST_BUFFER_SIZE,
      g_param_spec_uint ("first-buffer-size", "First Buffer Size", "Output first buffer size for decompressed data",
          1, G_MAXUINT, DEFAULT_FIRST_BUFFER_SIZE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  
  /* Set basic info of this plugin */
  gst_element_class_set_details_simple (gstelement_class,
      "zstddec",
      "Codec/Decoder",
      "Decodes zstd compressed streams", "Yitong Ren <ren1t@outlook.com>");
  
  /* Add pad to the plugin with the pad template*/  
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&src_factory));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&sink_factory));
  
  /* Pair with GST_DEBUG_CATEGORY_STATIC at file top */
  GST_DEBUG_CATEGORY_INIT (gst_zstddec_debug, "zstddec", 0, "Zstd decompressor");
}

/* initialize the new element
 * instantiate pads and add them to element
 * set pad callback functions
 * initialize instance structure
 */
static void
gst_zstddec_init (Gstzstddec * dec)
{
  /* Create and configue sink pad for GStreamer to use */
  dec->sinkpad = gst_pad_new_from_static_template (&sink_factory, "sink");

  /* Tell GStreamer which functions handle incoming events and data on this pad*/
  gst_pad_set_event_function (dec->sinkpad,
      GST_DEBUG_FUNCPTR (gst_zstddec_sink_event));
  gst_pad_set_chain_function (dec->sinkpad,
      GST_DEBUG_FUNCPTR (gst_zstddec_chain));
  gst_element_add_pad (GST_ELEMENT (dec), dec->sinkpad);
  
  /* Create source pad for GStreamer to use */
  dec->srcpad = gst_pad_new_from_static_template (&src_factory, "src");
  gst_element_add_pad (GST_ELEMENT (dec), dec->srcpad);
  
  /* Set default property values */
  dec->first_buffer_size = DEFAULT_FIRST_BUFFER_SIZE;
  dec->buffer_size = DEFAULT_BUFFER_SIZE;

  /* Initialize decompressor */
  gst_zstddec_decompress_init (dec);
}




/* Finalize function
 * Finalize the whole process by freeing the decompressor 
 * and letting the parent class clean up all the remaining resources
 */
static void
gst_zstddec_finalize(GObject *object)
{
  Gstzstddec *dec = GST_ZSTDDEC(object);

  GST_DEBUG_OBJECT(dec, "Finalize decompressor");
  gst_zstddec_decompress_end(dec);

  G_OBJECT_CLASS (parent_class)->finalize(object);
}

/* State changing function
 * Reset the compressor when transitioning from paused to ready.
 */
static GstStateChangeReturn
gst_zstddec_change_state (GstElement *element, GstStateChange transition)
{
  Gstzstddec *dec = GST_ZSTDDEC(element);
  GstStateChangeReturn ret;
  GST_DEBUG_OBJECT (dec, "Changing zstddec state");
  ret = GST_ELEMENT_CLASS (parent_class)->change_state(element, transition);
  if (ret != GST_STATE_CHANGE_SUCCESS)
    return ret;
  
  switch (transition)
  {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      gst_zstddec_decompress_init(dec);
      break;
    default:
      break;
  }
  return ret;
}

static void
gst_zstddec_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  Gstzstddec *dec = GST_ZSTDDEC (object);

  switch (prop_id) {
    case PROP_BUFFER_SIZE:
      dec->buffer_size = g_value_get_uint (value);
      GST_DEBUG_OBJECT (dec, "Set buffer size to: %d",dec->buffer_size);
      break;

    case PROP_FIRST_BUFFER_SIZE:
      dec->first_buffer_size = g_value_get_uint (value);
      GST_DEBUG_OBJECT (dec, "Set first buffer size to: %d",dec->first_buffer_size);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_zstddec_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  Gstzstddec *dec = GST_ZSTDDEC (object);

  switch (prop_id) {
    case PROP_BUFFER_SIZE:
      g_value_set_uint(value, dec->buffer_size);
      GST_DEBUG_OBJECT(dec, "Buffer size is: %d", dec->buffer_size);
      break;
    case PROP_FIRST_BUFFER_SIZE:
      g_value_set_uint(value, dec->first_buffer_size);
      GST_DEBUG_OBJECT(dec, "First buffer size is: %d", dec->first_buffer_size);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/* GstElement vmethod implementations */

/* this function handles sink events */
static gboolean
gst_zstddec_sink_event (GstPad * pad, GstObject * parent,
    GstEvent * event)
{
  Gstzstddec *dec;
  gboolean ret;

  dec = GST_ZSTDDEC (parent);

  GST_LOG_OBJECT (dec, "Received %s event: %" GST_PTR_FORMAT,
      GST_EVENT_TYPE_NAME (event), event);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CAPS:
    {
      GstCaps *caps;

      gst_event_parse_caps (event, &caps);
      /* do something with the caps */

      /* and forward */
      ret = gst_pad_event_default (pad, parent, event);
      break;
    }
    default:
      ret = gst_pad_event_default (pad, parent, event);
      break;
  }
  return ret;
}




/* chain function
 * this function does the actual processing
 */
static GstFlowReturn
gst_zstddec_chain (GstPad * pad, GstObject * parent, GstBuffer * buf)
{
  Gstzstddec *dec = GST_ZSTDDEC (parent);
  GstFlowReturn flow = GST_FLOW_OK;
  GstMapInfo inmap;

  /* Check if decompressor is ready */
  if (dec->ready == FALSE)
  {
    GST_ELEMENT_ERROR (dec, LIBRARY, FAILED, (NULL),("Decompressor not ready."));
    gst_buffer_unref (buf);
    return GST_FLOW_ERROR;

  }
  /* Map input buffer */
  gst_buffer_map (buf, &inmap, GST_MAP_READ);
  ZSTD_inBuffer input = {inmap.data, inmap.size, 0};
  
  while (input.pos < input.size)
  {
    GstBuffer *out;
    GstMapInfo outmap;
    size_t ret;
    guint have;

    /* Allocate output buffer*/
    out = gst_buffer_new_and_alloc (dec->offset ? dec->buffer_size : dec->first_buffer_size);
    gst_buffer_map (out, &outmap, GST_MAP_WRITE);

    /* Decompress */
    ZSTD_outBuffer output = { outmap.data, outmap.size,0};
    ret = ZSTD_decompressStream (dec->dctx, &output, &input);
    gst_buffer_unmap (out, &outmap);

    /* Check for errors */
    if (ZSTD_isError (ret))
    {
      GST_ELEMENT_ERROR (dec, STREAM, DECODE, (NULL),
          ("Failed to decompress: %s", ZSTD_getErrorName (ret)));
      gst_buffer_unref (out);
      gst_zstddec_decompress_init (dec);
      flow = GST_FLOW_ERROR;
      break;
    }

    /* No output produced */
    if (output.pos == 0)
    {
      gst_buffer_unref (out);
      break;
    }

    /* Resize output buffer to actual size */
    gst_buffer_resize (out, 0, output.pos);
    GST_BUFFER_OFFSET (out) = dec->offset;

    /* Detect content type at the first output */
    if (!dec->offset)
    {
      GstCaps *caps = NULL;
      caps = gst_type_find_helper_for_buffer (GST_OBJECT (dec), out, NULL);
      if (caps)
      {
        
        gst_pad_set_caps (dec->srcpad, caps);
        gst_caps_unref (caps); 
      }
    }
    /* Push to source pad*/
    have = output.pos;
    flow = gst_pad_push (dec->srcpad, out);
    if (flow != GST_FLOW_OK)
    {
      break;
    }
    dec->offset += have;
  }
  /* Cleanup and return */
  gst_buffer_unmap (buf, &inmap);
  gst_buffer_unref (buf);
  return flow;
}


/* entry point to initialize the plug-in
 * initialize the plug-in itself
 * register the element factories and other features
 */
static gboolean
zstddec_init (GstPlugin * zstddec)
{
  GST_DEBUG_CATEGORY_INIT (gst_zstddec_debug, "zstddec",
      0, "A zstandard file decompressor");

  return GST_ELEMENT_REGISTER (zstddec, zstddec);
}

/* PACKAGE: this is usually set by meson depending on some _INIT macro
 * in meson.build and then written into and defined in config.h, but we can
 * just set it ourselves here in case someone doesn't use meson to
 * compile this code. GST_PLUGIN_DEFINE needs PACKAGE to be defined.
 */
#ifndef PACKAGE
#define PACKAGE "myfirstzstddec"
#endif

/* gstreamer looks for this structure to register zstddecs */
GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    zstddec,
    "zstddec",
    zstddec_init,
    PACKAGE_VERSION, GST_LICENSE, GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
