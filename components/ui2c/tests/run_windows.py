"""Run the same native C tests with Clang/LLD, without MSVC/Windows SDK."""
import ctypes
from pathlib import Path
import subprocess
import tempfile

here = Path(__file__).resolve().parent
root = here.parents[2]
with tempfile.TemporaryDirectory(prefix="ui2c-test-") as temp:
    dll = Path(temp) / "ui2c_tests.dll"
    subprocess.run([
        "clang", "-shared", "-nostdlib", "-fuse-ld=lld", "-O1", "-fno-builtin",
        "-std=c11", "-Wall", "-Wextra", "-Werror", "-DUI2C_TEST_DLL",
        "-Wl,/noentry", "-I" + str(here / "fakes"),
        "-I" + str(here.parent / "include"),
        "-I" + str(root / "Drivers/CMSIS/Device/ST/STM32F0xx/Include"),
        "-I" + str(root / "Drivers/CMSIS/Include"),
        str(here / "test_ui2c.c"), str(here / "freestanding.c"),
        str(here.parent / "src/ui2c.c"),
        str(here.parent / "ports/stm32f0/ui2c_stm32f0.c"), "-o", str(dll),
    ], check=True)
    lib = ctypes.CDLL(str(dll))
    error = lib.ui2c_tests()
    count = lib.ui2c_test_cases()
    # Release before TemporaryDirectory cleanup on Windows.
    handle = lib._handle
    del lib
    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel32.FreeLibrary.argtypes = [ctypes.c_void_p]
    kernel32.FreeLibrary(handle)
    if error:
        raise SystemExit(f"FAIL test_ui2c.c:{error} ({count} completed cases)")
    print(f"PASS: {count} protocol/lifecycle cases")
