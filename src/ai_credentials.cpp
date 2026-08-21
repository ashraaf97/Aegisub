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

/// @file ai_credentials.cpp
/// @brief At-rest protection for the AI provider API key.
/// @ingroup ai

#include "ai_credentials.h"

#include "options.h"

#include <cstdlib>

#include <wx/base64.h>
#include <wx/buffer.h>
#include <wx/string.h>

#ifdef _WIN32
#include <windows.h>
#include <wincrypt.h>
#endif

namespace {
/// Marks a value as wrapped by Protect(). Without it we cannot distinguish a
/// DPAPI blob from a key the user pasted straight into the config file.
const char *kMarker = "dpapi:";
const size_t kMarkerLen = 6;

bool IsMarked(std::string const& s) {
	return s.compare(0, kMarkerLen, kMarker) == 0;
}
}

namespace ai {

bool ProtectionAvailable() {
#ifdef _WIN32
	return true;
#else
	return false;
#endif
}

std::string Protect(std::string const& plaintext) {
	if (plaintext.empty()) return "";

#ifdef _WIN32
	DATA_BLOB in;
	in.pbData = reinterpret_cast<BYTE *>(const_cast<char *>(plaintext.data()));
	in.cbData = static_cast<DWORD>(plaintext.size());

	DATA_BLOB out = { 0, nullptr };
	// CRYPTPROTECT_UI_FORBIDDEN: this can be reached from a background
	// request, and a modal Windows prompt there would wedge the app.
	if (!CryptProtectData(&in, L"Aegisub AI API key", nullptr, nullptr, nullptr,
	                      CRYPTPROTECT_UI_FORBIDDEN, &out))
		return plaintext; // Better an unwrapped key than a lost one.

	wxString b64 = wxBase64Encode(out.pbData, out.cbData);
	LocalFree(out.pbData);

	return std::string(kMarker) + b64.utf8_str().data();
#else
	return plaintext;
#endif
}

std::string Unprotect(std::string const& stored) {
	if (stored.empty()) return "";
	// Not ours: a hand-edited config, or a platform without DPAPI.
	if (!IsMarked(stored)) return stored;

#ifdef _WIN32
	wxMemoryBuffer buf = wxBase64Decode(stored.c_str() + kMarkerLen,
	                                    stored.size() - kMarkerLen,
	                                    wxBase64DecodeMode_SkipWS);
	if (buf.GetDataLen() == 0) return "";

	DATA_BLOB in;
	in.pbData = static_cast<BYTE *>(buf.GetData());
	in.cbData = static_cast<DWORD>(buf.GetDataLen());

	DATA_BLOB out = { 0, nullptr };
	if (!CryptUnprotectData(&in, nullptr, nullptr, nullptr, nullptr,
	                        CRYPTPROTECT_UI_FORBIDDEN, &out))
		// Typically a config copied from another user account or machine.
		return "";

	std::string result(reinterpret_cast<char *>(out.pbData), out.cbData);
	SecureZeroMemory(out.pbData, out.cbData);
	LocalFree(out.pbData);
	return result;
#else
	// Marked but we cannot unwrap it: the config came from Windows.
	return "";
#endif
}

std::string GetApiKey() {
	if (const char *env = std::getenv("OLLAMA_API_KEY")) {
		if (*env) return env;
	}
	return Unprotect(OPT_GET("AI/API Key")->GetString());
}

void SetApiKey(std::string const& plaintext) {
	OPT_SET("AI/API Key")->SetString(plaintext.empty() ? "" : Protect(plaintext));
}

}
