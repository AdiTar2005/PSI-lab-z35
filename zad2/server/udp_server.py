import socket
import sys
import time

BUFSIZE = 512


def main():
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8888
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.bind(("0.0.0.0", port))
    print(f"UDP server listening on port {port}", flush=True)
    cur_sid = None
    last_sid = None
    last_finish_time = 0.0
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
        if len(parts) < 4:
            continue

        try:
            sid = int(parts[0])
            seq = int(parts[1])
            send_ts = float(parts[2])
            msg_type = parts[3]
        except ValueError:
            continue

        if seq not in (0, 1) or msg_type not in ("DATA", "FIN"):
            continue

        ack = f"ACK {sid} {seq}".encode()
        if sid == last_sid and (time.time() - last_finish_time) < 1.5:
            if msg_type == "FIN":
                s.sendto(ack, addr)
            continue

        if cur_sid != sid:
            cur_sid = sid
            expected = 0
            received = 0
            duplicates = 0
            delay_sum = 0.0

        if seq == expected:
            expected = 1 - expected
            if msg_type == "DATA":
                delay_sum += time.time() * 1000.0 - send_ts
                received += 1
            else:
                s.sendto(ack, addr)
                avg_delay = delay_sum / received if received else 0.0
                print(f"Session {sid} from {addr[0]}:{addr[1]} - received: {received}, duplicates: {duplicates}, avg_delay: {avg_delay:.2f} ms", flush=True)
                last_sid = sid
                last_finish_time = time.time()
                cur_sid = None
                continue
        else:
            duplicates += 1

        s.sendto(ack, addr)
    s.close()


if __name__ == "__main__":
    main()
