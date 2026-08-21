// Copyright (c) 2026, Aegisub Project
//
// Permission to use, copy, modify, and distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
// WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
// ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
// WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
// ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
// OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
//
// Aegisub Project http://www.aegisub.org/

#pragma once

/// @file ai_credentials.h
/// @brief At-rest protection for the AI provider API key.
/// @ingroup ai

#include <string>

namespace ai {

/// Wrap a secret for storage in the config file.
///
/// On Windows this is DPAPI (CryptProtectData), scoped to the current user,
/// base64'd so it survives a JSON round trip and tagged with a marker so
/// Unprotect can tell a wrapped blob from a key someone pasted in by hand.
/// Elsewhere the value is returned unchanged -- the config file is then only
/// as protected as its file permissions, which is noted in the preferences UI.
std::string Protect(std::string const& plaintext);

/// Unwrap a value produced by Protect.
///
/// A value without the marker is assumed to be a plaintext key -- typed in by
/// hand, or carried over from another machine -- and is returned as-is rather
/// than fed to CryptUnprotectData as garbage. Returns an empty string if the
/// blob is marked but cannot be unwrapped, which is what happens to a config
/// copied between user accounts.
std::string Unprotect(std::string const& stored);

/// True if the platform actually encrypts, so the UI can say so honestly.
bool ProtectionAvailable();

/// The API key to use, or an empty string if none is configured.
///
/// OLLAMA_API_KEY in the environment wins over the stored value, so a key can
/// be supplied to a build without ever touching the config file.
std::string GetApiKey();

/// Store an API key, wrapping it first. An empty string clears it.
void SetApiKey(std::string const& plaintext);

}
