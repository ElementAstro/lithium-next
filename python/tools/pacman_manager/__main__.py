#!/usr/bin/env python3
"""
Main entry point for Pacman Manager Package.
Provides modern CLI interface with enhanced features.
"""

from __future__ import annotations

import asyncio
import sys
from typing import NoReturn

from .cli import CLI


def main() -> NoReturn:
    """Main entry point for the pacman manager."""
    try:
        cli = CLI()
        exit_code = cli.run()
        sys.exit(exit_code)
    except KeyboardInterrupt:
        print("\n⚠️  Operation cancelled by user", file=sys.stderr)
        sys.exit(130)  # Standard exit code for SIGINT
    except Exception as e:
        print(f"❌ Fatal error: {e}", file=sys.stderr)
        sys.exit(1)


async def async_main() -> NoReturn:
    """Async main entry point."""
    try:
        cli = CLI()
        exit_code = await cli.async_run()
        sys.exit(exit_code)
    except KeyboardInterrupt:
        print("\n⚠️  Operation cancelled by user", file=sys.stderr)
        sys.exit(130)
    except Exception as e:
        print(f"❌ Fatal error: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    # Check if we should run in async mode based on arguments
    if "--async" in sys.argv:
        sys.argv.remove("--async")
        asyncio.run(async_main())
    else:
        main()
