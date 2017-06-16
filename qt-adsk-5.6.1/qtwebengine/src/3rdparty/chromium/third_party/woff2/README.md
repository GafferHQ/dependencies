﻿This is a README for the font compression reference code. There are several
compression related modules in this repository.

brotli/ contains reference code for the Brotli byte-level compression
algorithm. Note that it is licensed under an Apache 2 license.

src/ contains the C++ code for compressing and decompressing fonts.

# Build & Run

This document documents how to run the compression reference code. At this
writing, the code, while it is intended to produce a bytestream that can be
reconstructed into a working font, the reference decompression code is not
done, and the exact format of that bytestream is subject to change.

## Build

On a standard Unix-style environment:

```
git clone https://github.com/google/woff2.git
cd woff2
git submodule init
git submodule update
make clean all
```

## Run

```
woff2_compress myfont.ttf
woff2_decompress myfont.woff2
```

# References

http://www.w3.org/TR/WOFF2/
http://www.w3.org/Submission/MTX/

Also please refer to documents (currently Google Docs):

WOFF Ultra Condensed file format: proposals and discussion of wire format
issues (PDF is in docs/ directory)

WIFF Ultra Condensed: more discussion of results and compression techniques.
This tool was used to prepare the data in that document.
