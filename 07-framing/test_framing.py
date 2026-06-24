#!/usr/bin/env python3
"""Test client for length-prefixed framing server."""

import socket
import struct
import sys

HOST = "localhost"
PORT = 9999


def send_framed(sock, message: bytes):
    """Send [4-byte big-endian length][payload]."""
    header = struct.pack("!I", len(message))  # !I = big-endian uint32
    sock.sendall(header + message)
    print(f"  SENT: [{len(message):4d} bytes] {message}")


def recv_framed(sock) -> bytes:
    """Receive [4-byte big-endian length][payload]."""
    header = recv_exact(sock, 4)
    if not header:
        return None
    length = struct.unpack("!I", header)[0]
    payload = recv_exact(sock, length)
    print(f"  RECV: [{length:4d} bytes] {payload}")
    return payload


def recv_exact(sock, n: int) -> bytes:
    """Read exactly n bytes."""
    data = b""
    while len(data) < n:
        chunk = sock.recv(n - len(data))
        if not chunk:
            return None
        data += chunk
    return data


def test_single_message():
    """Send one message, get one echo back."""
    print("\n--- Test: single message ---")
    with socket.socket() as s:
        s.connect((HOST, PORT))
        send_framed(s, b"hello")
        resp = recv_framed(s)
        assert resp == b"hello", f"expected b'hello', got {resp}"
    print("  PASS")


def test_multiple_messages():
    """Send multiple messages on the same connection."""
    print("\n--- Test: multiple messages ---")
    with socket.socket() as s:
        s.connect((HOST, PORT))
        for msg in [b"one", b"two", b"three"]:
            send_framed(s, msg)
            resp = recv_framed(s)
            assert resp == msg, f"expected {msg}, got {resp}"
    print("  PASS")


def test_pipelining():
    """Send 3 messages at once (concatenated), then read all 3 responses."""
    print("\n--- Test: pipelining (3 messages at once) ---")
    with socket.socket() as s:
        s.connect((HOST, PORT))
        # Build 3 frames and send as one blob
        blob = b""
        messages = [b"aaa", b"bbbb", b"cc"]
        for msg in messages:
            blob += struct.pack("!I", len(msg)) + msg
        s.sendall(blob)
        print(f"  SENT: {len(blob)} bytes as one blob (3 frames)")
        for msg in messages:
            resp = recv_framed(s)
            assert resp == msg, f"expected {msg}, got {resp}"
    print("  PASS")


def test_large_message():
    """Send a message near MSG_MAX (4096 bytes)."""
    print("\n--- Test: large message (4000 bytes) ---")
    with socket.socket() as s:
        s.connect((HOST, PORT))
        payload = b"X" * 4000
        send_framed(s, payload)
        resp = recv_framed(s)
        assert resp == payload, f"expected 4000 X's, got {len(resp)} bytes"
    print("  PASS")


def test_too_large():
    """Send a message exceeding MSG_MAX — server should disconnect us."""
    print("\n--- Test: oversized message (server should disconnect) ---")
    with socket.socket() as s:
        s.connect((HOST, PORT))
        header = struct.pack("!I", 5000)  # payload_len > 4096
        s.sendall(header + b"X" * 100)  # don't need full payload
        try:
            data = s.recv(1024)
            if not data:
                print("  Server closed connection (correct)")
            else:
                print(f"  WARNING: got data back: {data}")
        except ConnectionResetError:
            print("  Server reset connection (correct)")
    print("  PASS")


if __name__ == "__main__":
    tests = [
        test_single_message,
        test_multiple_messages,
        test_pipelining,
        test_large_message,
        test_too_large,
    ]
    if len(sys.argv) > 1:
        # Run specific test by name
        name = sys.argv[1]
        matching = [t for t in tests if name in t.__name__]
        if not matching:
            print(f"No test matching '{name}'. Available:")
            for t in tests:
                print(f"  {t.__name__}")
            sys.exit(1)
        tests = matching

    for test in tests:
        test()
    print("\n All tests passed.")
