#!/bin/bash

# Isolated test runner for expiration/eviction commands.

cleanup_keys() {
    for k in "$@"; do
        ./client del "$k" >/dev/null 2>&1 || true
    done
}

test_ttl_expiry() {
    echo "Testing TTL (Time-To-Live) expiration..."
    local key="ttl_key"
    cleanup_keys "$key"

    ./client set ex "$key" test_value 5
    echo "Set $key with 5 second TTL"
    ./client get "$key"
    echo "Getting $key immediately (should work)"
    sleep 6
    ./client get "$key"
    echo "Getting $key after 6 seconds (should be expired)"

    cleanup_keys "$key"
}

test_lru() {
    echo "Testing LRU (Least Recently Used) eviction..."
    local k1="lru_k1" k2="lru_k2" k3="lru_k3"
    cleanup_keys "$k1" "$k2" "$k3"

    ./client set "$k1" value1
    ./client set "$k2" value2
    ./client set "$k3" value3
    echo "Set 3 keys: $k1 $k2 $k3"
    ./client get "$k1"
    echo "Accessed $k1 (moves to front of LRU)"
    ./client lru_evict
    echo "Evicted least recently used key"
    ./client get "$k2"
    echo "Getting $k2 (should work if it wasn't evicted)"
    ./client get "$k3"
    echo "Getting $k3 (should work if it wasn't evicted)"

    cleanup_keys "$k1" "$k2" "$k3"
}

test_lfu() {
    echo "Testing LFU (Least Frequently Used) eviction..."
    local k1="lfu_k1" k2="lfu_k2"
    cleanup_keys "$k1" "$k2"

    ./client set "$k1" lfu_value1
    ./client set "$k2" lfu_value2
    ./client get "$k1"
    ./client get "$k1"
    ./client get "$k1"
    echo "Accessed $k1 3 times"
    ./client get "$k2"
    echo "Accessed $k2 1 time"
    ./client lfu_evict
    echo "Evicted least frequently used key"
    ./client get "$k1"
    echo "Getting $k1 (should work - higher frequency)"
    ./client get "$k2"
    echo "Getting $k2 (should be expired if it was evicted)"

    cleanup_keys "$k1" "$k2"
}

test_ttl_cmd() {
    echo "Testing TTL command..."
    local key="ttl_cmd_key"
    cleanup_keys "$key"

    ./client set ex "$key" ttl_value 10
    ./client ttl "$key"
    echo "Checking TTL for $key key"

    cleanup_keys "$key"
}

run_all() {
    test_ttl_expiry
    echo ""
    test_lru
    echo ""
    test_lfu
    echo ""
    test_ttl_cmd
}

case "$1" in
    ttl_expiry)
        test_ttl_expiry ;;
    lru)
        test_lru ;;
    lfu)
        test_lfu ;;
    ttl_cmd)
        test_ttl_cmd ;;
    ""|all)
        run_all ;;
    *)
        echo "Unknown test: $1" ; exit 1 ;;
esac
