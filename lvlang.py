"""
LVLang Python SDK & Execution Engine
Provides native Python bindings for running LVLang 16-bit 2-byte machine bytecode.
"""

import os
import subprocess
import ctypes
from typing import Union, Dict, Any, Optional

LVLC_PATH = os.path.join(os.path.dirname(__file__), "lvlc")

class LVLangRuntimeError(Exception):
    pass

class LVLang:
    """LVLang VM Execution Engine Wrapper for Python & AI Agents."""

    @staticmethod
    def eval(hex_stream: str) -> str:
        """
        Executes a 2-byte hex stream directly in the LVLang C VM runtime.
        Returns the stdout output string.
        """
        if not os.path.exists(LVLC_PATH):
            raise LVLangRuntimeError(f"LVLang executable CLI not found at '{LVLC_PATH}'. Run 'gcc -O3 lvlc.c -o lvlc -lSDL2 -lm' first.")

        process = subprocess.Popen(
            [LVLC_PATH, hex_stream],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True
        )

        stdout, stderr = process.communicate()

        # Extract output after the header
        output_marker = "=== Executing in LVLang VM Runtime ==="
        if output_marker in stdout:
            parts = stdout.split(output_marker)
            raw_output = parts[1]
            # Strip VM Halted status line if present
            if "[VM Halted]" in raw_output:
                raw_output = raw_output.split("[VM Halted]")[0]
            # Strip "Output: " prefix if present
            if raw_output.startswith("\nOutput: "):
                raw_output = raw_output[9:]
            elif raw_output.startswith("Output: "):
                raw_output = raw_output[8:]
            return raw_output.strip()
        
        return stdout.strip()

    @staticmethod
    def run_file(file_path: str) -> str:
        """
        Executes a binary bytecode file (.lvl) directly in LVLang VM runtime.
        """
        if not os.path.exists(LVLC_PATH):
            raise LVLangRuntimeError(f"LVLang executable CLI not found at '{LVLC_PATH}'.")

        process = subprocess.Popen(
            [LVLC_PATH, file_path],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True
        )

        stdout, stderr = process.communicate()
        return stdout.strip()

if __name__ == "__main__":
    # Test Python SDK execution
    print("[+] Testing LVLang Python SDK Execution...")
    # Math test: sqrt(100 / 50)
    res = LVLang.eval("01xE4 0Ax05 01xB2 0Ax05 0Ax04 0Ax08 0Ax07 05x04 05xFF")
    print(f"[+] Output from Python SDK: {res}")
