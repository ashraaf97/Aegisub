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

/// @file dialog_ai.h
/// @brief Run an AI action over the selected lines and review the result.
/// @ingroup ai

#include <functional>
#include <string>

#include <wx/string.h>

namespace agi { struct Context; }

/// One AI action, described as data.
///
/// The actions differ only in their prompt and their one parameter, so they
/// are values rather than subclasses: adding another means adding a spec in
/// command/ai.cpp, not new plumbing here.
struct AiActionSpec {
	/// Undo description, e.g. "ai proofread". Lower case, like the other
	/// commands, because it is shown as "Undo ai proofread".
	std::string undo_message;

	wxString title;

	/// Label for the single parameter field, or empty for actions that take
	/// no parameter. Free-form prompt and translate use this; proofread does
	/// not.
	wxString param_label;
	wxString param_default;

	/// True when the action is useless without a parameter, so the dialog can
	/// refuse to send an empty one.
	bool param_required = false;

	/// Builds the system prompt. Receives the parameter, empty if unused.
	std::function<std::string (std::string const&)> system_prompt;
};

/// Show the dialog for @p spec over the current selection.
///
/// Does nothing but warn if the selection has no rewritable text at all, which
/// happens when every selected line is pure typesetting.
void ShowAiDialog(agi::Context *c, AiActionSpec spec);
