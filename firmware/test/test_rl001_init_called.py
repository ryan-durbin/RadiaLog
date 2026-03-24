"""
test_rl001_init_called.py

RL-001: Verify that radiaCode.init() is called in setup() after radiaCode.connect(),
inside the `if (err == radiacode::Error::OK)` block, and NOT before connect().
"""

import re
import sys
from pathlib import Path

MAIN_CPP = Path(__file__).parent.parent / "src" / "main.cpp"


def extract_setup_body(source: str) -> str:
    """Extract the body of the setup() function."""
    # Find the setup() function definition
    setup_match = re.search(r'\bvoid\s+setup\s*\(\s*\)', source)
    assert setup_match, "Could not find setup() function in main.cpp"

    start = setup_match.start()
    # Find the opening brace
    brace_pos = source.index('{', start)
    depth = 0
    i = brace_pos
    while i < len(source):
        if source[i] == '{':
            depth += 1
        elif source[i] == '}':
            depth -= 1
            if depth == 0:
                return source[brace_pos:i + 1]
        i += 1
    raise AssertionError("Could not find end of setup() function")


def test_init_called_after_connect():
    """radiaCode.init() must be called after radiaCode.connect() in setup()."""
    source = MAIN_CPP.read_text()
    setup_body = extract_setup_body(source)

    connect_pos = setup_body.find('radiaCode.connect()')
    init_pos = setup_body.find('radiaCode.init()')

    assert connect_pos != -1, "radiaCode.connect() not found in setup()"
    assert init_pos != -1, "radiaCode.init() not found in setup()"
    assert init_pos > connect_pos, (
        f"radiaCode.init() (pos {init_pos}) must appear AFTER "
        f"radiaCode.connect() (pos {connect_pos}) in setup()"
    )
    print("PASS: radiaCode.init() is called after radiaCode.connect() in setup()")


def test_init_inside_ok_block():
    """radiaCode.init() must appear inside the `if (err == radiacode::Error::OK)` block."""
    source = MAIN_CPP.read_text()
    setup_body = extract_setup_body(source)

    ok_block_match = re.search(
        r'if\s*\(\s*err\s*==\s*radiacode::Error::OK\s*\)\s*\{',
        setup_body
    )
    assert ok_block_match, "`if (err == radiacode::Error::OK)` block not found in setup()"

    ok_start = ok_block_match.start()
    # Find the matching closing brace for this if block
    brace_pos = setup_body.index('{', ok_block_match.start())
    depth = 0
    i = brace_pos
    while i < len(setup_body):
        if setup_body[i] == '{':
            depth += 1
        elif setup_body[i] == '}':
            depth -= 1
            if depth == 0:
                ok_end = i
                break
        i += 1
    else:
        raise AssertionError("Could not find end of `if (err == radiacode::Error::OK)` block")

    ok_block_text = setup_body[ok_start:ok_end + 1]
    assert 'radiaCode.init()' in ok_block_text, (
        "radiaCode.init() must be inside the `if (err == radiacode::Error::OK)` block"
    )
    print("PASS: radiaCode.init() is inside the `if (err == radiacode::Error::OK)` block")


def test_init_not_called_before_connect():
    """radiaCode.init() must NOT appear before radiaCode.connect() in setup()."""
    source = MAIN_CPP.read_text()
    setup_body = extract_setup_body(source)

    connect_pos = setup_body.find('radiaCode.connect()')
    assert connect_pos != -1, "radiaCode.connect() not found in setup()"

    before_connect = setup_body[:connect_pos]
    assert 'radiaCode.init()' not in before_connect, (
        "radiaCode.init() must NOT appear before radiaCode.connect() in setup()"
    )
    print("PASS: radiaCode.init() does NOT appear before radiaCode.connect() in setup()")


def test_init_result_logged():
    """A log message must record the result of radiaCode.init()."""
    source = MAIN_CPP.read_text()
    setup_body = extract_setup_body(source)

    init_pos = setup_body.find('radiaCode.init()')
    assert init_pos != -1, "radiaCode.init() not found in setup()"

    # Check that there's a log call after init() in the same block
    after_init = setup_body[init_pos:]
    assert 'debugWS.log' in after_init, (
        "A debugWS.log() call must appear after radiaCode.init() to record the result"
    )
    print("PASS: A log message records the result of radiaCode.init()")


if __name__ == '__main__':
    failures = []
    tests = [
        test_init_called_after_connect,
        test_init_inside_ok_block,
        test_init_not_called_before_connect,
        test_init_result_logged,
    ]
    for t in tests:
        try:
            t()
        except AssertionError as e:
            print(f"FAIL: {t.__name__}: {e}")
            failures.append(t.__name__)
        except Exception as e:
            print(f"ERROR: {t.__name__}: {e}")
            failures.append(t.__name__)

    if failures:
        print(f"\n{len(failures)} test(s) failed: {', '.join(failures)}")
        sys.exit(1)
    else:
        print(f"\nAll {len(tests)} tests passed.")
