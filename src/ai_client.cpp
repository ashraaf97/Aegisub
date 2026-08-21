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

/// @file ai_client.cpp
/// @brief Ollama Cloud chat client, built on wxWebRequest.
/// @ingroup ai

#include "ai_client.h"

#include "ai_credentials.h"
#include "compat.h"
#include "format.h"
#include "options.h"

#include <libaegisub/cajun/elements.h>
#include <libaegisub/cajun/reader.h>
#include <libaegisub/cajun/writer.h>

#include <exception>
#include <sstream>
#include <vector>

#include <wx/event.h>
#include <wx/translation.h>
#include <wx/utils.h>
#include <wx/webrequest.h>

namespace {

/// In-flight requests. wxWebRequest is a refcounted handle -- dropping the
/// last copy cancels the transfer -- so the request has to be held here for
/// the duration. Only ever touched on the UI thread.
struct Pending {
	int id;
	wxWebRequest request;
	wxEvtHandler *owner;
	std::function<void (ai::ChatResult)> cb;
};
std::vector<Pending> g_pending;

std::vector<Pending>::iterator Find(int id) {
	for (auto it = g_pending.begin(); it != g_pending.end(); ++it)
		if (it->id == id) return it;
	return g_pending.end();
}

std::string BuildBody(std::string const& model,
                      std::string const& system_prompt,
                      std::string const& user_prompt) {
	json::Object sys;
	sys["role"] = std::string("system");
	sys["content"] = system_prompt;

	json::Object user;
	user["role"] = std::string("user");
	user["content"] = user_prompt;

	json::Array messages;
	messages.push_back(std::move(sys));
	messages.push_back(std::move(user));

	json::Object root;
	root["model"] = model;
	root["messages"] = std::move(messages);
	// Streaming would mean reassembling partial frames for no benefit: the
	// point here is to diff a finished result against the original lines.
	root["stream"] = false;

	std::ostringstream ss;
	agi::JsonWriter::Write(root, ss);
	return ss.str();
}

/// Pull the assistant text out of an Ollama /api/chat response, or the error
/// message if the server sent one instead.
ai::ChatResult ParseResponse(std::string const& body) {
	ai::ChatResult res;

	json::UnknownElement root;
	try {
		std::istringstream ss(body);
		json::Reader::Read(root, ss);
	}
	catch (std::exception const&) {
		res.error = from_wx(_("The server sent a response that could not be understood."));
		return res;
	}

	try {
		json::Object const& obj = root;

		auto err = obj.find("error");
		if (err != obj.end()) {
			json::String const& msg = err->second;
			res.error = msg;
			if (res.error.empty())
				res.error = from_wx(_("The server reported an unspecified error."));
			return res;
		}

		auto msg_it = obj.find("message");
		if (msg_it == obj.end()) {
			res.error = from_wx(_("The server's reply did not contain a message."));
			return res;
		}

		json::Object const& message = msg_it->second;
		auto content_it = message.find("content");
		if (content_it == message.end()) {
			res.error = from_wx(_("The server's reply did not contain a message."));
			return res;
		}

		json::String const& content = content_it->second;
		res.content = content;
		res.ok = true;
	}
	catch (std::exception const&) {
		// A field was present but of the wrong type.
		res.error = from_wx(_("The server's reply was not in the expected format."));
	}

	return res;
}

std::string HttpErrorText(int status) {
	switch (status) {
		case 401: case 403:
			return from_wx(_("The API key was rejected. Check it in Preferences."));
		case 404:
			return from_wx(_("The model or endpoint was not found."));
		case 429:
			return from_wx(_("Rate limited by the server. Try again shortly."));
		default:
			// Deliberately status-only. The body of an error response can echo
			// the request back, and the request carries the API key.
			return agi::format(_("The server returned HTTP %d."), status);
	}
}

}

namespace ai {

void Chat(wxEvtHandler *handler,
          std::string const& system_prompt,
          std::string const& user_prompt,
          std::function<void (ChatResult)> cb) {
	ChatWithKey(handler, GetApiKey(), system_prompt, user_prompt, std::move(cb));
}

void ChatWithKey(wxEvtHandler *handler,
                 std::string const& key,
                 std::string const& system_prompt,
                 std::string const& user_prompt,
                 std::function<void (ChatResult)> cb) {
	ChatResult fail;

	if (key.empty()) {
		fail.error = from_wx(_("No API key is set. Add one in Preferences."));
		cb(fail);
		return;
	}

	std::string endpoint = OPT_GET("AI/Endpoint")->GetString();
	std::string model = OPT_GET("AI/Model")->GetString();
	if (endpoint.empty() || model.empty()) {
		fail.error = from_wx(_("The AI endpoint or model is not configured."));
		cb(fail);
		return;
	}

	// Every request needs its own id: wxID_ANY would make all of them share
	// one, and the state events could not be told apart.
	static int next_id = wxID_HIGHEST + 1000;
	int const id = next_id++;

	auto request = wxWebSession::GetDefault().CreateRequest(
		handler, to_wx(endpoint + "/api/chat"), id);
	if (!request.IsOk()) {
		fail.error = from_wx(_("Could not create the web request. This build may lack HTTPS support."));
		cb(fail);
		return;
	}

	request.SetMethod("POST");
	request.SetHeader("Authorization", to_wx("Bearer " + key));
	request.SetData(to_wx(BuildBody(model, system_prompt, user_prompt)),
	                "application/json");

	g_pending.push_back({id, request, handler, std::move(cb)});

	handler->Bind(wxEVT_WEBREQUEST_STATE, [](wxWebRequestEvent &evt) {
		auto state = evt.GetState();
		if (state == wxWebRequest::State_Active || state == wxWebRequest::State_Idle)
			return;

		int const id = evt.GetId();
		auto it = Find(id);
		// Already dropped by CancelFor(): the owner is gone or going.
		if (it == g_pending.end()) return;

		auto cb = std::move(it->cb);
		g_pending.erase(it);
		// Not unbound: a lambda cannot be Unbind()ed without keeping the
		// functor around, and the binding is filtered to this one request id,
		// so it is inert afterwards and dies with the handler.

		ChatResult res;
		switch (state) {
			case wxWebRequest::State_Completed: {
				int status = evt.GetResponse().GetStatus();
				if (status >= 200 && status < 300)
					res = ParseResponse(evt.GetResponse().AsString().utf8_str().data());
				else
					res.error = HttpErrorText(status);
				break;
			}
			case wxWebRequest::State_Failed:
				// A transport-level description from wx; never includes headers.
				res.error = from_wx(evt.GetErrorDescription());
				if (res.error.empty())
					res.error = from_wx(_("The request failed."));
				break;
			case wxWebRequest::State_Cancelled:
				res.error = from_wx(_("The request was cancelled."));
				break;
			default:
				res.error = from_wx(_("The request ended unexpectedly."));
				break;
		}

		cb(res);
	}, id);

	request.Start();
}

void CancelFor(wxEvtHandler *handler) {
	for (size_t i = g_pending.size(); i > 0; --i) {
		auto &p = g_pending[i - 1];
		if (p.owner != handler) continue;
		if (p.request.GetState() == wxWebRequest::State_Active)
			p.request.Cancel();
		g_pending.erase(g_pending.begin() + (i - 1));
	}
}

}
