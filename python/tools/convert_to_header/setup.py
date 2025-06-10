#!/usr/bin/env python3
# -*- coding: utf-8 -*-

from setuptools import setup, find_packages

setup(
    name="convert_to_header",
    version="2.0.0",
    packages=find_packages(),
    install_requires=[
        "loguru>=0.6.0",
    ],
    extras_require={
        'yaml': ['PyYAML>=6.0'],
        'pybind': ['pybind11>=2.10.0'],
        'progress': ['tqdm>=4.64.0'],
    },
    entry_points={
        'console_scripts': [
            'convert_to_header=convert_to_header.cli:main',
        ],
    },
    author="Max Qian",
    author_email="astro_air@126.com",
    description="Convert binary files to C/C++ headers and vice versa",
    long_description=open("README.md").read(),
    long_description_content_type="text/markdown",
    url="https://github.com/yourusername/convert_to_header",
    classifiers=[
        "Development Status :: 5 - Production/Stable",
        "License :: OSI Approved :: MIT License",
        "Programming Language :: Python :: 3.9",
        "Programming Language :: Python :: 3.10",
        "Programming Language :: Python :: 3.11",
        "Topic :: Software Development :: Code Generators",
    ],
    python_requires='>=3.9',
)
