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

/// @file ai_client.h
/// @brief Ollama Cloud chat client, built on wxWebRequest.
/// @ingroup ai

#include <functional>
#include <string>

class wxEvtHandler;

namespace ai {

struct ChatResult {
	bool ok = false;
	std::string content; ///< The assistant's reply, when ok.
	std::string error;   ///< Human-readable failure. Never contains the key.
};

/// Send a one-shot, non-streaming chat request to the configured endpoint.
///
/// The request runs asynchronously against the wx event loop -- wxWebRequest
/// does its own I/O off the main thread and posts state events back -- so
/// @p cb is always invoked on the UI thread and may touch widgets directly.
/// Do not wrap this in a thread or in agi::dispatch.
///
/// @p handler must outlive the request; when it is destroyed the pending
/// callback is dropped rather than run against a dangling object.
///
/// Failures (no API key, HTTP 401/429/5xx, timeout, unparseable body) arrive
/// through ChatResult::error rather than as exceptions.
void Chat(wxEvtHandler *handler,
          std::string const& system_prompt,
          std::string const& user_prompt,
          std::function<void (ChatResult)> cb);

/// As Chat(), but with an explicit key rather than the configured one.
///
/// This exists for the Preferences "Test connection" button: the key the user
/// has just typed lives in the dialog's pending changes and is not in the
/// options until Apply, so testing the stored key would test the wrong thing.
void ChatWithKey(wxEvtHandler *handler,
                 std::string const& api_key,
                 std::string const& system_prompt,
                 std::string const& user_prompt,
                 std::function<void (ChatResult)> cb);

/// Cancel and forget every request issued for @p handler.
///
/// Call from the owner's destructor: wxWebRequest outlives the dialog that
/// started it otherwise, and its callback would run against freed widgets.
void CancelFor(wxEvtHandler *handler);

}
