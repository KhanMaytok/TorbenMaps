# Dockerfile to cross-compile planet.c to Windows executable
# This creates a Windows .exe file that can run on Windows 10 or 11

FROM debian:bullseye-slim

# Install MinGW cross-compiler for Windows
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
    gcc-mingw-w64-i686 \
    gcc-mingw-w64-x86-64 \
    make && \
    rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /build

# Copy source files
COPY planet.c .
COPY Makefile .

# Build for Windows (64-bit)
# Using x86_64-w64-mingw32-gcc as the compiler
RUN x86_64-w64-mingw32-gcc -O2 -Wall -D_USE_LIBM_MATH_H -o planet.exe planet.c -lm -static

# The compiled planet.exe can be extracted from the container
CMD ["echo", "planet.exe has been compiled. Use 'docker cp' to extract it."]
