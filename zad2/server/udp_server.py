import socket
import sys
import time

BUFSIZE = 512


def main():
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8888
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.bind(("0.0.0.0", port))
    print(f"UDP server listening on port {port}", flush=True)
    expected = 0
    received = 0
    duplicates = 0
    delay_sum = 0.0
    while True:
        try:
            data, addr = s.recvfrom(BUFSIZE)
        except KeyboardInterrupt:
            break

        if len(data) != BUFSIZE:
            continue

        parts = data.decode("ascii", errors="ignore").split()
        if len(parts) < 3:
            continue

        try:
            seq = int(parts[0])
            send_ts = float(parts[1])
            msg_type = parts[2]
        except ValueError:
            continue

        if seq not in (0, 1) or msg_type not in ("DATA", "FIN"):
            continue

        ack = f"ACK {seq}".encode()
        if seq == expected:
            expected = 1 - expected
            if msg_type == "DATA":
                delay_sum += time.time() * 1000.0 - send_ts
                received += 1
            else:
                for _ in range(5):
                    s.sendto(ack, addr)
                avg_delay = delay_sum / received if received else 0.0
                print(f"Session from {addr[0]}:{addr[1]} - received: {received}, duplicates: {duplicates}, avg_delay: {avg_delay:.2f} ms", flush=True)
                expected = 0
                received = 0
                duplicates = 0
                delay_sum = 0.0
                continue
        else:
            duplicates += 1

        s.sendto(ack, addr)
    s.close()


if __name__ == "__main__":
    main()
