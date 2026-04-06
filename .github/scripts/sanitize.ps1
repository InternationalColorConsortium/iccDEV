<#
@file
File:       sanitize.ps1

Contains:   Implementation of sanitizer for powershell.

Version:    V1

Copyright:  (c) see Software License
#>

<#
Copyright (c) International Color Consortium.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in
   the documentation and/or other materials provided with the
   distribution.

3. In the absence of prior written permission, the names "ICC" and "The
   International Color Consortium" must not be used to imply that the
   ICC organization endorses or promotes products derived from this
   software.

THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESSED OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE INTERNATIONAL COLOR CONSORTIUM OR
ITS CONTRIBUTING MEMBERS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
SUCH DAMAGE.
====================================================================

This software consists of voluntary contributions made by many
individuals on behalf of the International Color Consortium.

Membership in the ICC is encouraged when this software is used for
commercial purposes.

For more information on The International Color Consortium, please
see http://www.color.org/.
#>

###############################################################
# Copyright (©) 2024-2025 David H Hoyt. All rights reserved.
###############################################################
#                 https://srd.cx
#
# Last Updated: 17-DEC-2025 1700Z by David Hoyt
#
# Intent: Try Sanitizing User Controllable Inputs
#
# File: .github/scripts/sanitize.ps1
#
#
# Comment: Sanitizing User Controllable Input
#          - is a Moving Target
#          - needs ongoing updates
#          - needs additional unit tests
#
#
#
###############################################################

# Configuration - Maximum lengths
$script:SANITIZE_LINE_MAXLEN = if ($env:SANITIZE_LINE_MAXLEN) { [int]$env:SANITIZE_LINE_MAXLEN } else { 1000 }
$script:SANITIZE_PRINT_MAXLEN = if ($env:SANITIZE_PRINT_MAXLEN) { [int]$env:SANITIZE_PRINT_MAXLEN } else { 8000 }

<#
.SYNOPSIS
    Escapes HTML entities in a string.

.DESCRIPTION
    Replaces &, <, >, " and ' with HTML entities.

.PARAMETER InputString
    The string to escape.

.EXAMPLE
    Escape-Html "Hello <world>"
    Returns "Hello &lt;world&gt;"
#>
function Escape-Html {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$false, ValueFromPipeline=$true)]
        [AllowEmptyString()]
        [string]$InputString = ""
    )

    $result = $InputString
    $result = $result.Replace("&", "&amp;")
    $result = $result.Replace("<", "&lt;")
    $result = $result.Replace(">", "&gt;")
    $result = $result.Replace('"', "&quot;")
    $result = $result.Replace("'", "&#39;")
    return $result
}

<#
.SYNOPSIS
    Strips control characters but keeps newlines.

.DESCRIPTION
    Removes control characters except newline (LF). Removes CR and NUL.

.PARAMETER InputString
    The string to process.
#>
function Strip-CtrlKeepNewlines {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$false)]
        [AllowEmptyString()]
        [string]$InputString = ""
    )

    $result = $InputString
    $result = $result -replace "`r", ""

    $cleanChars = @()
    foreach ($char in $result.ToCharArray()) {
        $codePoint = [int]$char
        if ($codePoint -eq 0x0A -or ($codePoint -ge 0x20 -and $codePoint -le 0x7E) -or $codePoint -ge 0xA0) {
            $cleanChars += $char
        }
    }
    return -join $cleanChars
}

<#
.SYNOPSIS
    Strips control characters and newlines.

.DESCRIPTION
    Removes control characters and newlines (useful for single-line outputs).

.PARAMETER InputString
    The string to process.
#>
function Strip-CtrlRemoveNewlines {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$false)]
        [AllowEmptyString()]
        [string]$InputString = ""
    )

    $result = $InputString
    $result = $result -replace "`r", ""
    $result = $result -replace "`n", " "

    $cleanChars = @()
    foreach ($char in $result.ToCharArray()) {
        $codePoint = [int]$char
        if ($codePoint -eq 0x09 -or ($codePoint -ge 0x20 -and $codePoint -le 0x7E) -or $codePoint -ge 0xA0) {
            $cleanChars += $char
        }
    }
    return -join $cleanChars
}

<#
.SYNOPSIS
    Trims leading and trailing whitespace.

.PARAMETER InputString
    The string to trim.
#>
function Trim-Whitespace {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$false)]
        [AllowEmptyString()]
        [string]$InputString = ""
    )

    return $InputString.Trim()
}

<#
.SYNOPSIS
    Truncates a string to maximum length with ellipsis.

.PARAMETER InputString
    The string to truncate.

.PARAMETER MaxLen
    Maximum length allowed.
#>
function Truncate-String {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$false)]
        [AllowEmptyString()]
        [string]$InputString = "",

        [Parameter(Mandatory=$true)]
        [int]$MaxLen
    )

    if ($InputString.Length -le $MaxLen) {
        return $InputString
    }
    $headLen = $MaxLen - 3
    return $InputString.Substring(0, $headLen) + "..."
}

<#
.SYNOPSIS
    Sanitizes a line of text for safe output in GitHub Actions.

.DESCRIPTION
    Produces a single-line safe string by:
    - Removing CR/LF and control chars
    - Trimming whitespace
    - Escaping HTML entities
    - Truncating to SANITIZE_LINE_MAXLEN

.PARAMETER InputString
    The string to sanitize.

.EXAMPLE
    Sanitize-Line "Hello`0World"
    Returns sanitized version of the input string.
#>
function Sanitize-Line {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$false, ValueFromPipeline=$true)]
        [AllowEmptyString()]
        [string]$InputString = ""
    )

    $result = Strip-CtrlRemoveNewlines -InputString $InputString
    $result = Trim-Whitespace -InputString $result
    $result = Escape-Html -InputString $result
    $result = Truncate-String -InputString $result -MaxLen $script:SANITIZE_LINE_MAXLEN
    return $result
}

<#
.SYNOPSIS
    Sanitizes multi-line text for safe output in GitHub step summaries.

.DESCRIPTION
    Produces a multi-line safe string suitable for step summaries by:
    - Removing CR and dangerous control chars but preserving LF
    - Escaping HTML entities
    - Collapsing excessive consecutive newlines to max 3
    - Truncating total length to SANITIZE_PRINT_MAXLEN

.PARAMETER InputString
    The string to sanitize.

.EXAMPLE
    Sanitize-Print "Hello`nWorld"
    Returns sanitized version preserving newlines.
#>
function Sanitize-Print {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$false, ValueFromPipeline=$true)]
        [AllowEmptyString()]
        [string]$InputString = ""
    )

    $result = Strip-CtrlKeepNewlines -InputString $InputString
    $result = $result -replace "(`n){4,}", "`n`n`n"
    $result = Escape-Html -InputString $result
    $result = Truncate-String -InputString $result -MaxLen $script:SANITIZE_PRINT_MAXLEN
    return $result
}

<#
.SYNOPSIS
    Sanitizes branch, tag or ref names for filenames/concurrency groups.

.DESCRIPTION
    - Replaces disallowed chars with '-'
    - Collapses multiple '-' into single '-'
    - Trims leading/trailing '-'

.PARAMETER InputString
    The ref string to sanitize.

.EXAMPLE
    Sanitize-Ref "feature/my-branch"
    Returns "feature-my-branch"
#>
function Sanitize-Ref {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$false, ValueFromPipeline=$true)]
        [AllowEmptyString()]
        [string]$InputString = ""
    )

    $result = $InputString -replace "`0", ""
    $result = $result -replace "`r", ""
    $result = $result -replace "`n", ""
    $result = $result -replace "[^A-Za-z0-9._/-]", "-"
    $result = $result -replace "-+", "-"
    $result = $result -replace "^-+", ""
    $result = $result -replace "-+$", ""

    if ([string]::IsNullOrEmpty($result)) {
        $result = "ref-unknown"
    }

    return $result
}

<#
.SYNOPSIS
    Produces a filename-safe string (no slashes).

.DESCRIPTION
    Uses Sanitize-Ref and replaces forward slashes with underscores.

.PARAMETER InputString
    The string to sanitize.

.EXAMPLE
    Sanitize-Filename "feature/my-branch"
    Returns "feature_my-branch"
#>
function Sanitize-Filename {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$false, ValueFromPipeline=$true)]
        [AllowEmptyString()]
        [string]$InputString = ""
    )

    $result = Sanitize-Ref -InputString $InputString
    $result = $result.Replace("/", "_")
    return $result
}

<#
.SYNOPSIS
    Echoes arguments after sanitizing as print (multi-line).

.DESCRIPTION
    Useful as a drop-in replacement for echo when outputting to summaries.

.PARAMETER InputString
    The string(s) to sanitize and echo.

.EXAMPLE
    Safe-EchoForSummary "Hello" "World"
    Returns sanitized multi-line output.
#>
function Safe-EchoForSummary {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$false, ValueFromRemainingArguments=$true)]
        [AllowEmptyString()]
        [string[]]$InputString = @()
    )

    $joined = $InputString -join " "
    $sanitized = Sanitize-Print -InputString $joined
    Write-Output $sanitized
}

<#
.SYNOPSIS
    Detect hidden Unicode characters in a string (detection system, not filter).

.DESCRIPTION
    Returns $true if hidden characters are found (DANGEROUS).
    Returns $false if the string is clean.
    Emits [CRITICAL] diagnostics to stderr when hidden chars detected.
    Achieves parity with GitHub UI warning:
    "The head ref may contain hidden characters"

    Detects: BOM (U+FEFF), Bidi overrides (U+202A-202E, U+2066-2069),
    Zero-width chars (U+200B-200F, U+2060), Line/Paragraph separators
    (U+2028-2029), Interlinear annotation (U+FFF9-FFFB).

.PARAMETER InputString
    The string to check for hidden characters.

.PARAMETER Label
    Optional context label for diagnostic output (e.g., "GITHUB_HEAD_REF").

.EXAMPLE
    if (Detect-HiddenChars -InputString $env:GITHUB_HEAD_REF -Label "HEAD_REF") {
        Write-Host "[CRITICAL] Hidden chars in branch name"
    }
#>
function Detect-HiddenChars {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)]
        [AllowEmptyString()]
        [string]$InputString,

        [Parameter(Mandatory=$false)]
        [string]$Label = "input"
    )

    if ([string]::IsNullOrEmpty($InputString)) {
        return $false
    }

    $found = $false
    $details = @()

    # BOM / Zero-Width No-Break Space (U+FEFF)
    if ($InputString.Contains([char]0xFEFF)) {
        $details += "  - U+FEFF (BOM / Zero-Width No-Break Space)"
        $found = $true
    }

    # Bidi overrides (U+202A-202E)
    foreach ($cp in 0x202A..0x202E) {
        if ($InputString.Contains([char]$cp)) {
            $details += "  - U+202A-202E (Bidi Override/Embedding)"
            $found = $true
            break
        }
    }

    # Bidi isolates (U+2066-2069)
    foreach ($cp in 0x2066..0x2069) {
        if ($InputString.Contains([char]$cp)) {
            $details += "  - U+2066-2069 (Bidi Isolate)"
            $found = $true
            break
        }
    }

    # Zero-width chars (U+200B-200F)
    foreach ($cp in 0x200B..0x200F) {
        if ($InputString.Contains([char]$cp)) {
            $details += "  - U+200B-200F (Zero-Width Space/Joiner/Mark)"
            $found = $true
            break
        }
    }

    # Word joiner (U+2060)
    if ($InputString.Contains([char]0x2060)) {
        $details += "  - U+2060 (Word Joiner)"
        $found = $true
    }

    # Line/Paragraph separators (U+2028-2029)
    foreach ($cp in 0x2028..0x2029) {
        if ($InputString.Contains([char]$cp)) {
            $details += "  - U+2028-2029 (Line/Paragraph Separator)"
            $found = $true
            break
        }
    }

    # Interlinear annotation (U+FFF9-FFFB)
    foreach ($cp in 0xFFF9..0xFFFB) {
        if ($InputString.Contains([char]$cp)) {
            $details += "  - U+FFF9-FFFB (Interlinear Annotation)"
            $found = $true
            break
        }
    }

    # Broad check: any non-ASCII non-printable
    if (-not $found) {
        foreach ($c in $InputString.ToCharArray()) {
            $cp = [int]$c
            if ($cp -gt 127 -or ($cp -lt 32 -and $cp -ne 10 -and $cp -ne 13)) {
                $details += "  - Non-ASCII/control character(s) detected (U+$($cp.ToString('X4')))"
                $found = $true
                break
            }
        }
    }

    if ($found) {
        $hexBytes = [System.BitConverter]::ToString(
            [System.Text.Encoding]::UTF8.GetBytes($InputString)
        ).Replace("-","").ToLower()
        $sanitized = Sanitize-Ref -InputString $InputString

        Write-Host "[CRITICAL] Hidden Unicode characters detected in $Label" -ForegroundColor Red
        foreach ($d in $details) { Write-Host $d -ForegroundColor Yellow }
        Write-Host "  Raw bytes: $($hexBytes.Substring(0, [Math]::Min($hexBytes.Length, 120)))"
        Write-Host "  Sanitized: $sanitized"
        Write-Host '  GitHub UI parity: this finding matches GitHub warning'
        Write-Host '    "The head ref may contain hidden characters"'
    }

    return $found
}

<#
.SYNOPSIS
    Validate a ref name: detect hidden chars + return sanitized version.

.DESCRIPTION
    Wrapper that combines detection (alert to stderr) with sanitization
    (clean output on stdout). Always returns a safe string.

.PARAMETER InputString
    The ref name to validate.

.PARAMETER Label
    Optional context label for diagnostic output.

.EXAMPLE
    $cleanRef = Validate-Ref -InputString $env:GITHUB_HEAD_REF -Label "HEAD_REF"
#>
function Validate-Ref {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)]
        [AllowEmptyString()]
        [string]$InputString,

        [Parameter(Mandatory=$false)]
        [string]$Label = "ref"
    )

    $null = Detect-HiddenChars -InputString $InputString -Label $Label
    return (Sanitize-Ref -InputString $InputString)
}

<#
.SYNOPSIS
    Returns the sanitizer version marker.

.DESCRIPTION
    Provides a version string that callers can check to verify presence.

.EXAMPLE
    Sanitizer-Version
    Returns "iccDEV-sanitizer-v4"
#>
function Sanitizer-Version {
    return "iccDEV-sanitizer-v4"
}

# Functions are automatically available when dot-sourced
# Note: Export-ModuleMember is only used in module files (.psm1), not scripts (.ps1)
