#!/usr/bin/env python3
"""The browser harness must time out even when Chrome stops reading requests."""
import os
import signal
import time
from types import SimpleNamespace

from site_browser_test import Browser, run_step, test_step


def main():
    previous = signal.getsignal(signal.SIGALRM)
    reader, writer = os.pipe()
    started = time.monotonic()
    try:
        try:
            with test_step('blocked I/O guard', .05):
                os.read(reader, 1)
        except TimeoutError as error:
            assert 'blocked I/O guard' in str(error)
        else:
            raise AssertionError('Blocked I/O was not interrupted')
        assert time.monotonic() - started < 2
        assert signal.getsignal(signal.SIGALRM) == previous
        assert signal.getitimer(signal.ITIMER_REAL)[0] == 0
    finally:
        os.close(reader)
        os.close(writer)
    assert run_step('successful guard', lambda: 42, seconds=.1) == 42

    reader, writer = os.pipe()
    os.set_blocking(writer, False)
    with os.fdopen(writer, 'wb', buffering=0) as pipe:
        browser = object.__new__(Browser)
        browser.sequence, browser.session = 0, None
        browser.process = SimpleNamespace(stdin=pipe)
        started = time.monotonic()
        try:
            try:
                browser.command('unresponsive browser', timeout=.05, payload='x' * 1048576)
            except TimeoutError as error:
                assert 'writing browser request' in str(error)
            else:
                raise AssertionError('Full browser pipe did not time out')
            assert time.monotonic() - started < 2
        finally:
            os.close(reader)
    print('Local browser deadline tests passed: blocked I/O and full request pipe are bounded', flush=True)


if __name__ == '__main__': main()
