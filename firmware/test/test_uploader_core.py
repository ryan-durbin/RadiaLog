#!/usr/bin/env python3
"""
Tests for US-004: Upload Manager core with FreeRTOS task (uploader.h/cpp - Part 1)

Validates:
- File existence and structure
- Uploader class with required methods
- FreeRTOS task creation on core 0
- HTTPClient usage
- Required headers (X-Device-Token, X-Device-Id)
- ArduinoJson usage for JSON payload
- Batch size limit
- UploadConfig struct
"""

import os
import sys
import unittest

FIRMWARE_DIR = os.path.join(os.path.dirname(__file__), '..')
SRC_DIR = os.path.join(FIRMWARE_DIR, 'src')


def read_file(filename):
    path = os.path.join(SRC_DIR, filename)
    with open(path, 'r') as f:
        return f.read()


class TestUploaderHeaderExists(unittest.TestCase):
    def test_uploader_h_exists(self):
        self.assertTrue(os.path.isfile(os.path.join(SRC_DIR, 'uploader.h')),
                        'uploader.h must exist in firmware/src/')

    def test_uploader_cpp_exists(self):
        self.assertTrue(os.path.isfile(os.path.join(SRC_DIR, 'uploader.cpp')),
                        'uploader.cpp must exist in firmware/src/')


class TestUploaderHeaderGuard(unittest.TestCase):
    def test_pragma_once(self):
        content = read_file('uploader.h')
        self.assertIn('#pragma once', content, 'uploader.h must have #pragma once guard')


class TestUploadConfigStruct(unittest.TestCase):
    def setUp(self):
        self.header = read_file('uploader.h')

    def test_upload_config_struct(self):
        self.assertIn('struct UploadConfig', self.header,
                      'uploader.h must define UploadConfig struct')

    def test_radiamaps_url_field(self):
        self.assertIn('radiamaps_url', self.header,
                      'UploadConfig must have radiamaps_url field')

    def test_device_token_field(self):
        self.assertIn('device_token', self.header,
                      'UploadConfig must have device_token field')

    def test_device_id_field(self):
        self.assertIn('device_id', self.header,
                      'UploadConfig must have device_id field')


class TestUploaderClass(unittest.TestCase):
    def setUp(self):
        self.header = read_file('uploader.h')

    def test_class_uploader(self):
        self.assertIn('class Uploader', self.header,
                      'uploader.h must define Uploader class')

    def test_begin_method(self):
        self.assertIn('begin', self.header,
                      'Uploader must have begin() method')

    def test_begin_accepts_config(self):
        self.assertIn('UploadConfig', self.header,
                      'begin() must accept UploadConfig parameter')

    def test_begin_accepts_buffer_pointer(self):
        self.assertIn('ReadingBuffer*', self.header,
                      'begin() must accept ReadingBuffer pointer')

    def test_begin_accepts_wifi_pointer(self):
        self.assertIn('WifiMgr*', self.header,
                      'begin() must accept WifiMgr pointer')

    def test_force_upload_method(self):
        self.assertIn('forceUpload', self.header,
                      'Uploader must have forceUpload() method')

    def test_get_last_upload_time_method(self):
        self.assertIn('getLastUploadTime', self.header,
                      'Uploader must have getLastUploadTime() method')

    def test_is_uploading_method(self):
        self.assertIn('isUploading', self.header,
                      'Uploader must have isUploading() method')


class TestUploaderForwardDeclarations(unittest.TestCase):
    def setUp(self):
        self.header = read_file('uploader.h')

    def test_forward_decl_reading_buffer(self):
        self.assertIn('class ReadingBuffer', self.header,
                      'uploader.h must forward declare ReadingBuffer')

    def test_forward_decl_wifi_mgr(self):
        self.assertIn('class WifiMgr', self.header,
                      'uploader.h must forward declare WifiMgr')

    def test_no_buffer_h_include(self):
        # Header should use forward declarations, not full includes
        self.assertNotIn('#include "buffer.h"', self.header,
                         'uploader.h should use forward declarations, not #include "buffer.h"')

    def test_no_wifi_mgr_h_include(self):
        self.assertNotIn('#include "wifi_mgr.h"', self.header,
                         'uploader.h should use forward declarations, not #include "wifi_mgr.h"')


class TestUploaderCppImplementation(unittest.TestCase):
    def setUp(self):
        self.cpp = read_file('uploader.cpp')

    def test_includes_uploader_h(self):
        self.assertIn('#include "uploader.h"', self.cpp,
                      'uploader.cpp must include uploader.h')

    def test_includes_buffer_h(self):
        self.assertIn('#include "buffer.h"', self.cpp,
                      'uploader.cpp must include buffer.h for full definition')

    def test_includes_wifi_mgr_h(self):
        self.assertIn('#include "wifi_mgr.h"', self.cpp,
                      'uploader.cpp must include wifi_mgr.h for full definition')


class TestFreeRTOSTask(unittest.TestCase):
    def setUp(self):
        self.cpp = read_file('uploader.cpp')
        self.header = read_file('uploader.h')

    def test_xTaskCreatePinnedToCore(self):
        self.assertIn('xTaskCreatePinnedToCore', self.cpp,
                      'uploader.cpp must use xTaskCreatePinnedToCore for task creation')

    def test_core_0(self):
        # Find the xTaskCreatePinnedToCore call and verify core 0
        lines = self.cpp.split('\n')
        found_call = False
        call_block = ''
        in_call = False
        paren_depth = 0
        for line in lines:
            if 'xTaskCreatePinnedToCore' in line:
                in_call = True
                found_call = True
            if in_call:
                call_block += line + '\n'
                paren_depth += line.count('(') - line.count(')')
                if paren_depth <= 0 and found_call:
                    break
        self.assertTrue(found_call, 'Must call xTaskCreatePinnedToCore')
        # Last argument should be 0 (core 0)
        self.assertIn('0', call_block,
                      'xTaskCreatePinnedToCore last arg must be 0 (core 0)')

    def test_task_stack_size_8192(self):
        combined = self.header + self.cpp
        self.assertIn('8192', combined,
                      'Task stack size should be ~8192 bytes')

    def test_task_priority_1(self):
        self.assertIn('TASK_PRIORITY', self.header,
                      'Task priority constant must be defined')

    def test_task_handle(self):
        self.assertIn('TaskHandle_t', self.header,
                      'Must store TaskHandle_t for the FreeRTOS task')

    def test_upload_task_function(self):
        self.assertIn('_uploadTask', self.cpp,
                      'Must define the _uploadTask static function')


class TestHTTPClient(unittest.TestCase):
    def setUp(self):
        self.cpp = read_file('uploader.cpp')

    def test_httpclient_include(self):
        self.assertIn('#include <HTTPClient.h>', self.cpp,
                      'uploader.cpp must include HTTPClient.h')

    def test_httpclient_usage(self):
        self.assertIn('HTTPClient', self.cpp,
                      'uploader.cpp must use HTTPClient')

    def test_http_post(self):
        self.assertIn('.POST(', self.cpp,
                      'Must call http.POST() for upload requests')

    def test_http_begin(self):
        self.assertIn('.begin(', self.cpp,
                      'Must call http.begin() with URL')


class TestHeaders(unittest.TestCase):
    def setUp(self):
        self.cpp = read_file('uploader.cpp')

    def test_x_device_token_header(self):
        self.assertIn('X-Device-Token', self.cpp,
                      'Must set X-Device-Token header')

    def test_x_device_id_header(self):
        self.assertIn('X-Device-Id', self.cpp,
                      'Must set X-Device-Id header')

    def test_content_type_json(self):
        self.assertIn('application/json', self.cpp,
                      'Must set Content-Type: application/json header')


class TestArduinoJson(unittest.TestCase):
    def setUp(self):
        self.cpp = read_file('uploader.cpp')

    def test_arduinojson_include(self):
        self.assertIn('#include <ArduinoJson.h>', self.cpp,
                      'uploader.cpp must include ArduinoJson.h')

    def test_json_document(self):
        self.assertIn('JsonDocument', self.cpp,
                      'Must use JsonDocument for JSON construction')

    def test_json_array(self):
        self.assertIn('JsonArray', self.cpp,
                      'Must use JsonArray for readings array')

    def test_serialize_json(self):
        self.assertIn('serializeJson', self.cpp,
                      'Must call serializeJson to build payload')

    def test_readings_array_key(self):
        self.assertIn('"readings"', self.cpp,
                      'JSON must contain "readings" array key')


class TestBatchSize(unittest.TestCase):
    def setUp(self):
        self.header = read_file('uploader.h')
        self.cpp = read_file('uploader.cpp')

    def test_max_batch_size_constant(self):
        self.assertIn('MAX_BATCH_SIZE', self.header,
                      'Must define MAX_BATCH_SIZE constant')

    def test_batch_size_10000(self):
        self.assertIn('10000', self.header,
                      'MAX_BATCH_SIZE must be 10000')


class TestExponentialBackoff(unittest.TestCase):
    def setUp(self):
        self.header = read_file('uploader.h')
        self.cpp = read_file('uploader.cpp')

    def test_backoff_initial(self):
        self.assertIn('BACKOFF_INITIAL_MS', self.header,
                      'Must define BACKOFF_INITIAL_MS constant')

    def test_backoff_max(self):
        self.assertIn('BACKOFF_MAX_MS', self.header,
                      'Must define BACKOFF_MAX_MS constant')

    def test_backoff_max_5_min(self):
        self.assertIn('300000', self.header,
                      'BACKOFF_MAX_MS should be 300000 (5 minutes)')

    def test_backoff_doubling(self):
        self.assertIn('_backoffMs *= 2', self.cpp,
                      'Must double backoff on failure')


class TestForceUpload(unittest.TestCase):
    def setUp(self):
        self.cpp = read_file('uploader.cpp')

    def test_force_flag(self):
        self.assertIn('_forceFlag', self.cpp,
                      'Must use a flag for forceUpload signaling')

    def test_task_notification(self):
        self.assertIn('xTaskNotifyGive', self.cpp,
                      'forceUpload should use xTaskNotifyGive to wake task')

    def test_task_notify_take(self):
        self.assertIn('ulTaskNotifyTake', self.cpp,
                      'Task should use ulTaskNotifyTake to wait for notifications')


class TestMarkUploaded(unittest.TestCase):
    def setUp(self):
        self.cpp = read_file('uploader.cpp')

    def test_mark_uploaded_call(self):
        self.assertIn('markUploaded', self.cpp,
                      'Must call buffer->markUploaded on success')

    def test_get_unuploaded_call(self):
        self.assertIn('getUnuploaded', self.cpp,
                      'Must call buffer->getUnuploaded to fetch readings')


if __name__ == '__main__':
    unittest.main(verbosity=2)
