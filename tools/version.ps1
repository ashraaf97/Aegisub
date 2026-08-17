#!/usr/bin/env powershell
#
# Copyright (c) 2026, Aegisub Project
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
#
# Aegisub Project http://www.aegisub.org/

# PowerShell port of build/version.sh, for the Meson build on Windows.
# Generates $BuildRoot/git_version.h.
#
# The revision number continues the numbering inherited from the original
# Subversion repository so that builds stay comparable with historical
# releases: revision 6962 corresponded to commit 16cd907, and every commit
# since then counts as one more revision.

param (
    [Parameter(Position = 0, Mandatory = $true)]
    [string] $BuildRoot,
    [Parameter(Position = 1, Mandatory = $false)]
    [string] $SourceRoot = $null
)

$ErrorActionPreference = 'Stop'

$lastSvnRevision = 6962
$lastSvnHash = '16cd907fe7482cb54a7374cd28b8501f138116be'

if ([string]::IsNullOrWhiteSpace($SourceRoot)) {
    $SourceRoot = Join-Path $PSScriptRoot '..'
}
$SourceRoot = (Resolve-Path -LiteralPath $SourceRoot).Path

function Write-IfChanged {
    param([string] $Path, [string] $Content)

    # Meson runs this target on every build (build_always_stale), so only
    # touch the file when it actually changes -- otherwise the resource
    # compiler and version.cpp would rebuild on every single invocation.
    if (Test-Path -LiteralPath $Path) {
        $existing = Get-Content -LiteralPath $Path -Raw -ErrorAction SilentlyContinue
        if ($null -ne $existing -and $existing.Replace("`r`n", "`n") -eq $Content.Replace("`r`n", "`n")) {
            return
        }
    }

    $dir = Split-Path -Parent $Path
    if ($dir -and -not (Test-Path -LiteralPath $dir)) {
        New-Item -ItemType Directory -Path $dir -Force | Out-Null
    }
    Set-Content -LiteralPath $Path -Value $Content -NoNewline -Encoding utf8
}

$outputFile = Join-Path $BuildRoot 'git_version.h'

# Building from a source tarball rather than a git checkout: reuse the
# cached header that shipped with the tarball.
if (-not (Test-Path -LiteralPath (Join-Path $SourceRoot '.git'))) {
    $cached = Join-Path $SourceRoot 'build/git_version.h'
    if (-not (Test-Path -LiteralPath $cached)) {
        throw 'git repo not found and no cached build/git_version.h'
    }

    $content = Get-Content -LiteralPath $cached -Raw
    if ($content -notmatch 'BUILD_GIT_VERSION_NUMBER' -or $content -notmatch 'BUILD_GIT_VERSION_STRING') {
        throw "invalid $cached"
    }

    Write-IfChanged -Path $outputFile -Content $content
    exit 0
}

$commitsSince = @(& git -C $SourceRoot log --pretty=oneline "$lastSvnHash..HEAD" 2>$null).Count
$revision = $lastSvnRevision + $commitsSince

$installerVersion = '0.0.0'
$resourceVersion = '0, 0, 0'

$exactTag = & git -C $SourceRoot describe --exact-match 2>$null
if ($LASTEXITCODE -eq 0 -and -not [string]::IsNullOrWhiteSpace($exactTag)) {
    $versionString = ([string]$exactTag).Trim() -replace '^v', ''
    $taggedRelease = 1

    # Only a plain x.y.z tag can populate the installer/resource fields;
    # anything else leaves them at the 0.0.0 placeholder.
    if ($versionString -match '^\d+\.\d+\.\d+$') {
        $installerVersion = $versionString
        $resourceVersion = $versionString -replace '\.', ', '
    }
}
else {
    $branch = & git -C $SourceRoot symbolic-ref HEAD 2>$null
    if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($branch)) {
        $branch = '(unnamed branch)'
    }
    $branch = ([string]$branch).Trim() -replace '^refs/heads/', ''

    $hash = ([string](& git -C $SourceRoot rev-parse --short HEAD 2>$null)).Trim()

    $versionString = "$revision-$branch-$hash"
    $taggedRelease = 0
}

Write-IfChanged -Path $outputFile -Content @"
#define BUILD_GIT_VERSION_NUMBER $revision
#define BUILD_GIT_VERSION_STRING "$versionString"
#define TAGGED_RELEASE $taggedRelease
#define INSTALLER_VERSION "$installerVersion"
#define RESOURCE_BASE_VERSION $resourceVersion

"@
