#!/usr/bin/env python3
"""Host-side AES-256-GCM wrap for the house Wi-Fi PSK.

Copied from Arduino_Wemos_TTgo_D1_R32_ESPDuino-32_Waveshare_4inch_OpenMRN_WiFi
(utils/wifi_wrap.py). Matches sketches/LccWifiTurnoutNode/wifi_cred.cpp:

  IKM  = flash_uid[8] || MAC[6] || node[6]
  info = 05.01.01.01.A5 || MAC[6]
  salt = owlthree-d1r32-wifi-wrap-v1
  HKDF-SHA256 -> 32-byte key
  AES-256-GCM, 12-byte nonce, 16-byte tag, no AAD

The PSK is never written to a project file. Only ciphertext is emitted as
sketches/LccWifiTurnoutNode/wifi_psk_wrap.inc (gitignored).
"""

from __future__ import print_function

import argparse
import hmac
import os
import re
import struct
import sys
import tempfile
from hashlib import sha256

try:
    from pathlib import Path
except ImportError:
    Path = None  # type: ignore

SALT = b"owlthree-d1r32-wifi-wrap-v1"
OWL_PREFIX = bytes([0x05, 0x01, 0x01, 0x01, 0xA5])
WRAP_VER = 1
NONCE_LEN = 12
TAG_LEN = 16
CIPHER_LEN = 64
WRAP_STRUCT = struct.Struct("=B12s16sB64s")
assert WRAP_STRUCT.size == 94

MAC_RE = re.compile(
    r"MAC Address:\s*([0-9A-Fa-f]{2}(?::[0-9A-Fa-f]{2}){5})"
)
NODE_RE = re.compile(
    r"OpenLCB Node ID:\s*([0-9A-Fa-f]{2}(?:\.[0-9A-Fa-f]{2}){5})"
)
UID_RE = re.compile(
    r"SPI flash unique ID:\s*([0-9A-Fa-f]{16}|UNAVAILABLE)"
)


def _die(msg, code=1):
    sys.stderr.write(msg + "\n")
    raise SystemExit(code)


def parse_mac(text):
    parts = text.strip().replace("-", ":").split(":")
    if len(parts) != 6:
        _die("bad MAC: %r" % text)
    return bytes(int(p, 16) for p in parts)


def parse_node(text):
    parts = text.strip().split(".")
    if len(parts) != 6:
        _die("bad node ID: %r" % text)
    return bytes(int(p, 16) for p in parts)


def parse_uid(text):
    text = text.strip().upper()
    if text == "UNAVAILABLE":
        return bytes(8)
    if len(text) != 16 or any(c not in "0123456789ABCDEF" for c in text):
        _die("bad flash UID: %r" % text)
    return bytes.fromhex(text)


def parse_debug_log(text):
    mac_m = MAC_RE.search(text)
    node_m = NODE_RE.search(text)
    uid_m = UID_RE.search(text)
    missing = [
        name
        for name, m in (
            ("MAC Address", mac_m),
            ("OpenLCB Node ID", node_m),
            ("SPI flash unique ID", uid_m),
        )
        if not m
    ]
    if missing:
        _die("serial/log missing: " + ", ".join(missing))
    return {
        "WIFI_MAC": mac_m.group(1).upper(),
        "WIFI_NODE_ID": node_m.group(1).upper(),
        "WIFI_FLASH_UID": uid_m.group(1).upper(),
    }


def load_hw_ids(path):
    data = {}
    with open(path, "r", encoding="utf-8") as handle:
        for raw in handle:
            line = raw.strip()
            if not line or line.startswith("#") or "=" not in line:
                continue
            key, val = line.split("=", 1)
            data[key.strip()] = val.strip()
    for key in ("WIFI_MAC", "WIFI_NODE_ID", "WIFI_FLASH_UID"):
        if key not in data or not data[key]:
            _die("%s missing %s" % (path, key))
    return data


def write_hw_ids(path, ids):
    parent = os.path.dirname(path)
    if parent:
        os.makedirs(parent, exist_ok=True)
    with open(path, "w", encoding="utf-8") as handle:
        handle.write("# Board IDs from DEBUG serial. Not the Wi-Fi password.\n")
        handle.write("WIFI_MAC=%s\n" % ids["WIFI_MAC"])
        handle.write("WIFI_NODE_ID=%s\n" % ids["WIFI_NODE_ID"])
        handle.write("WIFI_FLASH_UID=%s\n" % ids["WIFI_FLASH_UID"])


def derive_key(mac, uid, node):
    if len(mac) != 6 or len(uid) != 8 or len(node) != 6:
        _die("ID lengths must be MAC=6 UID=8 node=6")
    ikm = uid + mac + node
    info = OWL_PREFIX + mac
    prk = hmac.new(SALT, ikm, sha256).digest()
    t = b""
    okm = b""
    block = 1
    while len(okm) < 32:
        t = hmac.new(prk, t + info + bytes([block]), sha256).digest()
        okm += t
        block += 1
    return okm[:32]


def _aesgcm():
    try:
        from cryptography.hazmat.primitives.ciphers.aead import AESGCM
    except ImportError:
        _die("Python cryptography package is required (AES-GCM)")
    return AESGCM


def encrypt_psk(key, psk, nonce=None):
    if not psk or len(psk) > CIPHER_LEN:
        _die("PSK must be 1..64 bytes")
    if nonce is None:
        nonce = os.urandom(NONCE_LEN)
    if len(nonce) != NONCE_LEN:
        _die("nonce must be 12 bytes")
    AESGCM = _aesgcm()
    packed = AESGCM(key).encrypt(nonce, psk, None)
    cipher, tag = packed[:-TAG_LEN], packed[-TAG_LEN:]
    blob = WRAP_STRUCT.pack(
        WRAP_VER, nonce, tag, len(psk), cipher.ljust(CIPHER_LEN, b"\x00")
    )
    return blob


def decrypt_psk(key, blob):
    if len(blob) != WRAP_STRUCT.size:
        _die("wrap blob must be %d bytes" % WRAP_STRUCT.size)
    ver, nonce, tag, clen, cipher = WRAP_STRUCT.unpack(blob)
    if ver != WRAP_VER or clen == 0 or clen > CIPHER_LEN:
        _die("wrap blob header invalid")
    AESGCM = _aesgcm()
    return AESGCM(key).decrypt(nonce, cipher[:clen] + tag, None)


def render_inc(ssid, blob):
    if not ssid or len(ssid) > 32:
        _die("SSID must be 1..32 characters")
    for ch in '"\\\n\r':
        if ch in ssid:
            _die("SSID contains unsupported characters")
    hex_bytes = ", ".join("0x%02X" % b for b in blob)
    return (
        "/* Generated wrap blob. Ciphertext only. Do not commit. */\n"
        'static const char kWifiWrapSsid[] = "%s";\n'
        "static const uint8_t kWifiWrapBlob[%d] = {\n    %s\n};\n"
        % (ssid, len(blob), hex_bytes)
    )


def write_inc(path, ssid, blob):
    parent = os.path.dirname(path)
    if parent:
        os.makedirs(parent, exist_ok=True)
    with open(path, "w", encoding="utf-8") as handle:
        handle.write(render_inc(ssid, blob))


def _read_psk_stdin():
    data = sys.stdin.buffer.read()
    if data.endswith(b"\n"):
        data = data[:-1]
    if data.endswith(b"\r"):
        data = data[:-1]
    if not data:
        _die("PSK on stdin is empty")
    return data


def cmd_encrypt(args):
    ids = load_hw_ids(args.ids)
    mac = parse_mac(ids["WIFI_MAC"])
    node = parse_node(ids["WIFI_NODE_ID"])
    uid = parse_uid(ids["WIFI_FLASH_UID"])
    psk = _read_psk_stdin()
    key = derive_key(mac, uid, node)
    blob = encrypt_psk(key, psk)
    if decrypt_psk(key, blob) != psk:
        _die("internal wrap check failed")
    write_inc(args.out, args.ssid, blob)
    print("Wrote ciphertext wrap (%d bytes) to %s" % (len(blob), args.out))
    print("PSK was not written to any file.")


def cmd_parse_log(args):
    with open(args.log, "r", encoding="utf-8", errors="replace") as handle:
        text = handle.read()
    ids = parse_debug_log(text)
    write_hw_ids(args.out, ids)
    print("Wrote %s" % args.out)
    for key in ("WIFI_MAC", "WIFI_NODE_ID", "WIFI_FLASH_UID"):
        print("%s=%s" % (key, ids[key]))


def cmd_selftest(_args):
    fake_log = (
        "I (123) lcc_wifi: boot\n"
        "==== DEBUG IDs (not the WiFi PSK) ====\n"
        "MAC Address: DE:AD:BE:EF:00:01\n"
        "OpenLCB Node ID: 05.01.01.01.A5.03\n"
        "SPI flash unique ID: 0123456789ABCDEF\n"
    )
    ids = parse_debug_log(fake_log)
    if ids["WIFI_MAC"] != "DE:AD:BE:EF:00:01":
        _die("selftest MAC parse failed")
    if ids["WIFI_NODE_ID"] != "05.01.01.01.A5.03":
        _die("selftest node parse failed")
    if ids["WIFI_FLASH_UID"] != "0123456789ABCDEF":
        _die("selftest UID parse failed")

    unavail = fake_log.replace("0123456789ABCDEF", "UNAVAILABLE")
    uid0 = parse_uid(parse_debug_log(unavail)["WIFI_FLASH_UID"])
    if uid0 != bytes(8):
        _die("selftest UNAVAILABLE UID must be 8 zero bytes")

    mac = parse_mac(ids["WIFI_MAC"])
    node = parse_node(ids["WIFI_NODE_ID"])
    uid = parse_uid(ids["WIFI_FLASH_UID"])
    key = derive_key(mac, uid, node)
    psk = b"fake-psk-not-a-house-password"
    nonce = bytes(range(12))
    blob = encrypt_psk(key, psk, nonce=nonce)
    if decrypt_psk(key, blob) != psk:
        _die("selftest decrypt mismatch")

    bad_key = derive_key(bytes([0x00]) * 6, uid, node)
    try:
        decrypt_psk(bad_key, blob)
    except Exception:
        pass
    else:
        _die("selftest expected GCM failure on wrong MAC")

    tmp = tempfile.mkdtemp()
    try:
        inc = os.path.join(tmp, "wifi_psk_wrap.inc")
        ids_path = os.path.join(tmp, "hw_ids.env")
        write_hw_ids(ids_path, ids)
        write_inc(inc, "SRIF2333", blob)
        with open(inc, "r", encoding="utf-8") as handle:
            text = handle.read()
        if psk.decode("ascii") in text:
            _die("selftest leaked PSK into wrap include")
        if "fake-psk" in text:
            _die("selftest leaked PSK fragment into wrap include")
        if "kWifiWrapBlob[94]" not in text:
            _die("selftest wrap include size mismatch")
    finally:
        for name in os.listdir(tmp):
            os.remove(os.path.join(tmp, name))
        os.rmdir(tmp)

    print("wifi_wrap selftest passed")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="cmd", required=True)

    p_enc = sub.add_parser("encrypt", help="Read PSK from stdin; write ciphertext .inc")
    p_enc.add_argument("--ids", required=True, help="local/hw_ids.env")
    p_enc.add_argument("--ssid", default="SRIF2333")
    p_enc.add_argument("--out", required=True, help="wifi_psk_wrap.inc")
    p_enc.set_defaults(func=cmd_encrypt)

    p_log = sub.add_parser("parse-log", help="Parse a saved DEBUG serial log")
    p_log.add_argument("--log", required=True)
    p_log.add_argument("--out", required=True)
    p_log.set_defaults(func=cmd_parse_log)

    p_t = sub.add_parser("selftest", help="Round-trip crypto and parser with fake data")
    p_t.set_defaults(func=cmd_selftest)

    args = parser.parse_args()
    args.func(args)


if __name__ == "__main__":
    main()
