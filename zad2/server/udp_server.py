import sys
import time
from socket import *

BUFSIZE = 512
TIMEOUT = 5.0


def moreWork():
    return True


def main():
    if len(sys.argv) < 2:
        print("no port, using 8888")
        port = 8888
    else:
        port = int(sys.argv[1])

    host = "0.0.0.0"
    print("Will listen on", host, ":", port, flush=True)
    s = socket(AF_INET, SOCK_DGRAM)
    s.bind((host, port))
    s.settimeout(TIMEOUT)
    expected_seq = 0
    received_count = 0
    duplicates_count = 0
    delay_sum = 0.0
    while moreWork():
        try:
            data, addr = s.recvfrom(BUFSIZE)
        except timeout:
            continue
        except KeyboardInterrupt:
            print("recvfrom() interrupted, ending.")
            break

        if len(data) != BUFSIZE:
            print("Invalid datagram size:", len(data))
            continue

        parts = data.decode("ascii", errors="ignore").split()
        if len(parts) < 3:
            print("Invalid datagram format")
            continue

        try:
            seq = int(parts[0])
            send_ts = float(parts[1])
            msg_type = parts[2]
        except ValueError:
            print("Malformed header")
            continue

        if seq not in (0, 1) or msg_type not in ("DATA", "FIN"):
            continue

        ack_msg = f"ACK {seq}".encode("ascii")
        if seq == expected_seq:
            expected_seq = 1 - expected_seq
            if msg_type == "DATA":
                recv_ts = time.time() * 1000.0
                delay_sum += (recv_ts - send_ts)
                received_count += 1
            elif msg_type == "FIN":
                for _ in range(5):
                    s.sendto(ack_msg, addr)
                avg_delay = (delay_sum / received_count) if received_count > 0 else 0.0
                print(
                    f"Session done from {addr}: received={received_count}, "
                    f"duplicates={duplicates_count}, avg_delay={avg_delay:.2f} ms",
                    flush=True,
                )
                expected_seq = 0
                received_count = 0
                duplicates_count = 0
                delay_sum = 0.0
                continue
        else:
            duplicates_count += 1
        s.sendto(ack_msg, addr)
    s.close()


if __name__ == "__main__":
    main()
