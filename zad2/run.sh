#!/bin/bash
set -e

NETWORK="z35_network"
SERVER_IMAGE="z35_udp_server"
CLIENT_IMAGE="z35_udp_client"
SERVER_CONTAINER="z35_udp_server_c"
CLIENT_CONTAINER="z35_udp_client_c"

docker build --tag $SERVER_IMAGE ./server
docker build --tag $CLIENT_IMAGE ./client

docker rm -f $SERVER_CONTAINER $CLIENT_CONTAINER >/dev/null 2>&1 || true

docker run -d --name $SERVER_CONTAINER --network $NETWORK --cap-add=NET_ADMIN $SERVER_IMAGE

sleep 1

for LOSS in 0 5 15 30; do
    echo "=== Test: packet loss = ${LOSS}% ==="
    docker exec $SERVER_CONTAINER tc qdisc replace dev eth0 root netem loss ${LOSS}%
    
    docker run --rm --name $CLIENT_CONTAINER --network $NETWORK --cap-add=NET_ADMIN $CLIENT_IMAGE \
        sh -c "tc qdisc add dev eth0 root netem loss ${LOSS}% && ./client $SERVER_CONTAINER 8888 500 50"
done

echo "=== Server logs ==="
docker logs $SERVER_CONTAINER
docker rm -f $SERVER_CONTAINER >/dev/null