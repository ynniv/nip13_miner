#!/bin/bash

# Parallel NIP-13 Miner Comparison Demo
# Shows the performance improvement of the parallel version

# Build both miners
if [ ! -f "nip13_miner" ] || [ ! -f "nip13_parallel" ]; then
    echo "📦 Building miners..."
    make all
    echo ""
fi

# Create test event
cat > demo_event.json << EOF
{"id":"","pubkey":"32e1827635450ebb3c5a7d12c1f8e7b2b514439ac10a67eef3d9fd9c5c68e245","created_at":1673347337,"kind":1,"tags":[],"content":"Parallel mining demo","sig":""}
EOF

echo -n "💻 System information:"
if command -v sysctl &> /dev/null; then
    CPU_CORES=$(sysctl -n hw.ncpu)
    echo "   CPU cores: $CPU_CORES"
elif [ -f /proc/cpuinfo ]; then
    CPU_CORES=$(nproc)
    echo "   CPU cores: $CPU_CORES"
else
    echo "   CPU cores: Unknown"
fi
echo ""

DIFFICULTY=13

echo "🔍 Demo 1: Quick mining comparison (difficulty $DIFFICULTY)"
echo "=================================================="
echo ""

echo "Single-threaded version:"
echo "------------------------"
./nip13_miner demo_event.json $DIFFICULTY 100 2>/dev/null | grep -E "(Found valid proof|Rate:|Time:)"

echo ""
echo "Parallel version:"
echo "-----------------"
./nip13_parallel demo_event.json $DIFFICULTY 5 2>/dev/null | grep -E "(Found valid proof|Rate:|Time:|threads)"

echo ""
echo "🏁 Demo 2: Benchmark mode comparison (difficulty $DIFFICULTY)"
echo "================================================================="
echo ""

echo "Single-threaded benchmark:"
echo "--------------------------"
./nip13_miner demo_event.json $DIFFICULTY benchmark 100 | grep -E "(Solutions per second|Hash rate)"

echo ""
echo "Parallel benchmark (all cores):"
echo "-------------------------------"
./nip13_parallel demo_event.json $DIFFICULTY benchmark 1000 | grep -E "(Solutions per second|Hash rate)"

echo ""
echo "🧪 Demo 3: Thread scaling comparison (difficulty $DIFFICULTY)"
echo "================================================================="
echo ""

echo "1 thread:"
echo "---------"
./nip13_parallel demo_event.json $DIFFICULTY benchmark 100 1 | grep -E "(Solutions per second|Hash rate)"

echo ""
echo "4 threads:"
echo "----------"
./nip13_parallel demo_event.json $DIFFICULTY benchmark 500 4 | grep -E "(Solutions per second|Hash rate)"

echo ""
echo "8 threads:"
echo "----------"
./nip13_parallel demo_event.json $DIFFICULTY benchmark 1000 8 | grep -E "(Solutions per second|Hash rate)"

echo ""
echo "All cores ($CPU_CORES threads):"
echo "$(printf '%*s' ${#CPU_CORES} | tr ' ' '-')$(printf '%*s' 9 | tr ' ' '-')"
./nip13_parallel demo_event.json 12 benchmark 3 | grep -E "(Solutions per second|Hash rate)"

echo ""
echo "✅ Demo complete!"
echo ""
echo "💡 Usage tips:"
echo "  ./nip13_parallel event.json 16 benchmark 10      # Find 10 solutions (all cores)"
echo "  ./nip13_parallel event.json 16 benchmark 10 4    # Find 10 solutions (4 threads)"
echo "  ./nip13_parallel event.json 18 100              # Mine with max 100M attempts"
echo "  ./nip13_parallel event.json 20 50 8             # Use only 8 threads"
echo "  ./nip13_parallel event.json 14 benchmark 5 1     # Single-threaded benchmark"
echo ""
