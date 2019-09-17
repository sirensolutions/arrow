﻿// Licensed to the Apache Software Foundation (ASF) under one or more
// contributor license agreements. See the NOTICE file distributed with
// this work for additional information regarding copyright ownership.
// The ASF licenses this file to You under the Apache License, Version 2.0
// (the "License"); you may not use this file except in compliance with
// the License.  You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

using Apache.Arrow.Ipc;
using Apache.Arrow.Types;
using System;
using System.IO;
using System.Linq;
using System.Net;
using System.Net.Sockets;
using System.Threading.Tasks;
using Xunit;

namespace Apache.Arrow.Tests
{
    public class ArrowStreamWriterTests
    {
        [Fact]
        public void Ctor_LeaveOpenDefault_StreamClosedOnDispose()
        {
            RecordBatch originalBatch = TestData.CreateSampleRecordBatch(length: 100);
            var stream = new MemoryStream();
            new ArrowStreamWriter(stream, originalBatch.Schema).Dispose();
            Assert.Throws<ObjectDisposedException>(() => stream.Position);
        }

        [Fact]
        public void Ctor_LeaveOpenFalse_StreamClosedOnDispose()
        {
            RecordBatch originalBatch = TestData.CreateSampleRecordBatch(length: 100);
            var stream = new MemoryStream();
            new ArrowStreamWriter(stream, originalBatch.Schema, leaveOpen: false).Dispose();
            Assert.Throws<ObjectDisposedException>(() => stream.Position);
        }

        [Fact]
        public void Ctor_LeaveOpenTrue_StreamValidOnDispose()
        {
            RecordBatch originalBatch = TestData.CreateSampleRecordBatch(length: 100);
            var stream = new MemoryStream();
            new ArrowStreamWriter(stream, originalBatch.Schema, leaveOpen: true).Dispose();
            Assert.Equal(0, stream.Position);
        }

        [Fact]
        public async Task CanWriteToNetworkStream()
        {
            RecordBatch originalBatch = TestData.CreateSampleRecordBatch(length: 100);

            const int port = 32154;
            TcpListener listener = new TcpListener(IPAddress.Loopback, port);
            listener.Start();

            using (TcpClient sender = new TcpClient())
            {
                sender.Connect(IPAddress.Loopback, port);
                NetworkStream stream = sender.GetStream();

                using (var writer = new ArrowStreamWriter(stream, originalBatch.Schema))
                {
                    await writer.WriteRecordBatchAsync(originalBatch);
                    stream.Flush();
                }
            }

            using (TcpClient receiver = listener.AcceptTcpClient())
            {
                NetworkStream stream = receiver.GetStream();
                using (var reader = new ArrowStreamReader(stream))
                {
                    RecordBatch newBatch = reader.ReadNextRecordBatch();
                    ArrowReaderVerifier.CompareBatches(originalBatch, newBatch);
                }
            }
        }

        [Fact]
        public async Task WriteEmptyBatch()
        {
            RecordBatch originalBatch = TestData.CreateSampleRecordBatch(length: 0);

            await TestRoundTripRecordBatch(originalBatch);
        }

        [Fact]
        public async Task WriteBatchWithNulls()
        {
            RecordBatch originalBatch = new RecordBatch.Builder()
                .Append("Column1", false, col => col.Int32(array => array.AppendRange(Enumerable.Range(0, 10))))
                .Append("Column2", true, new Int32Array(
                    valueBuffer: new ArrowBuffer.Builder<int>().AppendRange(Enumerable.Range(0, 10)).Build(),
                    nullBitmapBuffer: new ArrowBuffer.Builder<byte>().Append(0xfd).Append(0xff).Build(),
                    length: 10,
                    nullCount: 2,
                    offset: 0))
                .Append("Column3", true, new Int32Array(
                    valueBuffer: new ArrowBuffer.Builder<int>().AppendRange(Enumerable.Range(0, 10)).Build(),
                    nullBitmapBuffer: new ArrowBuffer.Builder<byte>().Append(0x00).Append(0x00).Build(),
                    length: 10,
                    nullCount: 10,
                    offset: 0))
                .Append("NullableBooleanColumn", true, new BooleanArray(
                    valueBuffer: new ArrowBuffer.Builder<byte>().Append(0xfd).Append(0xff).Build(),
                    nullBitmapBuffer: new ArrowBuffer.Builder<byte>().Append(0xed).Append(0xff).Build(),
                    length: 10,
                    nullCount: 3,
                    offset: 0))
                .Build();

            await TestRoundTripRecordBatch(originalBatch);
        }

        private static async Task TestRoundTripRecordBatch(RecordBatch originalBatch)
        {
            using (MemoryStream stream = new MemoryStream())
            {
                using (var writer = new ArrowStreamWriter(stream, originalBatch.Schema, leaveOpen: true))
                {
                    await writer.WriteRecordBatchAsync(originalBatch);
                }

                stream.Position = 0;

                using (var reader = new ArrowStreamReader(stream))
                {
                    RecordBatch newBatch = reader.ReadNextRecordBatch();
                    ArrowReaderVerifier.CompareBatches(originalBatch, newBatch);
                }
            }
        }

        [Fact]
        public async Task WriteBatchWithCorrectPadding()
        {
            byte value1 = 0x04;
            byte value2 = 0x14;
            var batch = new RecordBatch(
                new Schema.Builder()
                    .Field(f => f.Name("age").DataType(Int32Type.Default))
                    .Field(f => f.Name("characterCount").DataType(Int32Type.Default))
                    .Build(),
                new IArrowArray[]
                {
                    new Int32Array(
                        new ArrowBuffer(new byte[] { value1, value1, 0x00, 0x00 }),
                        ArrowBuffer.Empty,
                        length: 1,
                        nullCount: 0,
                        offset: 0),
                    new Int32Array(
                        new ArrowBuffer(new byte[] { value2, value2, 0x00, 0x00 }),
                        ArrowBuffer.Empty,
                        length: 1,
                        nullCount: 0,
                        offset: 0)
                },
                length: 1);

            await TestRoundTripRecordBatch(batch);

            using (MemoryStream stream = new MemoryStream())
            {
                using (var writer = new ArrowStreamWriter(stream, batch.Schema, leaveOpen: true))
                {
                    await writer.WriteRecordBatchAsync(batch);
                }

                byte[] writtenBytes = stream.ToArray();

                // ensure that the data buffers at the end are 8-byte aligned
                Assert.Equal(value1, writtenBytes[writtenBytes.Length - 16]);
                Assert.Equal(value1, writtenBytes[writtenBytes.Length - 15]);
                for (int i = 14; i > 8; i--)
                {
                    Assert.Equal(0, writtenBytes[writtenBytes.Length - i]);
                }

                Assert.Equal(value2, writtenBytes[writtenBytes.Length - 8]);
                Assert.Equal(value2, writtenBytes[writtenBytes.Length - 7]);
                for (int i = 6; i > 0; i--)
                {
                    Assert.Equal(0, writtenBytes[writtenBytes.Length - i]);
                }
            }
        }
    }
}
