/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <arrow/io/memory.h>
#include <arrow/ipc/writer.h>

#include <arrow-glib/buffer.hpp>
#include <arrow-glib/error.hpp>
#include <arrow-glib/file.hpp>
#include <arrow-glib/output-stream.hpp>
#include <arrow-glib/tensor.hpp>
#include <arrow-glib/writeable.hpp>

#include <iostream>
#include <sstream>

G_BEGIN_DECLS

/**
 * SECTION: output-stream
 * @section_id: output-stream-classes
 * @title: Output stream classes
 * @include: arrow-glib/arrow-glib.h
 *
 * #GArrowOutputStream is an interface for stream output. Stream
 * output is file based and writeable
 *
 * #GArrowFileOutputStream is a class for file output stream.
 *
 * #GArrowBufferOutputStream is a class for buffer output stream.
 *
 * #GArrowGIOOutputStream is a class for `GOutputStream` based output
 * stream.
 */

typedef struct GArrowOutputStreamPrivate_ {
  std::shared_ptr<arrow::io::OutputStream> output_stream;
} GArrowOutputStreamPrivate;

enum {
  PROP_0,
  PROP_OUTPUT_STREAM
};

static std::shared_ptr<arrow::io::FileInterface>
garrow_output_stream_get_raw_file_interface(GArrowFile *file)
{
  auto output_stream = GARROW_OUTPUT_STREAM(file);
  auto arrow_output_stream = garrow_output_stream_get_raw(output_stream);
  return arrow_output_stream;
}

static void
garrow_output_stream_file_interface_init(GArrowFileInterface *iface)
{
  iface->get_raw = garrow_output_stream_get_raw_file_interface;
}

static std::shared_ptr<arrow::io::Writable>
garrow_output_stream_get_raw_writeable_interface(GArrowWriteable *writeable)
{
  auto output_stream = GARROW_OUTPUT_STREAM(writeable);
  auto arrow_output_stream = garrow_output_stream_get_raw(output_stream);
  return arrow_output_stream;
}

static void
garrow_output_stream_writeable_interface_init(GArrowWriteableInterface *iface)
{
  iface->get_raw = garrow_output_stream_get_raw_writeable_interface;
}

G_DEFINE_TYPE_WITH_CODE(GArrowOutputStream,
                        garrow_output_stream,
                        G_TYPE_OBJECT,
                        G_ADD_PRIVATE(GArrowOutputStream)
                        G_IMPLEMENT_INTERFACE(GARROW_TYPE_FILE,
                                              garrow_output_stream_file_interface_init)
                        G_IMPLEMENT_INTERFACE(GARROW_TYPE_WRITEABLE,
                                              garrow_output_stream_writeable_interface_init));

#define GARROW_OUTPUT_STREAM_GET_PRIVATE(obj)                   \
  (G_TYPE_INSTANCE_GET_PRIVATE((obj),                           \
                               GARROW_TYPE_OUTPUT_STREAM,       \
                               GArrowOutputStreamPrivate))

static void
garrow_output_stream_finalize(GObject *object)
{
  GArrowOutputStreamPrivate *priv;

  priv = GARROW_OUTPUT_STREAM_GET_PRIVATE(object);

  priv->output_stream = nullptr;

  G_OBJECT_CLASS(garrow_output_stream_parent_class)->finalize(object);
}

static void
garrow_output_stream_set_property(GObject *object,
                                          guint prop_id,
                                          const GValue *value,
                                          GParamSpec *pspec)
{
  GArrowOutputStreamPrivate *priv;

  priv = GARROW_OUTPUT_STREAM_GET_PRIVATE(object);

  switch (prop_id) {
  case PROP_OUTPUT_STREAM:
    priv->output_stream =
      *static_cast<std::shared_ptr<arrow::io::OutputStream> *>(g_value_get_pointer(value));
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    break;
  }
}

static void
garrow_output_stream_get_property(GObject *object,
                                          guint prop_id,
                                          GValue *value,
                                          GParamSpec *pspec)
{
  switch (prop_id) {
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    break;
  }
}

static void
garrow_output_stream_init(GArrowOutputStream *object)
{
}

static void
garrow_output_stream_class_init(GArrowOutputStreamClass *klass)
{
  GObjectClass *gobject_class;
  GParamSpec *spec;

  gobject_class = G_OBJECT_CLASS(klass);

  gobject_class->finalize     = garrow_output_stream_finalize;
  gobject_class->set_property = garrow_output_stream_set_property;
  gobject_class->get_property = garrow_output_stream_get_property;

  spec = g_param_spec_pointer("output-stream",
                              "io::OutputStream",
                              "The raw std::shared<arrow::io::OutputStream> *",
                              static_cast<GParamFlags>(G_PARAM_WRITABLE |
                                                       G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property(gobject_class, PROP_OUTPUT_STREAM, spec);
}

/**
 * garrow_output_stream_write_tensor:
 * @stream: A #GArrowWriteable.
 * @tensor: A #GArrowTensor to be written.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Returns: The number of written bytes on success, -1 on error.
 *
 * Since: 0.4.0
 */
gint64
garrow_output_stream_write_tensor(GArrowOutputStream *stream,
                                  GArrowTensor *tensor,
                                  GError **error)
{
  auto arrow_tensor = garrow_tensor_get_raw(tensor);
  auto arrow_stream = garrow_output_stream_get_raw(stream);
  int32_t metadata_length;
  int64_t body_length;
  auto status = arrow::ipc::WriteTensor(*arrow_tensor,
                                        arrow_stream.get(),
                                        &metadata_length,
                                        &body_length);
  if (garrow_error_check(error, status, "[output-stream][write-tensor]")) {
    return metadata_length + body_length;
  } else {
    return -1;
  }
}


G_DEFINE_TYPE(GArrowFileOutputStream,
              garrow_file_output_stream,
              GARROW_TYPE_OUTPUT_STREAM);

static void
garrow_file_output_stream_init(GArrowFileOutputStream *file_output_stream)
{
}

static void
garrow_file_output_stream_class_init(GArrowFileOutputStreamClass *klass)
{
}

/**
 * garrow_file_output_stream_new:
 * @path: The path of the file output stream.
 * @append: Whether the path is opened as append mode or recreate mode.
 * @error: (nullable): Return location for a #GError or %NULL.
 *
 * Returns: (nullable): A newly opened #GArrowFileOutputStream or
 *   %NULL on error.
 */
GArrowFileOutputStream *
garrow_file_output_stream_new(const gchar *path,
                              gboolean append,
                              GError **error)
{
  std::shared_ptr<arrow::io::FileOutputStream> arrow_file_output_stream;
  auto status =
    arrow::io::FileOutputStream::Open(std::string(path),
                                      append,
                                      &arrow_file_output_stream);
  if (status.ok()) {
    return garrow_file_output_stream_new_raw(&arrow_file_output_stream);
  } else {
    std::string context("[io][file-output-stream][open]: <");
    context += path;
    context += ">";
    garrow_error_check(error, status, context.c_str());
    return NULL;
  }
}


G_DEFINE_TYPE(GArrowBufferOutputStream,
              garrow_buffer_output_stream,
              GARROW_TYPE_OUTPUT_STREAM);

static void
garrow_buffer_output_stream_init(GArrowBufferOutputStream *buffer_output_stream)
{
}

static void
garrow_buffer_output_stream_class_init(GArrowBufferOutputStreamClass *klass)
{
}

/**
 * garrow_buffer_output_stream_new:
 * @buffer: The resizable buffer to be output.
 *
 * Returns: (transfer full): A newly created #GArrowBufferOutputStream.
 */
GArrowBufferOutputStream *
garrow_buffer_output_stream_new(GArrowResizableBuffer *buffer)
{
  auto arrow_buffer = garrow_buffer_get_raw(GARROW_BUFFER(buffer));
  auto arrow_resizable_buffer =
    std::static_pointer_cast<arrow::ResizableBuffer>(arrow_buffer);
  auto arrow_buffer_output_stream =
    std::make_shared<arrow::io::BufferOutputStream>(arrow_resizable_buffer);
  return garrow_buffer_output_stream_new_raw(&arrow_buffer_output_stream);
}

G_END_DECLS


namespace garrow {
  class GIOOutputStream : public arrow::io::OutputStream {
  public:
    GIOOutputStream(GOutputStream *output_stream) :
      output_stream_(output_stream) {
      g_object_ref(output_stream_);
    }

    ~GIOOutputStream() {
      g_object_unref(output_stream_);
    }

    GOutputStream *get_output_stream() {
      return output_stream_;
    }

    arrow::Status Close() override {
      GError *error = NULL;
      if (g_output_stream_close(output_stream_, NULL, &error)) {
        return arrow::Status::OK();
      } else {
        return garrow_error_to_status(error,
                                      arrow::StatusCode::IOError,
                                      "[gio-output-stream][close]");
      }
    }

    arrow::Status Tell(int64_t *position) const override {
      if (!G_IS_SEEKABLE(output_stream_)) {
        std::string message("[gio-output-stream][tell] "
                            "not seekable output stream: <");
        message += G_OBJECT_CLASS_NAME(G_OBJECT_GET_CLASS(output_stream_));
        message += ">";
        return arrow::Status::NotImplemented(message);
      }

      *position = g_seekable_tell(G_SEEKABLE(output_stream_));
      return arrow::Status::OK();
    }

    arrow::Status Write(const void *data,
                        int64_t n_bytes) override {
      GError *error = NULL;
      gsize n_written_bytes;
      auto successed = g_output_stream_write_all(output_stream_,
                                                 data,
                                                 n_bytes,
                                                 &n_written_bytes,
                                                 NULL,
                                                 &error);
      if (successed) {
        return arrow::Status::OK();
      } else {
        std::stringstream message("[gio-output-stream][write]");
        message << "[" << n_written_bytes << "/" << n_bytes << "]";
        return garrow_error_to_status(error,
                                      arrow::StatusCode::IOError,
                                      message.str().c_str());
      }
    }

    arrow::Status Flush() override {
      GError *error = NULL;
      auto successed = g_output_stream_flush(output_stream_, NULL, &error);
      if (successed) {
        return arrow::Status::OK();
      } else {
        return garrow_error_to_status(error,
                                      arrow::StatusCode::IOError,
                                      "[gio-output-stream][flush]");
      }
    }

  private:
    GOutputStream *output_stream_;
  };
};

G_BEGIN_DECLS

G_DEFINE_TYPE(GArrowGIOOutputStream,
              garrow_gio_output_stream,
              GARROW_TYPE_OUTPUT_STREAM);

static void
garrow_gio_output_stream_init(GArrowGIOOutputStream *gio_output_stream)
{
}

static void
garrow_gio_output_stream_class_init(GArrowGIOOutputStreamClass *klass)
{
}

/**
 * garrow_gio_output_stream_new:
 * @gio_output_stream: The stream to be output.
 *
 * Returns: (transfer full): A newly created #GArrowGIOOutputStream.
 */
GArrowGIOOutputStream *
garrow_gio_output_stream_new(GOutputStream *gio_output_stream)
{
  auto arrow_output_stream =
    std::make_shared<garrow::GIOOutputStream>(gio_output_stream);
  auto object = g_object_new(GARROW_TYPE_GIO_OUTPUT_STREAM,
                             "output-stream", &arrow_output_stream,
                             NULL);
  auto output_stream = GARROW_GIO_OUTPUT_STREAM(object);
  return output_stream;
}

/**
 * garrow_gio_output_stream_get_raw:
 * @output_stream: A #GArrowGIOOutputStream.
 *
 * Returns: (transfer none): The wrapped #GOutputStream.
 *
 * Since: 0.5.0
 */
GOutputStream *
garrow_gio_output_stream_get_raw(GArrowGIOOutputStream *output_stream)
{
  auto arrow_output_stream =
    garrow_output_stream_get_raw(GARROW_OUTPUT_STREAM(output_stream));
  auto arrow_gio_output_stream =
    std::static_pointer_cast<garrow::GIOOutputStream>(arrow_output_stream);
  auto gio_output_stream = arrow_gio_output_stream->get_output_stream();
  return gio_output_stream;
}

G_END_DECLS


GArrowOutputStream *
garrow_output_stream_new_raw(std::shared_ptr<arrow::io::OutputStream> *arrow_output_stream)
{
  auto output_stream =
    GARROW_OUTPUT_STREAM(g_object_new(GARROW_TYPE_OUTPUT_STREAM,
                                      "output-stream", arrow_output_stream,
                                      NULL));
  return output_stream;
}

std::shared_ptr<arrow::io::OutputStream>
garrow_output_stream_get_raw(GArrowOutputStream *output_stream)
{
  GArrowOutputStreamPrivate *priv;

  priv = GARROW_OUTPUT_STREAM_GET_PRIVATE(output_stream);
  return priv->output_stream;
}


GArrowFileOutputStream *
garrow_file_output_stream_new_raw(std::shared_ptr<arrow::io::FileOutputStream> *arrow_file_output_stream)
{
  auto file_output_stream =
    GARROW_FILE_OUTPUT_STREAM(g_object_new(GARROW_TYPE_FILE_OUTPUT_STREAM,
                                           "output-stream", arrow_file_output_stream,
                                           NULL));
  return file_output_stream;
}

GArrowBufferOutputStream *
garrow_buffer_output_stream_new_raw(std::shared_ptr<arrow::io::BufferOutputStream> *arrow_buffer_output_stream)
{
  auto buffer_output_stream =
    GARROW_BUFFER_OUTPUT_STREAM(g_object_new(GARROW_TYPE_BUFFER_OUTPUT_STREAM,
                                             "output-stream", arrow_buffer_output_stream,
                                             NULL));
  return buffer_output_stream;
}
