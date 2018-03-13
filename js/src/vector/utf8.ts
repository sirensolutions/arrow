// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

import { Vector } from './vector';
import { VirtualVector } from './virtual';
import { TextDecoder } from 'text-encoding-utf-8';

const decoder = new TextDecoder('utf-8');

export class Utf8Vector extends Vector<string> {
    readonly values: Vector<Uint8Array | null>;
    constructor(argv: { values: Vector<Uint8Array | null> }) {
        super();
        this.values = argv.values;
    }
    get(index: number) {
        const chars = this.getCodePoints(index);
        return chars ? decoder.decode(chars) : null;
    }
    getCodePoints(index: number) {
        return this.values.get(index);
    }
    concat(...vectors: Vector<string>[]): Vector<string> {
        return new VirtualVector(Array, this, ...vectors);
    }
}
