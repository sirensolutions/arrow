/**
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 * <p>
 * http://www.apache.org/licenses/LICENSE-2.0
 * <p>
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package org.apache.arrow.vector;

import io.netty.buffer.ArrowBuf;
import org.apache.arrow.memory.BufferAllocator;
import org.apache.arrow.vector.ipc.message.ArrowFieldNode;

/**
 * Helper class for performing generic operations on a bit vector buffer.
 * External use of this class is not recommended.
 */
public class BitVectorHelper {

  /**
   * Get the index of byte corresponding to bit index in validity buffer
   */
  public static int byteIndex(int absoluteBitIndex) {
    return absoluteBitIndex >> 3;
  }

  /**
   * Get the relative index of bit within the byte in validity buffer
   */
  public static int bitIndex(int absoluteBitIndex) {
    return absoluteBitIndex & 7;
  }

  /**
   * Set the bit at provided index to 1.
   *
   * @param validityBuffer
   * @param index
   */
  public static void setValidityBitToOne(ArrowBuf validityBuffer, int index) {
    final int byteIndex = byteIndex(index);
    final int bitIndex = bitIndex(index);
    byte currentByte = validityBuffer.getByte(byteIndex);
    final byte bitMask = (byte) (1L << bitIndex);
    currentByte |= bitMask;
    validityBuffer.setByte(byteIndex, currentByte);
  }

  /**
   * Set the bit at a given index to provided value (1 or 0)
   *
   * @param validityBuffer
   * @param index
   * @param value
   */
  public static void setValidityBit(ArrowBuf validityBuffer, int index, int value) {
    final int byteIndex = byteIndex(index);
    final int bitIndex = bitIndex(index);
    byte currentByte = validityBuffer.getByte(byteIndex);
    final byte bitMask = (byte) (1L << bitIndex);
    if (value != 0) {
      currentByte |= bitMask;
    } else {
      currentByte -= (bitMask & currentByte);
    }
    validityBuffer.setByte(byteIndex, currentByte);
  }

  /**
   * Set the bit at a given index to provided value (1 or 0). Internally
   * takes care of allocating the buffer if the caller didn't do so.
   *
   * @param validityBuffer
   * @param allocator
   * @param valueCount
   * @param index
   * @param value
   * @return ArrowBuf
   */
  public static ArrowBuf setValidityBit(ArrowBuf validityBuffer, BufferAllocator allocator,
                                        int valueCount, int index, int value) {
    if (validityBuffer == null) {
      validityBuffer = allocator.buffer(getValidityBufferSize(valueCount));
    }
    setValidityBit(validityBuffer, index, value);
    if (index == (valueCount - 1)) {
      validityBuffer.writerIndex(getValidityBufferSize(valueCount));
    }

    return validityBuffer;
  }

  /**
   * Check if a bit at a given index is set or not.
   *
   * @param buffer
   * @param index
   * @return 1 if bit is set, 0 otherwise.
   */
  public static int get(final ArrowBuf buffer, int index) {
    final int byteIndex = index >> 3;
    final byte b = buffer.getByte(byteIndex);
    final int bitIndex = index & 7;
    return Long.bitCount(b & (1L << bitIndex));
  }

  /**
   * Compute the size of validity buffer required to manage a given number
   * of elements in a vector.
   *
   * @param valueCount
   * @return buffer size
   */
  public static int getValidityBufferSize(int valueCount) {
    return ((int) Math.ceil(valueCount / 8.0));
  }

  /**
   * Given a validity buffer, find the number of bits that are not set.
   * This is used to compute the number of null elements in a nullable vector.
   *
   * @param validityBuffer
   * @param valueCount
   * @return number of bits not set.
   */
  public static int getNullCount(final ArrowBuf validityBuffer, final int valueCount) {
    if (valueCount == 0) {
      return 0;
    }
    int count = 0;
    final int sizeInBytes = getValidityBufferSize(valueCount);

    for (int i = 0; i < sizeInBytes; ++i) {
      final byte byteValue = validityBuffer.getByte(i);
      /* Java uses two's complement binary representation, hence 11111111_b which is -1
       * when converted to Int will have 32bits set to 1. Masking the MSB and then
       * adding it back solves the issue.
       */
      count += Integer.bitCount(byteValue & 0x7F) - (byteValue >> 7);
    }
    int nullCount = (sizeInBytes * 8) - count;
    /* if the valueCount is not a multiple of 8,
     * the bits on the right were counted as null bits.
     */
    int remainder = valueCount % 8;
    nullCount -= remainder == 0 ? 0 : 8 - remainder;
    return nullCount;
  }

  public static byte getBitsFromCurrentByte(final ArrowBuf data, final int index, final int offset) {
    return (byte) ((data.getByte(index) & 0xFF) >>> offset);
  }

  public static byte getBitsFromNextByte(ArrowBuf data, int index, int offset) {
    return (byte) ((data.getByte(index) << (8 - offset)));
  }

  public static ArrowBuf loadValidityBuffer(final ArrowFieldNode fieldNode,
                                            final ArrowBuf sourceValidityBuffer,
                                            final BufferAllocator allocator) {
    final int valueCount = fieldNode.getLength();
    ArrowBuf newBuffer = null;
    /* either all NULLs or all non-NULLs */
    if (fieldNode.getNullCount() == 0 || fieldNode.getNullCount() == valueCount) {
      newBuffer = allocator.buffer(getValidityBufferSize(valueCount));
      newBuffer.setZero(0, newBuffer.capacity());
      if (fieldNode.getNullCount() != 0) {
        /* all NULLs */
        return newBuffer;
      }
      /* all non-NULLs */
      int fullBytesCount = valueCount / 8;
      for (int i = 0; i < fullBytesCount; ++i) {
        newBuffer.setByte(i, 0xFF);
      }
      int remainder = valueCount % 8;
      if (remainder > 0) {
        byte bitMask = (byte) (0xFFL >>> ((8 - remainder) & 7));
        newBuffer.setByte(fullBytesCount, bitMask);
      }
    } else {
      /* mixed byte pattern -- create another ArrowBuf associated with the
       * target allocator
       */
      newBuffer = sourceValidityBuffer.retain(allocator);
    }

    return newBuffer;
  }

  /**
   * Set the byte of the given index in the data buffer by applying a bit mask to
   * the current byte at that index.
   *
   * @param data
   * @param byteIndex
   * @param bitMask
   */
  static void setBitMaskedByte(ArrowBuf data, int byteIndex, byte bitMask) {
    byte currentByte = data.getByte(byteIndex);
    currentByte |= bitMask;
    data.setByte(byteIndex, currentByte);
  }
}
