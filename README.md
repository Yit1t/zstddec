# zstddec – a Zstandard decompressor

## Introduction
This is a Zstandard decode plugin for GStreamer, which accept a stream compressed with Zstandard and output an uncompressed stream. The plugin inherits directly from `GstElement` and implements a streaming decompressor using the zstd `ZSTD_DCtx` API.

## Usage
```bash
gst-launch-1.0 filesrc location=file.txt.zst ! zstddec ! filesink location=file.txt
```

The output is identical to decompressing with the `zstd` command-line tool:

```bash
zstd -d file.txt.zst > file.txt
```
### Properties
| Property | Type | Default | Description |
| -------- | ---- | ------- | ----------- |
| `buffer-size` | uint | 1024 | Output buffer size for decompressed data |
| `first-buffer-size` | uint | 1024 | Output buffer size for the first buffer (used for content type detection) |

Example with custom buffer size:

```bash
gst-launch-1.0 filesrc location=file.txt.zst ! zstddec buffer-size=4096 ! filesink location=file.txt
```

## Building
### Clone

```bash
git clone https://github.com/Yit1t/zstddec.git
cd zstddec
```

### Dependencies

- GStreamer 1.0 (`gstreamer-1.0`, `gstreamer-base-1.0` via pkg-config)
- libzstd (`libzstd` via pkg-config)
- Meson build system
- Ninja

### Install dependencies (Ubuntu/Debian)

```bash
sudo apt-get install meson ninja-build gcc pkg-config \
    libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev libzstd-dev \
    gstreamer1.0-tools zstd
```

### Compile

```bash
meson setup build
meson compile -C build
```

### Verify the plugin is recognized

```bash
GST_PLUGIN_PATH=./build gst-inspect-1.0 zstddec
```

## Testing
```bash
echo "hello world" > file.txt
zstd file.txt -o file.txt.zst
zstd -d file.txt.zst -o expected.txt
GST_PLUGIN_PATH=./build gst-launch-1.0 \
    filesrc location=file.txt.zst ! zstddec ! filesink location=actual.txt
diff expected.txt actual.txt
```

### Docker (Linux x86-64 verification)

```bash
docker build -t zstddec-test .
docker run -it zstddec-test bash
# Then run the tests above inside the container
```
### CI

GitHub Actions runs build and test automatically on every push. See `.github/workflows/ci.yml`.

## Project Structure

```
zstddec/
├── AUTHORS
├── COPYING
├── meson.build              # Build configuration
├── Dockerfile               # Linux build verification
├── .gitignore
├── .dockerignore
├── .github/workflows/
│   └── ci.yml               # GitHub Actions CI
└── src/
    ├── gstzstddec.h         # Struct definition and type macros
    └── gstzstddec.c         # Plugin implementation
```
## References
1. [An example of zstd decompression](https://github.com/facebook/zstd/blob/dev/examples/streaming_decompression.c)
2. [The Basics of Writing a GStreamer Plugin](https://gstreamer.freedesktop.org/documentation/plugin-development/basics/index.html)
3. [GStreamer Tutorials](https://gstreamer.freedesktop.org/documentation/tutorials/basic/index.html?gi-language=c)
4. [The code of a gz decoder](https://github.com/Snec/gst-gz/blob/master/src/gstgzdec.c#L421)