/**
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package org.apache.arrow.vector.ipc.message;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.List;

import org.apache.arrow.flatbuf.Buffer;
import org.apache.arrow.flatbuf.DictionaryBatch;
import org.apache.arrow.flatbuf.FieldNode;
import org.apache.arrow.flatbuf.Message;
import org.apache.arrow.flatbuf.MessageHeader;
import org.apache.arrow.flatbuf.MetadataVersion;
import org.apache.arrow.flatbuf.RecordBatch;
import org.apache.arrow.memory.BufferAllocator;
import org.apache.arrow.vector.ipc.ReadChannel;
import org.apache.arrow.vector.ipc.WriteChannel;
import org.apache.arrow.vector.types.pojo.Schema;

import com.google.flatbuffers.FlatBufferBuilder;

import siren.io.netty.buffer.ArrowBuf;

/**
 * Utility class for serializing Messages. Messages are all serialized a similar way.
 * 1. 4 byte little endian message header prefix
 * 2. FB serialized Message: This includes it the body length, which is the serialized
 * body and the type of the message.
 * 3. Serialized message.
 *
 * For schema messages, the serialization is simply the FB serialized Schema.
 *
 * For RecordBatch messages the serialization is:
 * 1. 4 byte little endian batch metadata header
 * 2. FB serialized RowBatch
 * 3. Padding to align to 8 byte boundary.
 * 4. serialized RowBatch buffers.
 */
public class MessageSerializer {

  public static int bytesToInt(byte[] bytes) {
    return ((bytes[3] & 255) << 24) +
        ((bytes[2] & 255) << 16) +
        ((bytes[1] & 255) << 8) +
        ((bytes[0] & 255) << 0);
  }

  /**
   * Serialize a schema object.
   *
   * @param out    where to write the schema
   * @param schema the object to serialize to out
   * @return the resulting size of the serialized schema
   * @throws IOException if something went wrong
   */
  public static long serialize(WriteChannel out, Schema schema) throws IOException {
    long start = out.getCurrentPosition();
    assert start % 8 == 0;

    FlatBufferBuilder builder = new FlatBufferBuilder();
    int schemaOffset = schema.getSchema(builder);
    ByteBuffer serializedMessage = serializeMessage(builder, MessageHeader.Schema, schemaOffset, 0);

    int size = serializedMessage.remaining();
    // ensure that message aligns to 8 byte padding - 4 bytes for size, then message body
    if ((size + 4) % 8 != 0) {
      size += 8 - (size + 4) % 8;
    }

    out.writeIntLittleEndian(size);
    out.write(serializedMessage);
    out.align(); // any bytes written are already captured by our size modification above

    assert (size + 4) % 8 == 0;
    return size + 4;
  }

  /**
   * Deserializes a schema object. Format is from serialize().
   *
   * @param reader the reader interface to deserialize from
   * @return the deserialized object
   * @throws IOException if something went wrong
   */
  public static Schema deserializeSchema(MessageReader reader) throws IOException {
    Message message = reader.readNextMessage();
    if (message == null) {
      throw new IOException("Unexpected end of input. Missing schema.");
    }
    if (message.headerType() != MessageHeader.Schema) {
      throw new IOException("Expected schema but header was " + message.headerType());
    }

    return Schema.convertSchema((org.apache.arrow.flatbuf.Schema)
        message.header(new org.apache.arrow.flatbuf.Schema()));
  }

  /**
   * Deserializes a schema object. Format is from serialize().
   *
   * @param in the channel to deserialize from
   * @return the deserialized object
   * @throws IOException if something went wrong
   */
  public static Schema deserializeSchema(ReadChannel in) throws IOException {
    return deserializeSchema(new MessageChannelReader(in));
  }

  /**
   * Serializes an ArrowRecordBatch. Returns the offset and length of the written batch.
   *
   * @param out   where to write the batch
   * @param batch the object to serialize to out
   * @return the serialized block metadata
   * @throws IOException if something went wrong
   */
  public static ArrowBlock serialize(WriteChannel out, ArrowRecordBatch batch)
      throws IOException {

    long start = out.getCurrentPosition();
    int bodyLength = batch.computeBodyLength();
    assert bodyLength % 8 == 0;

    FlatBufferBuilder builder = new FlatBufferBuilder();
    int batchOffset = batch.writeTo(builder);

    ByteBuffer serializedMessage = serializeMessage(builder, MessageHeader.RecordBatch, batchOffset, bodyLength);

    int metadataLength = serializedMessage.remaining();

    // calculate alignment bytes so that metadata length points to the correct location after alignment
    int padding = (int) ((start + metadataLength + 4) % 8);
    if (padding != 0) {
      metadataLength += (8 - padding);
    }

    out.writeIntLittleEndian(metadataLength);
    out.write(serializedMessage);

    // Align the output to 8 byte boundary.
    out.align();

    long bufferLength = writeBatchBuffers(out, batch);
    assert bufferLength % 8 == 0;

    // Metadata size in the Block account for the size prefix
    return new ArrowBlock(start, metadataLength + 4, bufferLength);
  }

  public static long writeBatchBuffers(WriteChannel out, ArrowRecordBatch batch) throws IOException {
    long bufferStart = out.getCurrentPosition();
    List<ArrowBuf> buffers = batch.getBuffers();
    List<ArrowBuffer> buffersLayout = batch.getBuffersLayout();

    for (int i = 0; i < buffers.size(); i++) {
      ArrowBuf buffer = buffers.get(i);
      ArrowBuffer layout = buffersLayout.get(i);
      long startPosition = bufferStart + layout.getOffset();
      if (startPosition != out.getCurrentPosition()) {
        out.writeZeros((int) (startPosition - out.getCurrentPosition()));
      }
      out.write(buffer);
      if (out.getCurrentPosition() != startPosition + layout.getSize()) {
        throw new IllegalStateException("wrong buffer size: " + out.getCurrentPosition() +
            " != " + startPosition + layout.getSize());
      }
    }
    out.align();
    return out.getCurrentPosition() - bufferStart;
  }

  /**
   * Deserializes a RecordBatch.
   *
   * @param reader  the reader interface to deserialize from
   * @param message the object to derialize to
   * @param alloc   to allocate buffers
   * @return the deserialized object
   * @throws IOException if something went wrong
   */
  public static ArrowRecordBatch deserializeRecordBatch(MessageReader reader, Message message, BufferAllocator alloc)
      throws IOException {
    RecordBatch recordBatchFB = (RecordBatch) message.header(new RecordBatch());

    // Now read the record batch body
    ArrowBuf buffer = reader.readMessageBody(message, alloc);
    return deserializeRecordBatch(recordBatchFB, buffer);
  }

  /**
   * Deserializes a RecordBatch knowing the size of the entire message up front. This
   * minimizes the number of reads to the underlying stream.
   *
   * @param in    the channel to deserialize from
   * @param block the object to derialize to
   * @param alloc to allocate buffers
   * @return the deserialized object
   * @throws IOException if something went wrong
   */
  public static ArrowRecordBatch deserializeRecordBatch(ReadChannel in, ArrowBlock block,
                                                        BufferAllocator alloc) throws IOException {
    // Metadata length contains integer prefix plus byte padding
    long totalLen = block.getMetadataLength() + block.getBodyLength();

    if (totalLen > Integer.MAX_VALUE) {
      throw new IOException("Cannot currently deserialize record batches over 2GB");
    }

    ArrowBuf buffer = alloc.buffer((int) totalLen);
    if (in.readFully(buffer, (int) totalLen) != totalLen) {
      throw new IOException("Unexpected end of input trying to read batch.");
    }

    ArrowBuf metadataBuffer = buffer.slice(4, block.getMetadataLength() - 4);

    Message messageFB =
        Message.getRootAsMessage(metadataBuffer.nioBuffer().asReadOnlyBuffer());

    RecordBatch recordBatchFB = (RecordBatch) messageFB.header(new RecordBatch());

    // Now read the body
    final ArrowBuf body = buffer.slice(block.getMetadataLength(),
        (int) totalLen - block.getMetadataLength());
    return deserializeRecordBatch(recordBatchFB, body);
  }

  /**
   * Deserializes a record batch given the Flatbuffer metadata and in-memory body.
   *
   * @param recordBatchFB Deserialized FlatBuffer record batch
   * @param body Read body of the record batch
   * @return ArrowRecordBatch from metadata and in-memory body
   * @throws IOException
   */
  public static ArrowRecordBatch deserializeRecordBatch(RecordBatch recordBatchFB,
                                                        ArrowBuf body) throws IOException {
    // Now read the body
    int nodesLength = recordBatchFB.nodesLength();
    List<ArrowFieldNode> nodes = new ArrayList<>();
    for (int i = 0; i < nodesLength; ++i) {
      FieldNode node = recordBatchFB.nodes(i);
      if ((int) node.length() != node.length() ||
          (int) node.nullCount() != node.nullCount()) {
        throw new IOException("Cannot currently deserialize record batches with " +
            "node length larger than Int.MAX_VALUE");
      }
      nodes.add(new ArrowFieldNode((int) node.length(), (int) node.nullCount()));
    }
    List<ArrowBuf> buffers = new ArrayList<>();
    for (int i = 0; i < recordBatchFB.buffersLength(); ++i) {
      Buffer bufferFB = recordBatchFB.buffers(i);
      ArrowBuf vectorBuffer = body.slice((int) bufferFB.offset(), (int) bufferFB.length());
      buffers.add(vectorBuffer);
    }
    if ((int) recordBatchFB.length() != recordBatchFB.length()) {
      throw new IOException("Cannot currently deserialize record batches over 2GB");
    }
    ArrowRecordBatch arrowRecordBatch =
        new ArrowRecordBatch((int) recordBatchFB.length(), nodes, buffers);
    body.release();
    return arrowRecordBatch;
  }

  /**
   * Serializes a dictionary ArrowRecordBatch. Returns the offset and length of the written batch.
   *
   * @param out   where to serialize
   * @param batch the batch to serialize
   * @return the metadata of the serialized block
   * @throws IOException if something went wrong
   */
  public static ArrowBlock serialize(WriteChannel out, ArrowDictionaryBatch batch) throws IOException {
    long start = out.getCurrentPosition();
    int bodyLength = batch.computeBodyLength();
    assert bodyLength % 8 == 0;

    FlatBufferBuilder builder = new FlatBufferBuilder();
    int batchOffset = batch.writeTo(builder);

    ByteBuffer serializedMessage = serializeMessage(builder, MessageHeader.DictionaryBatch, batchOffset, bodyLength);

    int metadataLength = serializedMessage.remaining();

    // calculate alignment bytes so that metadata length points to the correct location after alignment
    int padding = (int) ((start + metadataLength + 4) % 8);
    if (padding != 0) {
      metadataLength += (8 - padding);
    }

    out.writeIntLittleEndian(metadataLength);
    out.write(serializedMessage);

    // Align the output to 8 byte boundary.
    out.align();

    // write the embedded record batch
    long bufferLength = writeBatchBuffers(out, batch.getDictionary());
    assert bufferLength % 8 == 0;

    // Metadata size in the Block account for the size prefix
    return new ArrowBlock(start, metadataLength + 4, bufferLength);
  }

  /**
   * Deserializes a DictionaryBatch.
   *
   * @param reader  where to read from
   * @param message the message message metadata to deserialize
   * @param alloc   the allocator for new buffers
   * @return the corresponding dictionary batch
   * @throws IOException if something went wrong
   */
  public static ArrowDictionaryBatch deserializeDictionaryBatch(MessageReader reader,
                                                                Message message,
                                                                BufferAllocator alloc) throws IOException {
    DictionaryBatch dictionaryBatchFB = (DictionaryBatch) message.header(new DictionaryBatch());

    // Now read the record batch body
    ArrowBuf body = reader.readMessageBody(message, alloc);
    ArrowRecordBatch recordBatch = deserializeRecordBatch(dictionaryBatchFB.data(), body);
    return new ArrowDictionaryBatch(dictionaryBatchFB.id(), recordBatch);
  }

  /**
   * Deserializes a DictionaryBatch knowing the size of the entire message up front. This
   * minimizes the number of reads to the underlying stream.
   *
   * @param in    where to read from
   * @param block block metadata for deserializing
   * @param alloc to allocate new buffers
   * @return the corresponding dictionary
   * @throws IOException if something went wrong
   */
  public static ArrowDictionaryBatch deserializeDictionaryBatch(ReadChannel in,
                                                                ArrowBlock block,
                                                                BufferAllocator alloc) throws IOException {
    // Metadata length contains integer prefix plus byte padding
    long totalLen = block.getMetadataLength() + block.getBodyLength();

    if (totalLen > Integer.MAX_VALUE) {
      throw new IOException("Cannot currently deserialize record batches over 2GB");
    }

    ArrowBuf buffer = alloc.buffer((int) totalLen);
    if (in.readFully(buffer, (int) totalLen) != totalLen) {
      throw new IOException("Unexpected end of input trying to read batch.");
    }

    ArrowBuf metadataBuffer = buffer.slice(4, block.getMetadataLength() - 4);

    Message messageFB =
        Message.getRootAsMessage(metadataBuffer.nioBuffer().asReadOnlyBuffer());

    DictionaryBatch dictionaryBatchFB = (DictionaryBatch) messageFB.header(new DictionaryBatch());

    // Now read the body
    final ArrowBuf body = buffer.slice(block.getMetadataLength(),
        (int) totalLen - block.getMetadataLength());
    ArrowRecordBatch recordBatch = deserializeRecordBatch(dictionaryBatchFB.data(), body);
    return new ArrowDictionaryBatch(dictionaryBatchFB.id(), recordBatch);
  }

  /**
   * Deserialize a message that is either an ArrowDictionaryBatch or ArrowRecordBatch.
   *
   * @param reader Interface to read messages from
   * @param alloc Allocator for message data
   * @return The deserialized record batch
   * @throws IOException if the message is not an ArrowDictionaryBatch or ArrowRecordBatch
   */
  public static ArrowMessage deserializeMessageBatch(MessageReader reader, BufferAllocator alloc) throws IOException {
    Message message = reader.readNextMessage();
    if (message == null) {
      return null;
    } else if (message.bodyLength() > Integer.MAX_VALUE) {
      throw new IOException("Cannot currently deserialize record batches over 2GB");
    }

    if (message.version() != MetadataVersion.V4) {
      throw new IOException("Received metadata with an incompatible version number");
    }

    switch (message.headerType()) {
      case MessageHeader.RecordBatch:
        return deserializeRecordBatch(reader, message, alloc);
      case MessageHeader.DictionaryBatch:
        return deserializeDictionaryBatch(reader, message, alloc);
      default:
        throw new IOException("Unexpected message header type " + message.headerType());
    }
  }

  /**
   * Deserialize a message that is either an ArrowDictionaryBatch or ArrowRecordBatch.
   *
   * @param in ReadChannel to read messages from
   * @param alloc Allocator for message data
   * @return The deserialized record batch
   * @throws IOException if the message is not an ArrowDictionaryBatch or ArrowRecordBatch
   */
  public static ArrowMessage deserializeMessageBatch(ReadChannel in, BufferAllocator alloc) throws IOException {
    return deserializeMessageBatch(new MessageChannelReader(in), alloc);
  }

  /**
   * Serializes a message header.
   *
   * @param builder      to write the flatbuf to
   * @param headerType   headerType field
   * @param headerOffset header offset field
   * @param bodyLength   body length field
   * @return the corresponding ByteBuffer
   */
  public static ByteBuffer serializeMessage(FlatBufferBuilder builder, byte headerType,
                                            int headerOffset, int bodyLength) {
    Message.startMessage(builder);
    Message.addHeaderType(builder, headerType);
    Message.addHeader(builder, headerOffset);
    Message.addVersion(builder, MetadataVersion.V4);
    Message.addBodyLength(builder, bodyLength);
    builder.finish(Message.endMessage(builder));
    return builder.dataBuffer();
  }

  private static Message deserializeMessage(ReadChannel in) throws IOException {
    // Read the message size. There is an i32 little endian prefix.
    ByteBuffer buffer = ByteBuffer.allocate(4);
    if (in.readFully(buffer) != 4) {
      return null;
    }
    int messageLength = bytesToInt(buffer.array());
    if (messageLength == 0) {
      return null;
    }

    buffer = ByteBuffer.allocate(messageLength);
    if (in.readFully(buffer) != messageLength) {
      throw new IOException(
          "Unexpected end of stream trying to read message.");
    }
    buffer.rewind();

    return Message.getRootAsMessage(buffer);
  }
}
