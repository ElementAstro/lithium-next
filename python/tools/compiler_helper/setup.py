#!/usr/bin/env python3
# -*- coding: utf-8 -*-
from setuptools import setup, find_packages

setup(
    name="compiler_helper",
    version="0.1.0",
    description="Enhanced compiler helper tool for C++ projects",
    author="Max Qian",
    author_email="lightapt@example.com",
    packages=find_packages(),
    python_requires=">=3.8",
    install_requires=[
        "loguru>=0.5.0",
    ],
    extras_require={
        "dev": [
            "pytest>=6.0.0",
            "black>=21.5b2",
            "mypy>=0.812",
        ],
        "pybind": ["pybind11>=2.6.0"],
    },
    entry_points={
        "console_scripts": [
            "compiler_helper=compiler_helper_lib.cli:main",
        ],
    },
    classifiers=[
        "Development Status :: 4 - Beta",
        "Intended Audience :: Developers",
        "License :: OSI Approved :: GNU General Public License v3 or later (GPLv3+)",
        "Programming Language :: Python :: 3",
        "Programming Language :: Python :: 3.8",
        "Programming Language :: Python :: 3.9",
        "Programming Language :: Python :: 3.10",
        "Programming Language :: Python :: 3.11",
        "Topic :: Software Development :: Build Tools",
    ],
)
