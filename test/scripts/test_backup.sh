#!/bin/bash

# Exit on error
set -e

# Get the directory where the script is located
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
TEST_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$(dirname "$TEST_DIR")/build"

# Configuration
CONFIG_FILE="$TEST_DIR/config/test_config.json"
LOG_DIR="$TEST_DIR/logs"
BACKUP_DIR="$TEST_DIR/backups"
TEST_LOG="$LOG_DIR/test_run_$(date +%Y%m%d_%H%M%S).log"
TEST_REPORT="$LOG_DIR/test_report_$(date +%Y%m%d_%H%M%S).html"

# Test statistics
declare -A TEST_RESULTS
declare -A TEST_DURATIONS
declare -A TEST_ERRORS
TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0

# Function to log messages
log() {
    local level=$1
    local message=$2
    local timestamp=$(date '+%Y-%m-%d %H:%M:%S')
    echo "[$timestamp] [$level] $message" | tee -a "$TEST_LOG"
}

# Function to generate HTML report
generate_html_report() {
    local report_file="$TEST_REPORT"
    local start_time=$(date '+%Y-%m-%d %H:%M:%S')
    local end_time=$(date '+%Y-%m-%d %H:%M:%S')
    
    cat > "$report_file" << EOF
<!DOCTYPE html>
<html>
<head>
    <title>VMware Backup Test Report</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; }
        .header { background-color: #f0f0f0; padding: 10px; margin-bottom: 20px; }
        .summary { margin-bottom: 20px; }
        .test-case { margin-bottom: 10px; padding: 10px; border: 1px solid #ddd; }
        .passed { background-color: #e6ffe6; }
        .failed { background-color: #ffe6e6; }
        .details { margin-top: 10px; padding: 10px; background-color: #f9f9f9; }
    </style>
</head>
<body>
    <div class="header">
        <h1>VMware Backup Test Report</h1>
        <p>Start Time: $start_time</p>
        <p>End Time: $end_time</p>
    </div>
    
    <div class="summary">
        <h2>Test Summary</h2>
        <p>Total Tests: $TOTAL_TESTS</p>
        <p>Passed: $PASSED_TESTS</p>
        <p>Failed: $FAILED_TESTS</p>
        <p>Success Rate: $(( (PASSED_TESTS * 100) / TOTAL_TESTS ))%</p>
    </div>
    
    <div class="test-cases">
        <h2>Test Cases</h2>
EOF
    
    # Add test cases
    for test_name in "${!TEST_RESULTS[@]}"; do
        local result=${TEST_RESULTS[$test_name]}
        local duration=${TEST_DURATIONS[$test_name]}
        local error=${TEST_ERRORS[$test_name]}
        local status_class=$([ "$result" = "PASSED" ] && echo "passed" || echo "failed")
        
        cat >> "$report_file" << EOF
        <div class="test-case $status_class">
            <h3>$test_name</h3>
            <p>Status: $result</p>
            <p>Duration: $duration seconds</p>
EOF
        
        if [ "$result" = "FAILED" ] && [ -n "$error" ]; then
            cat >> "$report_file" << EOF
            <div class="details">
                <p>Error: $error</p>
            </div>
EOF
        fi
        
        cat >> "$report_file" << EOF
        </div>
EOF
    done
    
    cat >> "$report_file" << EOF
    </div>
</body>
</html>
EOF
    
    log "INFO" "Test report generated: $report_file"
}

# Function to cleanup test artifacts
cleanup() {
    local vm_name=$1
    log "INFO" "Starting cleanup for VM: $vm_name"
    
    # Remove backup files for this VM
    if [ -d "$BACKUP_DIR/$vm_name" ]; then
        log "INFO" "Removing backup files for VM: $vm_name"
        rm -rf "$BACKUP_DIR/$vm_name"
    fi
    
    # Archive logs
    local log_archive="$LOG_DIR/archive_$(date +%Y%m%d_%H%M%S)"
    mkdir -p "$log_archive"
    
    # Move relevant log files to archive
    find "$LOG_DIR" -maxdepth 1 -name "backup_${vm_name}_*.log" -o -name "restore_${vm_name}_*.log" | while read -r log_file; do
        if [ -f "$log_file" ]; then
            mv "$log_file" "$log_archive/"
            log "INFO" "Archived log file: $(basename "$log_file")"
        fi
    done
    
    log "INFO" "Cleanup completed for VM: $vm_name"
}

# Function to run a backup test
run_backup_test() {
    local vm_name=$1
    local test_name=$2
    local test_log="$LOG_DIR/backup_${vm_name}_$(date +%Y%m%d_%H%M%S).log"
    local start_time=$(date +%s)
    
    log "INFO" "Starting backup test: $test_name for VM: $vm_name"
    log "DEBUG" "Using log file: $test_log"
    
    # Run backup with debug logging
    "$BUILD_DIR/vmware-backup" \
        --config "$CONFIG_FILE" \
        --vm "$vm_name" \
        --backup-dir "$BACKUP_DIR" \
        --log-level debug \
        --log-file "$test_log" 2>&1 | tee -a "$TEST_LOG"
    
    local result=$?
    local end_time=$(date +%s)
    local duration=$((end_time - start_time))
    
    # Update test statistics
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    TEST_DURATIONS[$test_name]=$duration
    
    # Check backup result
    if [ $result -eq 0 ]; then
        log "INFO" "Backup test passed: $test_name"
        log "DEBUG" "Backup files created in: $BACKUP_DIR/$vm_name"
        TEST_RESULTS[$test_name]="PASSED"
        PASSED_TESTS=$((PASSED_TESTS + 1))
    else
        log "ERROR" "Backup test failed: $test_name (exit code: $result)"
        log "ERROR" "Check log file for details: $test_log"
        TEST_RESULTS[$test_name]="FAILED"
        TEST_ERRORS[$test_name]="Exit code: $result"
        FAILED_TESTS=$((FAILED_TESTS + 1))
        exit 1
    fi
}

# Function to run a restore test
run_restore_test() {
    local vm_name=$1
    local test_name=$2
    local test_log="$LOG_DIR/restore_${vm_name}_$(date +%Y%m%d_%H%M%S).log"
    local start_time=$(date +%s)
    
    log "INFO" "Starting restore test: $test_name for VM: $vm_name"
    log "DEBUG" "Using log file: $test_log"
    
    # Run restore with debug logging
    "$BUILD_DIR/vmware-restore" \
        --config "$CONFIG_FILE" \
        --vm "$vm_name" \
        --backup-dir "$BACKUP_DIR" \
        --log-level debug \
        --log-file "$test_log" 2>&1 | tee -a "$TEST_LOG"
    
    local result=$?
    local end_time=$(date +%s)
    local duration=$((end_time - start_time))
    
    # Update test statistics
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    TEST_DURATIONS[$test_name]=$duration
    
    # Check restore result
    if [ $result -eq 0 ]; then
        log "INFO" "Restore test passed: $test_name"
        log "DEBUG" "Restore completed for VM: $vm_name"
        TEST_RESULTS[$test_name]="PASSED"
        PASSED_TESTS=$((PASSED_TESTS + 1))
    else
        log "ERROR" "Restore test failed: $test_name (exit code: $result)"
        log "ERROR" "Check log file for details: $test_log"
        TEST_RESULTS[$test_name]="FAILED"
        TEST_ERRORS[$test_name]="Exit code: $result"
        FAILED_TESTS=$((FAILED_TESTS + 1))
        exit 1
    fi
}

# Function to verify test environment
verify_environment() {
    log "INFO" "Verifying test environment..."
    
    # Check if executables exist
    if [ ! -f "$BUILD_DIR/vmware-backup" ] || [ ! -f "$BUILD_DIR/vmware-restore" ]; then
        log "ERROR" "Required executables not found in $BUILD_DIR. Please build the project first."
        exit 1
    fi
    
    # Check if config file exists
    if [ ! -f "$CONFIG_FILE" ]; then
        log "ERROR" "Config file not found: $CONFIG_FILE"
        exit 1
    fi
    
    # Create necessary directories
    mkdir -p "$LOG_DIR"
    mkdir -p "$BACKUP_DIR"
    
    log "INFO" "Test environment verified successfully"
}

# Main test sequence
verify_environment

log "INFO" "Starting backup/restore tests..."

# Test 1: Basic backup
run_backup_test "test-vm-1" "Basic backup test"
cleanup "test-vm-1"

# Test 2: Backup with verification
run_backup_test "test-vm-2" "Backup with verification test"
cleanup "test-vm-2"

# Test 3: Restore test
run_restore_test "test-vm-1" "Basic restore test"
cleanup "test-vm-1"

log "INFO" "All tests completed successfully!"

# Generate HTML report
generate_html_report

# Print summary
echo "Test Summary:"
echo "-------------"
echo "Total Tests: $TOTAL_TESTS"
echo "Passed: $PASSED_TESTS"
echo "Failed: $FAILED_TESTS"
echo "Success Rate: $(( (PASSED_TESTS * 100) / TOTAL_TESTS ))%"
echo "Test logs: $TEST_LOG"
echo "Test report: $TEST_REPORT"
echo "Backup directory: $BACKUP_DIR"
echo "Log directory: $LOG_DIR"
echo "Archived logs: $LOG_DIR/archive_*" 