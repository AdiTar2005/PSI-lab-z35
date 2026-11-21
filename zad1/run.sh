#!/bin/bash
set -e

NETWORK="z35_network"
SERVER_NAME="z35_server"
CLIENT_NAME="z35_client"
PORT="8888"
TOTAL_MB="1"

cleanup() {
    echo ""
    echo "=== Sprzatanie kontenerow ==="
    docker rm -f $CLIENT_NAME $SERVER_NAME 2>/dev/null || true
}

trap cleanup EXIT INT TERM

cleanup

echo "=== Budowanie obrazow Dockera ==="
docker build -t z35_server_img ./server
docker build -t z35_client_img ./client

echo "=== Uruchamianie serwera w sieci $NETWORK ==="
docker run -d --init --name $SERVER_NAME --network $NETWORK z35_server_img
sleep 1

for BUF in 100 1024 10240; do
    echo ">>> TEST: bufor klienta = ${BUF} B, wolumen = ${TOTAL_MB} MB <<<"
    docker run -i --rm --init --name $CLIENT_NAME --network $NETWORK z35_client_img $SERVER_NAME $PORT $BUF $TOTAL_MB
    sleep 1
done

echo "=== Logi serwera ==="
docker logs $SERVER_NAME