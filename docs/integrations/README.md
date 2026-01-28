# Platform Integrations

Platform-specific integration guides for Kryon.

## Plan 9 / 9P Integration

- **[PLAN9_9P_INTEGRATION.md](PLAN9_9P_INTEGRATION.md)** - Complete guide for integrating with Plan 9 / 9P filesystems

This guide covers:
- Using Plan9Port's lib9pclient library
- 9P2000 protocol client implementation
- Integration architecture and examples
- Building with 9P support

## Overview

Kryon supports integration with Plan 9 and Inferno systems through standard 9P client libraries. The integration allows Kryon applications to access 9P filesystems over network or local sockets without requiring the Inferno emu runtime.

**Recommended approach:** Use Plan9Port's lib9pclient for standalone 9P filesystem access.
