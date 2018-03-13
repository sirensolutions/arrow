@rem Licensed to the Apache Software Foundation (ASF) under one
@rem or more contributor license agreements.  See the NOTICE file
@rem distributed with this work for additional information
@rem regarding copyright ownership.  The ASF licenses this file
@rem to you under the Apache License, Version 2.0 (the
@rem "License"); you may not use this file except in compliance
@rem with the License.  You may obtain a copy of the License at
@rem
@rem   http://www.apache.org/licenses/LICENSE-2.0
@rem
@rem Unless required by applicable law or agreed to in writing,
@rem software distributed under the License is distributed on an
@rem "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
@rem KIND, either express or implied.  See the License for the
@rem specific language governing permissions and limitations
@rem under the License.

@echo on

if "%JOB%" == "Static_Crt_Build" (
  mkdir cpp\build-debug
  pushd cpp\build-debug

  cmake -G "%GENERATOR%" ^
        -DARROW_USE_STATIC_CRT=ON ^
        -DARROW_BOOST_USE_SHARED=OFF ^
        -DCMAKE_BUILD_TYPE=Debug ^
        -DARROW_CXXFLAGS="/MP" ^
        ..  || exit /B

  cmake --build . --config Debug || exit /B
  popd

  mkdir cpp\build-release
  pushd cpp\build-release

  cmake -G "%GENERATOR%" ^
        -DARROW_USE_STATIC_CRT=ON ^
        -DARROW_BOOST_USE_SHARED=OFF ^
        -DCMAKE_BUILD_TYPE=Release ^
        -DARROW_CXXFLAGS="/WX /MP" ^
        ..  || exit /B

  cmake --build . --config Release || exit /B
  ctest -VV  || exit /B
  popd

  @rem Finish Static_Crt_Build build successfully
  exit /B 0
)

if "%JOB%" == "Build_Debug" (
  mkdir cpp\build-debug
  pushd cpp\build-debug

  cmake -G "%GENERATOR%" ^
        -DARROW_BOOST_USE_SHARED=OFF ^
        -DCMAKE_BUILD_TYPE=%CONFIGURATION% ^
        -DARROW_CXXFLAGS="/MP" ^
        ..  || exit /B

  cmake --build . --config Debug || exit /B
  popd

  @rem Finish Debug build successfully
  exit /B 0
)

conda update --yes --quiet conda
conda config --set auto_update_conda false
conda info -a

conda config --set show_channel_urls True

# Help with SSL timeouts to S3
conda config --set remote_connect_timeout_secs 12

conda config --add channels https://repo.continuum.io/pkgs/free
conda config --add channels conda-forge
conda info -a

conda create -n arrow -q -y python=%PYTHON% ^
      six pytest setuptools numpy pandas cython ^
      thrift-cpp

if "%JOB%" == "Toolchain" (

  conda install -n arrow -q -y -c conda-forge ^
      flatbuffers rapidjson ^
      cmake ^
      git ^
      boost-cpp ^
      snappy zlib brotli gflags lz4-c zstd
)

call activate arrow

if "%JOB%" == "Toolchain" (
  set ARROW_BUILD_TOOLCHAIN=%CONDA_PREFIX%\Library
)

set ARROW_HOME=%CONDA_PREFIX%\Library

@rem Build and test Arrow C++ libraries

mkdir cpp\build
pushd cpp\build

cmake -G "%GENERATOR%" ^
      -DCMAKE_INSTALL_PREFIX=%CONDA_PREFIX%\Library ^
      -DARROW_BOOST_USE_SHARED=OFF ^
      -DCMAKE_BUILD_TYPE=%CONFIGURATION% ^
      -DARROW_CXXFLAGS="/WX /MP" ^
      -DARROW_PYTHON=ON ^
      ..  || exit /B
cmake --build . --target INSTALL --config %CONFIGURATION%  || exit /B

@rem Needed so python-test.exe works
set PYTHONPATH=%CONDA_PREFIX%\Lib;%CONDA_PREFIX%\Lib\site-packages;%CONDA_PREFIX%\python35.zip;%CONDA_PREFIX%\DLLs;%CONDA_PREFIX%;%PYTHONPATH%

ctest -VV  || exit /B
popd

@rem Build parquet-cpp

git clone https://github.com/apache/parquet-cpp.git || exit /B
mkdir parquet-cpp\build
pushd parquet-cpp\build

set PARQUET_BUILD_TOOLCHAIN=%CONDA_PREFIX%\Library
set PARQUET_HOME=%CONDA_PREFIX%\Library
cmake -G "%GENERATOR%" ^
     -DCMAKE_INSTALL_PREFIX=%PARQUET_HOME% ^
     -DCMAKE_BUILD_TYPE=%CONFIGURATION% ^
     -DPARQUET_BOOST_USE_SHARED=OFF ^
     -DPARQUET_BUILD_TESTS=off .. || exit /B
cmake --build . --target INSTALL --config %CONFIGURATION% || exit /B
popd

@rem Build and import pyarrow
@rem parquet-cpp has some additional runtime dependencies that we need to figure out
@rem see PARQUET-1018

pushd python

set PYARROW_CXXFLAGS=/WX
python setup.py build_ext --inplace --with-parquet --bundle-arrow-cpp bdist_wheel  || exit /B
py.test pyarrow -v -s --parquet || exit /B

popd
