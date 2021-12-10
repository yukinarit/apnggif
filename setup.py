from pathlib import Path
from setuptools import setup, Extension

with open("README.md", "r", encoding="utf8") as f:
    readme = f.read()

zlib = [str(p) for p in Path("zlib").glob("*.c")]

libpng = [str(p) for p in Path("libpng").glob("*.c")]

ext = Extension(
    "apnggif_sys",
    sources=["apng2gif.cpp", "py.cpp"] + zlib + libpng,
    include_dirs=["libpng", "zlib"],
    language="c++",
    extra_compile_args=["-DPNG_ARM_NEON_OPT=0"],
)

setup(
    name="apnggif",
    version="0.1.4",
    description="Python interface to apng2gif",
    long_description=readme,
    long_description_content_type="text/markdown",
    author="yukinarit",
    author_email="yukinarit84@gmail.com",
    url="https://github.com/yukinarit/apnggif",
    py_modules=["apnggif", "apnggif_sys"],
    ext_modules=[ext],
    install_requires=["click", "coloredlogs"],
    entry_points={
        "console_scripts": ["apnggif=apnggif:main"],
    },
    python_requires=">=3.6",
    license="zlib/libpng",
    classifiers=[
        "Development Status :: 4 - Beta",
        "Environment :: Console",
        "Intended Audience :: Developers",
        "License :: OSI Approved :: zlib/libpng License",
        "Programming Language :: Python :: 3.6",
        "Programming Language :: Python :: 3.7",
        "Programming Language :: Python :: 3.8",
        "Programming Language :: Python :: 3.9",
        "Programming Language :: Python :: 3.10",
        "Programming Language :: Python :: Implementation :: CPython",
        "Topic :: Scientific/Engineering :: Image Processing",
    ],
)
