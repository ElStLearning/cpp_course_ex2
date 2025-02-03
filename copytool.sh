#!/bin/sh

# Set log file path
LOG_FILE="log.txt"

# Check if three arguments are provided
if [ "$#" -ne 3 ]; then
    echo "Usage: $0 <input_file> <output_file> <shared_memory_name>" >> "$LOG_FILE" 2>&1
    exit 1
fi

# Set input, output, and shared memory name from command line arguments
INPUT_FILE="$1"
OUTPUT_FILE="$2"
SHARED_MEMORY_NAME="$3"

# Set the program executable path
PROGRAM="./x64/Release/ex_2.exe"

# Check if the executable exists
if [ ! -f "$PROGRAM" ]; then
    echo "Executable $PROGRAM not found." >> "$LOG_FILE" 2>&1
    exit 1
fi

# Running the program
echo "Running $PROGRAM ..."
"$PROGRAM" "$INPUT_FILE" "$OUTPUT_FILE" "$SHARED_MEMORY_NAME"

# Check for success of the program
if [ $? -eq 0 ]; then
    echo "Program executed successfully." >> "$LOG_FILE" 2>&1
    echo "File $INPUT_FILE has been copied to $OUTPUT_FILE" >> "$LOG_FILE" 2>&1
else
    echo "Program failed to execute properly." >> "$LOG_FILE" 2>&1
fi
