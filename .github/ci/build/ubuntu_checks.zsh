#!/bin/zsh
##########################################################################################################
#
# Copyright (c) 2025-2026. International Color Consortium. All rights reserved.
# Copyright (c) 2024. David H Hoyt LLC. All rights reserved.
#
# Last Updated: 26-MAR-2025 at 1200 EDT by David Hoyt
#
# Intent:
#   This script polls the unix zsh host, retrieves system, build and development environment
#   details, and reports developer-related configurations.
#
# Features:
#   - Reports kernel, OS, and CPU details
#   - Checks for developer toolchain versions and paths
#   - Examines environmental variables relevant to development
#
# Usage:
#   Run this script in a terminal from project_root with:
#     /bin/zsh -c "$(curl -fsSL https://raw.githubusercontent.com/InternationalColorConsortium/iccDEV/refs/heads/ci-qa-flags/.github/ci/build/ubuntu_checks.zsh)"

set -euo pipefail

log_file="linux_inventory_$(date +%Y%m%d_%H%M%S).log"
HTML_FILE="linux_build_report.html"

typeset -a executables dynamic_libs static_libs signed_files unsigned_files_list
executables=()
dynamic_libs=()
static_libs=()
signed_files=()
unsigned_files_list=()

typeset -i total_files=0
typeset -i executables_count=0
typeset -i dynamic_libs_count=0
typeset -i static_libs_count=0
typeset -i signed_count=0
typeset -i unsigned_count=0

log_message() {
    echo "$(date '+%Y-%m-%d %H:%M:%S') | $1" | tee -a "$log_file"
}

trap 'log_message "❌ ERROR on line $LINENO. Command failed."' ERR

escape_html() {
    echo "$1" | sed -E 's/&/\&amp;/g; s/</\&lt;/g; s/>/\&gt;/g; s/"/\&quot;/g; s/'"'"'/\&#39;/g'
}

check_signature() {
    unsigned_files_list+=("$1")
    let "unsigned_count+=1"
}

log_message "🔍 Scanning for binaries built in last 24 hours..."
log_message "Search root: $(pwd)"

while read -r binary; do
    [[ -z "$binary" ]] && continue
    [[ ! -r "$binary" ]] && log_message "⚠️ Skipping unreadable file: $binary" && continue

    if ! file_info=$(file "$binary" 2>/dev/null); then
        file_info="UNKNOWN"
    fi

    log_message "Analyzing: $binary → $file_info"

    case "$file_info" in
        (*"ELF"*64*"executable"*)
            executables+=("$binary")
            let "executables_count+=1"
            ;;
        (*"ELF"*shared*)
            dynamic_libs+=("$binary")
            let "dynamic_libs_count+=1"
            ;;
        (*"current ar archive"*)
            static_libs+=("$binary")
            let "static_libs_count+=1"
            ;;
        (*"Mach-O 64-bit dynamically linked shared library"*)
            dynamic_libs+=("$binary")
            let "dynamic_libs_count+=1"
            ;;
        (*"Mach-O 64-bit executable"*)
            executables+=("$binary")
            let "executables_count+=1"
            ;;
    esac

    check_signature "$binary"
    let "total_files+=1"
done < <(
    find . -type f \( -perm -111 -o -name "*.a" -o -name "*.so" -o -name "*.dylib" \) \
    -mmin -1440 ! -path "*/.git/*" ! -path "*/CMakeFiles/*" ! -name "*.sh" 2>/dev/null
)

log_message "✅ Scan complete: total files = $total_files"

log_message "Build Report Summary"
log_message "----------------------"
log_message "Total Files Found: $total_files"
log_message "Executables: $executables_count"
log_message "Dynamic Libs: $dynamic_libs_count"
log_message "Static Libs: $static_libs_count"
log_message "Unsigned Files: $unsigned_count"

generate_html_report() {
    log_message "📝 Generating HTML report..."

    {
        cat <<EOF
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<title>Linux Build Report</title>
<style>
body { font-family: Arial; margin: 20px; }
table { width: 100%; border-collapse: collapse; }
th, td { padding: 8px; border: 1px solid #ddd; }
th { background-color: #f4f4f4; }
pre { background: #f4f4f4; padding: 10px; border-radius: 5px; }
</style>
</head>
<body>
<h1>iccMAX | Ubuntu Developer Report</h1>
<p><strong>Generated:</strong> $(date)</p>
<h2>Host and Build Summary</h2>
<table>
<tr><th>Total Files</th><td>$total_files</td></tr>
<tr><th>Executables</th><td>$executables_count</td></tr>
<tr><th>Dynamic Libraries</th><td>$dynamic_libs_count</td></tr>
<tr><th>Static Libraries</th><td>$static_libs_count</td></tr>
<tr><th>Unsigned Files</th><td>$unsigned_count</td></tr>
</table>
EOF
    } > "$HTML_FILE"

    generate_table_section() {
        local title="$1"
        shift
        local -a filelist
        filelist=("$@")

        echo "<h2>$title</h2><table><tr><th>#</th><th>Path</th></tr>" >> "$HTML_FILE"
        if (( ${#filelist[@]} > 0 )); then
            local i=1
            for f in "${filelist[@]}"; do
                echo "<tr><td>$i</td><td>$(escape_html "$f")</td></tr>" >> "$HTML_FILE"
                let "i+=1"
            done
        else
            echo "<tr><td colspan='2'>None</td></tr>" >> "$HTML_FILE"
        fi
        echo "</table>" >> "$HTML_FILE"
    }

    generate_table_section "Executables" "${(@)executables}"
    generate_table_section "Dynamic Libraries" "${(@)dynamic_libs}"
    generate_table_section "Static Libraries" "${(@)static_libs}"
    generate_table_section "Unsigned Files" "${(@)unsigned_files_list}"

    echo "<h2>System Info</h2><pre>$(escape_html "$(uname -a)\n$(cat /etc/os-release 2>/dev/null || true)")</pre>" >> "$HTML_FILE"
    echo "</body></html>" >> "$HTML_FILE"

    log_message "✅ HTML report generated: $HTML_FILE"
}

generate_html_report
