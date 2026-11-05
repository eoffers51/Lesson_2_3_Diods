"""Live SEGGER SystemView over a COM port via SystemView's IP recorder.

The trace firmware (sdkconfig.sysview_uart) streams SystemView data on UART0,
which DevKits wire to their USB-UART bridge (the COM port). ESP-IDF's UART
transport does not answer SystemView's "hello" handshake, so SystemView's own
UART recorder stops with "Error while recording". This bridge answers the
hello and then just copies bytes both ways:

    SystemView --TCP 19111--> this bridge <--COM port 1 Mbaud--> ESP32

It also resets the board (RTS pulse, DevKit auto-reset circuit) before every
recording, so SystemView always gets the system description and task list.

Usage (ESP-IDF Python, it has pyserial; trace firmware flashed):
    python tools/sysview_bridge.py --port COM5
SystemView: Target > Recorder Configuration > IP, 127.0.0.1, port 19111,
then Target > Start Recording (F5). Ctrl+C here frees the COM port.

Tested: esp_sysview 1.0.2 (SystemView target protocol 3.32), SystemView V4.10b.
"""
import argparse
import socket
import threading
import time

import serial

# Target hello as in SEGGER's recorder samples: 'S', 'V', then the target's
# SystemView version as (VERSION / 10000, (VERSION / 1000) % 10), padded to
# 32 bytes (the IP recorder's hello size). VERSION = MAJOR*10000 + MINOR*100 + REV
# from SEGGER_SYSVIEW.h in the esp_sysview component (1.0.2: 3.32.0).


def target_hello(major: int, minor: int, rev: int = 0) -> bytes:
    version = major * 10000 + minor * 100 + rev
    return bytes([ord("S"), ord("V"), version // 10000, (version // 1000) % 10]) + bytes(28)


def reset_board(ser: serial.Serial) -> None:
    """Pulse EN through the CH343's RTS line, then wait for the app to boot.

    After a reset the firmware waits silently for SystemView's START command,
    and START makes it send the system description and the task list first.
    """
    ser.dtr = False
    ser.rts = True
    time.sleep(0.1)
    ser.rts = False
    time.sleep(1.5)
    ser.reset_input_buffer()


def pump_serial_to_tcp(ser, conn, stop, stats):
    while not stop.is_set():
        data = ser.read(4096)
        if data:
            try:
                conn.sendall(data)
            except OSError:
                break
            stats["up"] += len(data)
    stop.set()


def pump_tcp_to_serial(ser, conn, stop, stats):
    while not stop.is_set():
        try:
            data = conn.recv(256)
        except socket.timeout:
            continue
        except OSError:
            break
        if not data:
            break
        stats["down"] += len(data)
        print(f"  SystemView -> board: {data.hex(' ')}")
        ser.write(data)
    stop.set()


def serve(args):
    ser = serial.Serial(args.port, args.baud, timeout=0.05)
    print(f"Resetting the board on {args.port} ...")
    reset_board(ser)

    srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind(("127.0.0.1", args.tcp))
    srv.listen(1)
    print(f"Waiting for SystemView on 127.0.0.1:{args.tcp} (IP recorder). Ctrl+C to quit.")

    while True:
        conn, addr = srv.accept()
        conn.settimeout(0.3)
        print(f"SystemView connected from {addr[0]}:{addr[1]}")

        # SystemView first sends its own hello. Read it, log it, don't forward it.
        hello = b""
        deadline = time.time() + 1.0
        while time.time() < deadline and len(hello) < 32:
            try:
                chunk = conn.recv(32 - len(hello))
            except socket.timeout:
                break
            if not chunk:
                break
            hello += chunk
        print(f"  host hello ({len(hello)} bytes): {hello.hex(' ')}")
        conn.sendall(target_hello(args.sv_major, args.sv_minor))

        stop = threading.Event()
        stats = {"up": 0, "down": 0}
        t1 = threading.Thread(target=pump_serial_to_tcp, args=(ser, conn, stop, stats), daemon=True)
        t2 = threading.Thread(target=pump_tcp_to_serial, args=(ser, conn, stop, stats), daemon=True)
        t1.start()
        t2.start()
        last = time.time()
        while not stop.is_set():
            time.sleep(0.5)
            if time.time() - last >= 5:
                print(f"  streaming: {stats['up'] / 1024:.1f} KiB board -> SystemView so far")
                last = time.time()
        conn.close()
        print(f"SystemView disconnected after {stats['up'] / 1024:.1f} KiB.")
        # Reset again so the next recording also starts with the task list.
        reset_board(ser)
        print("Board reset. Waiting for the next recording.")


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--port", default="COM5")
    p.add_argument("--baud", type=int, default=1000000)
    p.add_argument("--tcp", type=int, default=19111)
    p.add_argument("--sv-major", type=int, default=3, help="SEGGER_SYSVIEW_MAJOR of the firmware")
    p.add_argument("--sv-minor", type=int, default=32, help="SEGGER_SYSVIEW_MINOR of the firmware")
    serve(p.parse_args())


if __name__ == "__main__":
    main()
