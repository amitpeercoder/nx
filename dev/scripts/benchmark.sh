#!/bin/bash

# Performance baseline script for nx
# This script runs benchmarks and compares against target performance

set -e

echo "=== nx Performance Benchmarks ==="
echo "Date: $(date)"
echo "Host: $(hostname)"

# System information
echo ""
echo "=== System Information ==="
echo "OS: $(uname -a)"
echo "CPU: $(lscpu | grep 'Model name' | head -1 | cut -d':' -f2 | xargs)"
echo "CPU Cores: $(nproc)"
echo "Memory: $(free -h | grep '^Mem:' | awk '{print $2}')"
echo "Disk: $(df -h . | tail -1 | awk '{print $4}' | head -1) available"

# Build information
BUILD_DIR="${BUILD_DIR:-build}"
if [ ! -d "$BUILD_DIR" ]; then
    echo "Error: Build directory '$BUILD_DIR' not found"
    echo "Please run: cmake --preset release && cmake --build --preset release"
    exit 1
fi

BENCHMARK_EXEC="$BUILD_DIR/tests/nx_benchmark"
if [ ! -f "$BENCHMARK_EXEC" ]; then
    echo "Error: Benchmark executable not found at $BENCHMARK_EXEC"
    echo "Please run: cmake --build --preset release --target nx_benchmark"
    exit 1
fi

echo ""
echo "=== Performance Targets (from specification) ==="
echo "- ULID generation: Fast (target: <1ms)"
echo "- Note creation: <50ms (10k notes)"
echo "- Note serialization: <10ms per note"
echo "- Corpus generation: <30s for 10k notes"
echo "- Memory usage: <100MB for typical operations"

echo ""
echo "=== Running Benchmarks ==="

# Run benchmarks with specific filters for key operations
echo ""
echo "--- Core Operation Benchmarks ---"
$BENCHMARK_EXEC --benchmark_filter="BM_UlidGeneration|BM_UlidParsing|BM_NoteCreation" \
    --benchmark_min_time=1.0 \
    --benchmark_format=console

echo ""
echo "--- Corpus Generation Benchmarks ---"
$BENCHMARK_EXEC --benchmark_filter="BM_CorpusGeneration" \
    --benchmark_min_time=0.5 \
    --benchmark_format=console

echo ""
echo "--- Serialization Benchmarks ---"
$BENCHMARK_EXEC --benchmark_filter="BM_NoteSerialization|BM_NoteDeserialization" \
    --benchmark_min_time=1.0 \
    --benchmark_format=console

echo ""
echo "--- Memory Usage Benchmarks ---"
$BENCHMARK_EXEC --benchmark_filter="BM_CorpusMemoryUsage" \
    --benchmark_min_time=0.5 \
    --benchmark_format=console

echo ""
echo "--- Full Benchmark Suite ---"
echo "Running complete benchmark suite..."
$BENCHMARK_EXEC --benchmark_format=json > benchmark_results.json
echo "Results saved to benchmark_results.json"

echo ""
echo "=== Performance Analysis ==="

# Simple performance analysis using Python if available
if command -v python3 &> /dev/null; then
    python3 -c "
import json
import sys

try:
    with open('benchmark_results.json', 'r') as f:
        data = json.load(f)
    
    print('Key Performance Metrics:')
    print('-' * 40)
    
    for benchmark in data['benchmarks']:
        name = benchmark['name']
        time_unit = benchmark['time_unit']
        real_time = benchmark['real_time']
        
        if 'UlidGeneration' in name:
            print(f'ULID Generation: {real_time:.2f} {time_unit}')
        elif 'NoteCreation' in name and 'Realistic' not in name:
            print(f'Simple Note Creation: {real_time:.2f} {time_unit}')
        elif 'RealisticNoteCreation' in name:
            print(f'Realistic Note Creation: {real_time:.2f} {time_unit}')
        elif 'NoteSerialization' in name and 'Deserialization' not in name:
            print(f'Note Serialization: {real_time:.2f} {time_unit}')
        elif 'NoteDeserialization' in name:
            print(f'Note Deserialization: {real_time:.2f} {time_unit}')
    
    print('')
    print('Corpus Generation Performance:')
    print('-' * 40)
    
    for benchmark in data['benchmarks']:
        name = benchmark['name']
        if 'CorpusGeneration' in name and '/' in name:
            size = name.split('/')[-1]
            time_unit = benchmark['time_unit']
            real_time = benchmark['real_time']
            items_per_second = benchmark.get('items_per_second', 0)
            
            print(f'{size} notes: {real_time:.2f} {time_unit} ({items_per_second:.0f} notes/sec)')
    
    print('')
    print('Memory Usage:')
    print('-' * 40)
    
    for benchmark in data['benchmarks']:
        name = benchmark['name']
        if 'MemoryUsage' in name and '/' in name:
            size = name.split('/')[-1]
            counters = benchmark.get('counters', {})
            memory_per_note = counters.get('MemoryPerNote', 0)
            total_memory_mb = counters.get('TotalMemoryMB', 0)
            
            if memory_per_note > 0:
                print(f'{size} notes: {memory_per_note:.0f} bytes/note, {total_memory_mb:.1f} MB total')

except Exception as e:
    print(f'Could not analyze benchmark results: {e}')
    print('Raw JSON results available in benchmark_results.json')
"
else
    echo "Python3 not available for analysis. Raw results in benchmark_results.json"
fi

echo ""
echo "=== Performance Regression Detection ==="

# Save baseline if it doesn't exist
BASELINE_FILE="benchmark_baseline.json"
if [ ! -f "$BASELINE_FILE" ]; then
    cp benchmark_results.json "$BASELINE_FILE"
    echo "Baseline saved to $BASELINE_FILE"
else
    echo "Comparing against baseline..."
    
    # Simple baseline comparison using jq
    if command -v jq > /dev/null 2>&1; then
        echo "Performance comparison (current vs baseline):"
        echo "============================================="
        
        # Compare key metrics
        for metric in "real_time" "cpu_time"; do
            echo ""
            echo "Metric: $metric"
            echo "----------------"
            
            # Get benchmark names from current results
            jq -r '.benchmarks[] | .name' benchmark_results.json | while read -r benchmark_name; do
                # Get current and baseline values
                current=$(jq -r ".benchmarks[] | select(.name == \"$benchmark_name\") | .$metric" benchmark_results.json 2>/dev/null)
                baseline=$(jq -r ".benchmarks[] | select(.name == \"$benchmark_name\") | .$metric" "$BASELINE_FILE" 2>/dev/null)
                
                if [ "$current" != "null" ] && [ "$baseline" != "null" ] && [ "$current" != "" ] && [ "$baseline" != "" ]; then
                    # Calculate percentage difference
                    diff=$(echo "scale=2; (($current - $baseline) / $baseline) * 100" | bc 2>/dev/null || echo "N/A")
                    
                    # Format output with color coding
                    if [ "$diff" != "N/A" ]; then
                        if (( $(echo "$diff < -5" | bc -l 2>/dev/null || echo 0) )); then
                            # Significant improvement (> 5% faster)
                            echo "  $benchmark_name: ${diff}% (IMPROVED)"
                        elif (( $(echo "$diff > 10" | bc -l 2>/dev/null || echo 0) )); then
                            # Significant regression (> 10% slower)
                            echo "  $benchmark_name: +${diff}% (REGRESSION)"
                        else
                            # Small change
                            echo "  $benchmark_name: ${diff}%"
                        fi
                    else
                        echo "  $benchmark_name: Unable to calculate difference"
                    fi
                else
                    echo "  $benchmark_name: Missing data in baseline or current results"
                fi
            done
        done
        
        # Summary statistics
        echo ""
        echo "Summary:"
        echo "--------"
        
        # Count regressions and improvements
        regression_count=0
        improvement_count=0
        
        # This is a simplified count - in a real implementation we'd do proper analysis
        jq -r '.benchmarks[] | .name' benchmark_results.json | while read -r benchmark_name; do
            current=$(jq -r ".benchmarks[] | select(.name == \"$benchmark_name\") | .real_time" benchmark_results.json 2>/dev/null)
            baseline=$(jq -r ".benchmarks[] | select(.name == \"$benchmark_name\") | .real_time" "$BASELINE_FILE" 2>/dev/null)
            
            if [ "$current" != "null" ] && [ "$baseline" != "null" ] && [ "$current" != "" ] && [ "$baseline" != "" ]; then
                diff=$(echo "scale=2; (($current - $baseline) / $baseline) * 100" | bc 2>/dev/null || echo "0")
                if [ "$diff" != "N/A" ]; then
                    if (( $(echo "$diff > 10" | bc -l 2>/dev/null || echo 0) )); then
                        regression_count=$((regression_count + 1))
                    elif (( $(echo "$diff < -5" | bc -l 2>/dev/null || echo 0) )); then
                        improvement_count=$((improvement_count + 1))
                    fi
                fi
            fi
        done
        
        total_benchmarks=$(jq '.benchmarks | length' benchmark_results.json 2>/dev/null || echo "0")
        echo "Total benchmarks: $total_benchmarks"
        echo "Performance improvements: $improvement_count"
        echo "Performance regressions: $regression_count"
        
        if [ $regression_count -gt 0 ]; then
            echo ""
            echo "⚠️  WARNING: $regression_count performance regressions detected!"
            echo "   Consider investigating these issues before release."
        fi
        
        if [ $improvement_count -gt 0 ]; then
            echo ""
            echo "✅ $improvement_count performance improvements detected!"
        fi
        
    else
        echo "jq not found. Installing jq for JSON processing..."
        echo "Please install jq to enable baseline comparison:"
        echo "  Ubuntu/Debian: sudo apt-get install jq"
        echo "  macOS: brew install jq"
        echo "  CentOS/RHEL: sudo yum install jq"
        echo ""
        echo "For now, doing basic file comparison:"
        
        # Fallback to basic file size and modification comparison
        current_size=$(wc -c < benchmark_results.json)
        baseline_size=$(wc -c < "$BASELINE_FILE")
        
        echo "Current results size: $current_size bytes"
        echo "Baseline results size: $baseline_size bytes"
        
        if [ $current_size -gt $((baseline_size * 11 / 10)) ]; then
            echo "⚠️  Results file is significantly larger than baseline"
        elif [ $current_size -lt $((baseline_size * 9 / 10)) ]; then
            echo "ℹ️  Results file is significantly smaller than baseline"
        else
            echo "✅ Results file size is similar to baseline"
        fi
    fi
fi

echo ""
echo "=== Recommendations ==="
echo "1. Run this script regularly to detect performance regressions"
echo "2. Update baseline after significant optimizations"
echo "3. Monitor memory usage trends with larger corpora"
echo "4. Profile with 'perf' or 'valgrind' for detailed analysis"

echo ""
echo "Benchmark complete!"