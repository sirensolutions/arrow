<!---
  Licensed to the Apache Software Foundation (ASF) under one
  or more contributor license agreements.  See the NOTICE file
  distributed with this work for additional information
  regarding copyright ownership.  The ASF licenses this file
  to you under the Apache License, Version 2.0 (the
  "License"); you may not use this file except in compliance
  with the License.  You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing,
  software distributed under the License is distributed on an
  "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
  KIND, either express or implied.  See the License for the
  specific language governing permissions and limitations
  under the License.
-->

# Siren fork of Arrow

The properties `drill.enable_unsafe_memory_access` and
  `arrow.enable_unsafe_memory_access` are prefixed with `siren` and their
  default value is set to `true`. The first property is deprecated.

## Check that Arrow uses Unsafe class to access off-heap memory for memory allocation
In order to check that Arrow uses Unsafe class for memory allocation, run the unit test `CheckArrowTest` in 
  `https://github.com/sirensolutions/siren-platform/blob/master/core/src/test/java/io/siren/federate/core/common/CheckArrowTest.java`.

## Build

To build the `memory`, `format`, `vector` and `algorithm` modules:

```sh
$ cd java
$ mvn clean package
```

Because of the default value change of `unsafe_memory_access` property, some
tests in `vector` fail.

```sh
mvn -pl maven,maven/module-info-compiler-maven-plugin,memory,memory/memory-core,memory/memory-unsafe,format,vector,algorithm install -Dsiren.arrow.enable_unsafe_memory_access=false -Dsiren.drill.enable_unsafe_memory_access=false
```

## Make a new release of Siren's Apache Arrow

- Tests should pass.

- Make a new version:

```sh
mvn versions:set -DnewVersion=siren-0.14.1-2
```

- tag the commit for the release

```sh
git tag --sign siren-0.14.1-2
````

- Deploy to Siren's artifactory

```sh
$ mvn deploy -DskipTests=true -P artifactory -Dartifactory_username=<USERNAME> -Dartifactory_password=<PASSWORD>
```

## Update to a new version of Siren's Apache Arrow
Developer tips on updating to a new version of Arrow can be found here: https://sirensolutions.atlassian.net/wiki/spaces/EN/pages/3108864001/Upgrading+Federate+Apache+Arrow+Version .

- add `git@github.com:apache/arrow.git` as the `upstream` remote.
- execute `git fetch --all --tags`
- create a temporary branch from `siren-changes`
- rebase against the new tag.

# Apache Arrow

[![Fuzzing Status](https://oss-fuzz-build-logs.storage.googleapis.com/badges/arrow.svg)](https://bugs.chromium.org/p/oss-fuzz/issues/list?sort=-opened&can=1&q=proj:arrow)
[![License](http://img.shields.io/:license-Apache%202-blue.svg)](https://github.com/apache/arrow/blob/main/LICENSE.txt)
[![Twitter Follow](https://img.shields.io/twitter/follow/apachearrow.svg?style=social&label=Follow)](https://twitter.com/apachearrow)

## Powering In-Memory Analytics

Apache Arrow is a development platform for in-memory analytics. It contains a
set of technologies that enable big data systems to process and move data fast.

Major components of the project include:

 - [The Arrow Columnar In-Memory Format](https://arrow.apache.org/docs/dev/format/Columnar.html):
   a standard and efficient in-memory representation of various datatypes, plain or nested
 - [The Arrow IPC Format](https://arrow.apache.org/docs/dev/format/Columnar.html#serialization-and-interprocess-communication-ipc):
   an efficient serialization of the Arrow format and associated metadata,
   for communication between processes and heterogeneous environments
 - [The Arrow Flight RPC protocol](https://github.com/apache/arrow/tree/main/format/Flight.proto):
   based on the Arrow IPC format, a building block for remote services exchanging
   Arrow data with application-defined semantics (for example a storage server or a database)
 - [C++ libraries](https://github.com/apache/arrow/tree/main/cpp)
 - [C bindings using GLib](https://github.com/apache/arrow/tree/main/c_glib)
 - [C# .NET libraries](https://github.com/apache/arrow/tree/main/csharp)
 - [Gandiva](https://github.com/apache/arrow/tree/main/cpp/src/gandiva):
   an [LLVM](https://llvm.org)-based Arrow expression compiler, part of the C++ codebase
 - [Go libraries](https://github.com/apache/arrow/tree/main/go)
 - [Java libraries](https://github.com/apache/arrow/tree/main/java)
 - [JavaScript libraries](https://github.com/apache/arrow/tree/main/js)
 - [Python libraries](https://github.com/apache/arrow/tree/main/python)
 - [R libraries](https://github.com/apache/arrow/tree/main/r)
 - [Ruby libraries](https://github.com/apache/arrow/tree/main/ruby)
 - [Rust libraries](https://github.com/apache/arrow-rs)

Arrow is an [Apache Software Foundation](https://www.apache.org) project. Learn more at
[arrow.apache.org](https://arrow.apache.org).

## What's in the Arrow libraries?

The reference Arrow libraries contain many distinct software components:

- Columnar vector and table-like containers (similar to data frames) supporting
  flat or nested types
- Fast, language agnostic metadata messaging layer (using Google's Flatbuffers
  library)
- Reference-counted off-heap buffer memory management, for zero-copy memory
  sharing and handling memory-mapped files
- IO interfaces to local and remote filesystems
- Self-describing binary wire formats (streaming and batch/file-like) for
  remote procedure calls (RPC) and interprocess communication (IPC)
- Integration tests for verifying binary compatibility between the
  implementations (e.g. sending data from Java to C++)
- Conversions to and from other in-memory data structures
- Readers and writers for various widely-used file formats (such as Parquet, CSV)

## Implementation status

The official Arrow libraries in this repository are in different stages of
implementing the Arrow format and related features.  See our current
[feature matrix](https://arrow.apache.org/docs/dev/status.html)
on git main.

## How to Contribute

Please read our latest [project contribution guide][5].

## Getting involved

Even if you do not plan to contribute to Apache Arrow itself or Arrow
integrations in other projects, we'd be happy to have you involved:

- Join the mailing list: send an email to
  [dev-subscribe@arrow.apache.org][1]. Share your ideas and use cases for the
  project.
- Follow our activity on [GitHub issues][3]
- [Learn the format][2]
- Contribute code to one of the reference implementations

[1]: mailto:dev-subscribe@arrow.apache.org
[2]: https://github.com/apache/arrow/tree/main/format
[3]: https://github.com/apache/arrow/issues
[4]: https://github.com/apache/arrow
[5]: https://arrow.apache.org/docs/dev/developers/index.html
