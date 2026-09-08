# Dev tool. A stand-in video source for the camera panel (IOT-75), so the live
# view can be built and demonstrated without hardware.
#
#   python backend-api/tools/fake_camera.py        # serves :8090/stream
#
# Serves multipart/x-mixed-replace, the same shape an ESP32-CAM's MJPEG endpoint
# uses, so the frontend needs no special case for the fake. Frames are PNG
# rather than JPEG because the standard library can encode PNG and cannot encode
# JPEG; browsers render either in a multipart stream.
#
# Nothing here touches the ledger. Media never enters a block (ADR 0011) — the
# ledger only records that a stream came online, and where.

import os
import struct
import time
import zlib
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

PORT = int(os.environ.get("CAMERA_PORT", "8090"))
WIDTH = int(os.environ.get("CAMERA_WIDTH", "320"))
HEIGHT = int(os.environ.get("CAMERA_HEIGHT", "240"))
FPS = float(os.environ.get("CAMERA_FPS", "5"))
BOUNDARY = "frameboundary"


def _chunk(tag: bytes, payload: bytes) -> bytes:
    return (
        struct.pack(">I", len(payload))
        + tag
        + payload
        + struct.pack(">I", zlib.crc32(tag + payload) & 0xFFFFFFFF)
    )


def png_frame(tick: int) -> bytes:
    """A frame with a moving bar, so it is obvious at a glance that the stream is
    live rather than a still image."""
    bar_x = int((tick * 7) % WIDTH)
    rows = bytearray()
    for y in range(HEIGHT):
        rows.append(0)  # PNG filter type 0 for this scanline
        for x in range(WIDTH):
            if abs(x - bar_x) < 12:
                rows += bytes((60, 200, 110))
            else:
                shade = 30 + (y * 90 // HEIGHT)
                rows += bytes((shade, shade, shade + 12))

    header = struct.pack(">IIBBBBB", WIDTH, HEIGHT, 8, 2, 0, 0, 0)
    return (
        b"\x89PNG\r\n\x1a\n"
        + _chunk(b"IHDR", header)
        + _chunk(b"IDAT", zlib.compress(bytes(rows), 6))
        + _chunk(b"IEND", b"")
    )


class Handler(BaseHTTPRequestHandler):
    # Name is BaseHTTPRequestHandler's interface, not a choice.
    def do_GET(self) -> None:
        if self.path.startswith("/stream"):
            self._stream()
        elif self.path.startswith("/health"):
            self._json(b'{"status":"ok"}')
        else:
            self.send_error(404)

    def _json(self, body: bytes) -> None:
        self.send_response(200)
        self.send_header("content-type", "application/json")
        # The dashboard is served from another origin; without this the browser
        # blocks the stream with no error visible server-side.
        self.send_header("access-control-allow-origin", "*")
        self.send_header("content-length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def _stream(self) -> None:
        self.send_response(200)
        self.send_header("access-control-allow-origin", "*")
        self.send_header("cache-control", "no-store")
        self.send_header(
            "content-type", f"multipart/x-mixed-replace; boundary={BOUNDARY}"
        )
        self.end_headers()

        tick = 0
        try:
            while True:
                frame = png_frame(tick)
                self.wfile.write(f"--{BOUNDARY}\r\n".encode())
                self.wfile.write(b"content-type: image/png\r\n")
                self.wfile.write(f"content-length: {len(frame)}\r\n\r\n".encode())
                self.wfile.write(frame)
                self.wfile.write(b"\r\n")
                self.wfile.flush()
                tick += 1
                time.sleep(1.0 / FPS)
        except (BrokenPipeError, ConnectionResetError):
            # The viewer closed the tab. Normal, not an error.
            pass

    def log_message(self, *args) -> None:
        return  # one line per frame would be unreadable


def main() -> int:
    # Threading: an <img> holds its connection open for the life of the page, so
    # a single-threaded server would serve exactly one viewer.
    server = ThreadingHTTPServer(("0.0.0.0", PORT), Handler)
    print(f"fake camera on http://0.0.0.0:{PORT}/stream ({WIDTH}x{HEIGHT} @ {FPS}fps)")
    server.serve_forever()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
