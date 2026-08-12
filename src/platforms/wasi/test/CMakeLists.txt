#
# This file is part of AtomVM.
#
# Copyright 2025 AtomVM Contributors
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# SPDX-License-Identifier: Apache-2.0 OR LGPL-2.1-or-later
#

project(wasi_tests)

include(BuildErlang)

# Core VM tests
pack_runnable(test_arithmetic test_arithmetic estdlib)
pack_runnable(test_binary test_binary estdlib)
pack_runnable(test_lists test_lists estdlib)
pack_runnable(test_maps test_maps estdlib eavmlib)
pack_runnable(test_processes test_processes estdlib eavmlib)
pack_runnable(test_recursion test_recursion estdlib)

# I/O tests
pack_runnable(test_io_format test_io_format estdlib eavmlib)
pack_runnable(test_console_port test_console_port estdlib eavmlib)
pack_runnable(test_console_port2 test_console_port2 estdlib eavmlib)

# File I/O tests
pack_runnable(test_file_io test_file_io estdlib eavmlib)
pack_runnable(test_file_io2 test_file_io2 estdlib eavmlib)
pack_runnable(test_file_debug test_file_debug estdlib eavmlib)
pack_runnable(test_open3 test_open3 estdlib eavmlib)
pack_runnable(test_open4 test_open4 estdlib eavmlib)
pack_runnable(test_open5 test_open5 estdlib eavmlib)
pack_runnable(test_open6 test_open6 estdlib eavmlib)

# Socket tests
pack_runnable(test_socket_basic test_socket_basic estdlib eavmlib)
pack_runnable(test_tcp_client test_tcp_client estdlib eavmlib)
pack_runnable(test_tcp_httpbin test_tcp_httpbin estdlib eavmlib)
pack_runnable(test_tcp_local test_tcp_local estdlib eavmlib)
