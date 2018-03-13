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

const {
    mainExport, gCCLanguageNames,
    targetDir, observableFromStreams
} = require('./util');

const gulp = require('gulp');
const path = require('path');
const gulpRename = require(`gulp-rename`);
const { memoizeTask } = require('./memoize-task');
const { Observable, ReplaySubject } = require('rxjs');

const arrowTask = ((cache) => memoizeTask(cache, function copyMain(target, format) {
    const out = targetDir(target);
    const srcGlob = `src/**/*.ts`;
    const es5Glob = `${targetDir(`es5`, `cjs`)}/**/*.js`;
    const esmGlob = `${targetDir(`es2015`, `esm`)}/**/*.js`;
    const es5UmdGlob = `${targetDir(`es5`, `umd`)}/**/*.js`;
    const es5UmdMaps = `${targetDir(`es5`, `umd`)}/**/*.map`;
    const es2015UmdGlob = `${targetDir(`es2015`, `umd`)}/**/*.js`;
    const es2015UmdMaps = `${targetDir(`es2015`, `umd`)}/**/*.map`;
    const ch_ext = (ext) => gulpRename((p) => { p.extname = ext; });
    const append = (ap) => gulpRename((p) => { p.basename += ap; });
    return Observable.forkJoin(
      observableFromStreams(gulp.src(srcGlob), gulp.dest(out)), // copy src ts files
      observableFromStreams(gulp.src(es5Glob), gulp.dest(out)), // copy es5 cjs files
      observableFromStreams(gulp.src(esmGlob), ch_ext(`.mjs`), gulp.dest(out)), // copy es2015 esm files and rename to `.mjs`
      observableFromStreams(gulp.src(es5UmdGlob), append(`.es5.min`), gulp.dest(out)), // copy es5 umd files and add `.min`
      observableFromStreams(gulp.src(es5UmdMaps),                     gulp.dest(out)), // copy es5 umd sourcemap files, but don't rename
      observableFromStreams(gulp.src(es2015UmdGlob), append(`.es2015.min`), gulp.dest(out)), // copy es2015 umd files and add `.es6.min`
      observableFromStreams(gulp.src(es2015UmdMaps),                        gulp.dest(out)), // copy es2015 umd sourcemap files, but don't rename
    ).publish(new ReplaySubject()).refCount();
}))({});

const arrowTSTask = ((cache) => memoizeTask(cache, function copyTS(target, format) {
    return observableFromStreams(gulp.src(`src/**/*.ts`), gulp.dest(targetDir(target, format)));
}))({});
  
  
module.exports = arrowTask;
module.exports.arrowTask = arrowTask;
module.exports.arrowTSTask = arrowTSTask;