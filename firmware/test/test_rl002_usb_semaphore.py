#!/usr/bin/env python3
"""
Tests for RL-002: USB transfer semaphore in RadiaCode transport.

Verifies that:
1. radiacode.h declares void* _xfer_semaphore private member
2. radiacode.cpp creates semaphore with xSemaphoreCreateBinary() in initUsbHost()
3. radiacode.cpp uses xSemaphoreTake() in usbWrite() instead of vTaskDelay
4. radiacode.cpp uses xSemaphoreTake() in usbRead() instead of vTaskDelay
5. radiacode.cpp has a USB transfer callback that calls xSemaphoreGiveFromISR
6. vTaskDelay is NOT used inside #ifdef ARDUINO blocks of usbWrite()/usbRead()
7. Callback is assigned to xfer->callback in claimInterface()
8. Semaphore is deleted in releaseUsbHost()
"""

import os
import re
import unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
HEADER = os.path.join(REPO, "firmware", "src", "radiacode.h")
SOURCE = os.path.join(REPO, "firmware", "src", "radiacode.cpp")


def read_file(path):
    with open(path, "r") as f:
        return f.read()


def extract_arduino_block(source, func_signature):
    """Extract the #ifdef ARDUINO block content from a function.
    
    Finds the function by signature, then finds the #ifdef ARDUINO / #else 
    block within it.
    """
    # Find the function
    func_start = source.find(func_signature)
    if func_start == -1:
        return ""
    
    # Find the #ifdef ARDUINO within this function
    ifdef_pos = source.find("#ifdef ARDUINO", func_start)
    if ifdef_pos == -1:
        return ""
    
    # Find the matching #else or #endif
    else_pos = source.find("#else", ifdef_pos)
    endif_pos = source.find("#endif", ifdef_pos)
    
    if else_pos != -1 and else_pos < endif_pos:
        end = else_pos
    else:
        end = endif_pos
    
    return source[ifdef_pos:end]


class TestRL002Header(unittest.TestCase):
    """Tests for radiacode.h changes."""

    def setUp(self):
        self.header = read_file(HEADER)

    def test_xfer_semaphore_declared(self):
        """AC-1: radiacode.h declares void* _xfer_semaphore private member."""
        # Look for the member declaration
        self.assertRegex(
            self.header,
            r'void\s*\*\s*_xfer_semaphore',
            "_xfer_semaphore member not found in radiacode.h"
        )

    def test_xfer_semaphore_in_private_section(self):
        """_xfer_semaphore should be in the private section of the class."""
        # Find 'private:' then check _xfer_semaphore appears after it
        private_pos = self.header.find("private:")
        self.assertNotEqual(private_pos, -1, "No private: section found")
        semaphore_pos = self.header.find("_xfer_semaphore", private_pos)
        self.assertNotEqual(semaphore_pos, -1,
                          "_xfer_semaphore not found after private:")


class TestRL002Source(unittest.TestCase):
    """Tests for radiacode.cpp changes."""

    def setUp(self):
        self.source = read_file(SOURCE)

    def test_semaphore_create_in_initUsbHost(self):
        """AC-2: xSemaphoreCreateBinary() called in initUsbHost()."""
        block = extract_arduino_block(self.source, "RadiaCode::initUsbHost()")
        self.assertIn("xSemaphoreCreateBinary()", block,
                      "xSemaphoreCreateBinary() not found in initUsbHost() ARDUINO block")

    def test_semaphore_take_in_usbWrite(self):
        """AC-3: xSemaphoreTake() used in usbWrite() instead of vTaskDelay."""
        block = extract_arduino_block(self.source, "RadiaCode::usbWrite(")
        self.assertIn("xSemaphoreTake", block,
                      "xSemaphoreTake not found in usbWrite() ARDUINO block")

    def test_semaphore_take_in_usbRead(self):
        """AC-4: xSemaphoreTake() used in usbRead() instead of vTaskDelay."""
        block = extract_arduino_block(self.source, "RadiaCode::usbRead(")
        self.assertIn("xSemaphoreTake", block,
                      "xSemaphoreTake not found in usbRead() ARDUINO block")

    def test_callback_has_xSemaphoreGiveFromISR(self):
        """AC-5: USB transfer callback calls xSemaphoreGiveFromISR."""
        self.assertIn("xSemaphoreGiveFromISR", self.source,
                      "xSemaphoreGiveFromISR not found in radiacode.cpp")

    def test_callback_function_exists(self):
        """AC-5: A static usbTransferCallback function exists."""
        self.assertRegex(
            self.source,
            r'static\s+void\s+usbTransferCallback\s*\(\s*usb_transfer_t\s*\*',
            "usbTransferCallback function not found in radiacode.cpp"
        )

    def test_no_vTaskDelay_in_usbWrite_arduino(self):
        """AC-6: vTaskDelay is NOT in the #ifdef ARDUINO block of usbWrite()."""
        block = extract_arduino_block(self.source, "RadiaCode::usbWrite(")
        self.assertNotIn("vTaskDelay", block,
                        "vTaskDelay still present in usbWrite() ARDUINO block")

    def test_no_vTaskDelay_in_usbRead_arduino(self):
        """AC-7: vTaskDelay is NOT in the #ifdef ARDUINO block of usbRead()."""
        block = extract_arduino_block(self.source, "RadiaCode::usbRead(")
        self.assertNotIn("vTaskDelay", block,
                        "vTaskDelay still present in usbRead() ARDUINO block")

    def test_callback_assigned_to_xfer(self):
        """Callback is assigned to xfer->callback in claimInterface()."""
        block = extract_arduino_block(self.source, "RadiaCode::claimInterface()")
        self.assertIn("callback = usbTransferCallback", block,
                      "usbTransferCallback not assigned to xfer->callback in claimInterface()")

    def test_context_assigned_to_xfer(self):
        """Semaphore handle is assigned to xfer->context in claimInterface()."""
        block = extract_arduino_block(self.source, "RadiaCode::claimInterface()")
        self.assertIn("context = _xfer_semaphore", block,
                      "_xfer_semaphore not assigned to xfer->context in claimInterface()")

    def test_semaphore_delete_in_releaseUsbHost(self):
        """Semaphore is deleted in releaseUsbHost()."""
        block = extract_arduino_block(self.source, "RadiaCode::releaseUsbHost()")
        self.assertIn("vSemaphoreDelete", block,
                      "vSemaphoreDelete not found in releaseUsbHost() ARDUINO block")

    def test_constructor_initializes_semaphore(self):
        """Constructor initializer list includes _xfer_semaphore(nullptr)."""
        self.assertIn("_xfer_semaphore(nullptr)", self.source,
                      "_xfer_semaphore not initialized in constructor")


if __name__ == "__main__":
    unittest.main()
