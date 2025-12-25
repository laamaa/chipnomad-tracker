#!/bin/bash

# Script to convert local #include <file.h> to #include "file.h"
# while preserving system library includes

# Common system headers to exclude
SYSTEM_HEADERS=(
    "stdio.h" "stdlib.h" "string.h" "math.h" "time.h" "ctype.h"
    "stddef.h" "stdint.h" "stdbool.h" "limits.h" "float.h" "assert.h"
    "errno.h" "signal.h" "setjmp.h" "stdarg.h" "locale.h" "iso646.h"
    "complex.h" "fenv.h" "inttypes.h" "tgmath.h" "wchar.h" "wctype.h"
    "unistd.h" "fcntl.h" "sys/types.h" "sys/stat.h" "dirent.h"
    "pthread.h" "semaphore.h" "regex.h" "fnmatch.h" "glob.h"
    "arpa/inet.h" "netinet/in.h" "sys/socket.h" "netdb.h"
    "SDL2/SDL.h" "SDL/SDL.h" "strings.h" "ft2build.h"
)

# Function to check if a header is a system header
is_system_header() {
    local header="$1"
    for sys_header in "${SYSTEM_HEADERS[@]}"; do
        if [ "$header" = "$sys_header" ]; then
            return 0
        fi
    done
    return 1
}

# Find all C/C++ source and header files
find . -type f \( -name "*.c" -o -name "*.h" -o -name "*.cpp" -o -name "*.hpp" -o -name "*.cc" \) | while read -r file; do
    echo "Processing: $file"
    
    # Create a temporary file
    temp_file=$(mktemp)

    # Process the file line by line
    while true; do
        IFS= read -r line
        read_status=$?

        # Stop if EOF reached and no data read
        if [ $read_status -ne 0 ] && [ -z "$line" ]; then
            break
        fi

        # Check if line contains #include <...>
        if [[ $line =~ ^[[:space:]]*#include[[:space:]]*\<([^>]+)\> ]]; then
            header="${BASH_REMATCH[1]}"

            # Check if it's NOT a system header
            if ! is_system_header "$header"; then
                # Convert to quotes
                line=$(echo "$line" | sed 's/#include[[:space:]]*<\([^>]*\)>/#include "\1"/')
                echo "  Changed: <$header> -> \"$header\""
            fi
        fi

        if [ $read_status -eq 0 ]; then
            printf "%s\n" "$line" >> "$temp_file"
        else
            printf "%s" "$line" >> "$temp_file"
            break
        fi
    done < "$file"

    # Replace original file with modified version
    mv "$temp_file" "$file"
done

echo "Done!"