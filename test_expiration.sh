#!/bin/bash

# Test script for expiration mechanisms
echo "Testing TTL (Time-To-Live) expiration..."
./client set ex test_key test_value 5
echo "Set key with 5 second TTL"
./client get test_key
echo "Getting key immediately (should work)"
sleep 6
./client get test_key
echo "Getting key after 6 seconds (should be expired)"

echo ""
echo "Testing LRU (Least Recently Used) eviction..."
./client set key1 value1
./client set key2 value2
./client set key3 value3
echo "Set 3 keys"
./client get key1
echo "Accessed key1 (moves to front of LRU)"
./client lru_evict
echo "Evicted least recently used key"
./client get key2
echo "Getting key2 (should work if it wasn't evicted)"
./client get key3
echo "Getting key3 (should work if it wasn't evicted)"

echo ""
echo "Testing LFU (Least Frequently Used) eviction..."
./client set lfu_key1 lfu_value1
./client set lfu_key2 lfu_value2
./client get lfu_key1
./client get lfu_key1
./client get lfu_key1
echo "Accessed lfu_key1 3 times"
./client get lfu_key2
echo "Accessed lfu_key2 1 time"
./client lfu_evict
echo "Evicted least frequently used key"
./client get lfu_key1
echo "Getting lfu_key1 (should work - higher frequency)"
./client get lfu_key2
echo "Getting lfu_key2 (should be expired if it was evicted)"

echo ""
echo "Testing TTL command..."
./client set ex ttl_test ttl_value 10
./client ttl ttl_test
echo "Checking TTL for ttl_test key"

