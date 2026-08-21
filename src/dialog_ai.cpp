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

/// @file dialog_ai.cpp
/// @brief Run an AI action over the selected lines and review the result.
/// @ingroup ai

#include "dialog_ai.h"

#include "ai_client.h"
#include "ass_dialogue.h"
#include "ass_file.h"
#include "compat.h"
#include "format.h"
#include "include/aegisub/context.h"
#include "options.h"
#include "selection_controller.h"

#include <libaegisub/ass/text_segments.h>

#include <algorithm>
#include <map>
#include <vector>

#include <wx/button.h>
#include <wx/dialog.h>
#include <wx/listctrl.h>
#include <wx/msgdlg.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

namespace {

/// One selected line, plus where its text sits in the flattened request.
struct LineWork {
	AssDialogue *line = nullptr;
	std::vector<agi::ass::TextSegment> segments;
	/// 1-based index of this line's first segment in the numbered list.
	size_t first_index = 0;
	/// The rewritten line body, once a reply has been matched to it.
	std::string proposed;
	bool have_proposal = false;
};

class DialogAi final : public wxDialog {
	agi::Context *c;
	AiActionSpec spec;

	std::vector<LineWork> work;
	/// Every rewritable run across every selected line, in order. The index
	/// into this is what the numbered list numbers.
	std::vector<std::string> flat;

	wxTextCtrl *param = nullptr;
	wxListCtrl *preview = nullptr;
	wxStaticText *status = nullptr;
	wxButton *run_button = nullptr;
	wxButton *apply_button = nullptr;

	void OnRun();
	void OnApply();
	void Populate(std::map<size_t, std::string> const& replies);

public:
	DialogAi(agi::Context *c, AiActionSpec spec);
	~DialogAi();
};

DialogAi::DialogAi(agi::Context *c, AiActionSpec spec_)
: wxDialog(c->parent, -1, spec_.title, wxDefaultPosition, wxSize(760, 480),
           wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
, c(c)
, spec(std::move(spec_))
{
	auto main = new wxBoxSizer(wxVERTICAL);

	if (!spec.param_label.empty()) {
		auto row = new wxBoxSizer(wxHORIZONTAL);
		row->Add(new wxStaticText(this, -1, spec.param_label), 0,
		         wxALIGN_CENTRE_VERTICAL | wxRIGHT, 5);
		param = new wxTextCtrl(this, -1, spec.param_default);
		row->Add(param, 1, wxEXPAND);
		main->Add(row, 0, wxEXPAND | wxALL, 5);
	}

	preview = new wxListCtrl(this, -1, wxDefaultPosition, wxDefaultSize,
	                         wxLC_REPORT | wxLC_HRULES);
	// Per-line opt-out: a model can be wrong about one line out of twenty,
	// and the alternative is accepting the whole batch or none of it.
	preview->EnableCheckBoxes(true);
	preview->AppendColumn(_("Line"), wxLIST_FORMAT_RIGHT, 50);
	preview->AppendColumn(_("Original"), wxLIST_FORMAT_LEFT, 330);
	preview->AppendColumn(_("Proposed"), wxLIST_FORMAT_LEFT, 330);
	main->Add(preview, 1, wxEXPAND | wxALL, 5);

	status = new wxStaticText(this, -1, "");
	main->Add(status, 0, wxEXPAND | wxLEFT | wxRIGHT, 5);

	auto buttons = new wxBoxSizer(wxHORIZONTAL);
	run_button = new wxButton(this, -1, _("Run"));
	apply_button = new wxButton(this, wxID_OK, _("Apply"));
	apply_button->Enable(false);
	buttons->Add(run_button, 0, wxRIGHT, 5);
	buttons->AddStretchSpacer();
	buttons->Add(apply_button, 0, wxRIGHT, 5);
	buttons->Add(new wxButton(this, wxID_CANCEL, _("Cancel")));
	main->Add(buttons, 0, wxEXPAND | wxALL, 5);

	SetSizer(main);

	run_button->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { OnRun(); });
	apply_button->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { OnApply(); });

	// Selection is a std::set, so it carries no useful order; the grid order
	// is what the user sees and what the numbering has to follow.
	std::vector<AssDialogue *> lines(c->selectionController->GetSelectedSet().begin(),
	                                 c->selectionController->GetSelectedSet().end());
	std::sort(lines.begin(), lines.end(),
	          [](AssDialogue *a, AssDialogue *b) { return a->Row < b->Row; });

	size_t next_index = 1;
	for (auto line : lines) {
		LineWork w;
		w.line = line;
		w.segments = agi::ass::TextSegments(line->Text.get());
		// Nothing to rewrite: a pure typesetting or drawing line.
		if (w.segments.empty()) continue;

		w.first_index = next_index;
		for (auto const& seg : w.segments) {
			flat.push_back(line->Text.get().substr(seg.begin, seg.length));
			++next_index;
		}
		work.push_back(std::move(w));
	}

	for (size_t i = 0; i < work.size(); ++i) {
		long row = preview->InsertItem(static_cast<long>(i),
		                               std::to_wstring(work[i].line->Row + 1));
		preview->SetItem(row, 1, to_wx(work[i].line->Text.get()));
	}

	status->SetLabel(agi::wxformat(_("%d line(s) with text, %d run(s) to rewrite."),
	                             (int)work.size(), (int)flat.size()));
}

DialogAi::~DialogAi() {
	// The request outlives this dialog otherwise, and its callback would run
	// against freed widgets.
	ai::CancelFor(this);
}

void DialogAi::OnRun() {
	if (work.empty()) {
		status->SetLabel(_("Nothing in the selection can be rewritten."));
		return;
	}

	std::string param_value = param ? from_wx(param->GetValue()) : std::string();
	if (spec.param_required && param_value.empty()) {
		status->SetLabel(agi::wxformat(_("%s is required."), spec.param_label));
		return;
	}

	int const max_lines = (int)OPT_GET("AI/Max Lines Per Request")->GetInt();
	if ((int)flat.size() > max_lines) {
		status->SetLabel(agi::wxformat(
			_("Selection has %d runs, over the %d configured for one request. Select fewer lines."),
			(int)flat.size(), max_lines));
		return;
	}

	run_button->Enable(false);
	apply_button->Enable(false);
	status->SetLabel(_("Waiting for the model..."));

	ai::Chat(this, spec.system_prompt(param_value),
	         agi::ass::BuildNumberedList(flat),
	         [this](ai::ChatResult r) {
		run_button->Enable(true);
		if (!r.ok) {
			status->SetLabel(to_wx(r.error));
			return;
		}
		Populate(agi::ass::ParseNumberedList(r.content));
	});
}

void DialogAi::Populate(std::map<size_t, std::string> const& replies) {
	int matched = 0, skipped = 0;

	for (size_t i = 0; i < work.size(); ++i) {
		auto &w = work[i];
		w.have_proposal = false;

		std::vector<std::string> replacements;
		bool complete = true;
		for (size_t s = 0; s < w.segments.size(); ++s) {
			auto it = replies.find(w.first_index + s);
			if (it == replies.end()) { complete = false; break; }
			replacements.push_back(it->second);
		}

		// A line is all or nothing: rewriting only some of its runs would
		// leave a half-translated, half-original line.
		if (!complete ||
		    !agi::ass::ReplaceTextSegments(w.line->Text.get(), w.segments,
		                                   replacements, w.proposed)) {
			++skipped;
			preview->SetItem((long)i, 2, _("(no usable reply)"));
			preview->CheckItem((long)i, false);
			continue;
		}

		w.have_proposal = true;
		++matched;
		preview->SetItem((long)i, 2, to_wx(w.proposed));
		// Unchanged lines are left unchecked: there is nothing to apply and
		// checking them only invites an empty undo entry.
		preview->CheckItem((long)i, w.proposed != w.line->Text.get());
	}

	apply_button->Enable(matched > 0);
	if (skipped)
		status->SetLabel(agi::wxformat(_("%d line(s) ready, %d skipped -- the reply did not cover them."),
		                             matched, skipped));
	else
		status->SetLabel(agi::wxformat(_("%d line(s) ready. Uncheck any you do not want."), matched));
}

void DialogAi::OnApply() {
	int applied = 0;
	for (size_t i = 0; i < work.size(); ++i) {
		auto const& w = work[i];
		if (!w.have_proposal || !preview->IsItemChecked((long)i)) continue;
		if (w.proposed == w.line->Text.get()) continue;
		w.line->Text = w.proposed;
		++applied;
	}

	if (applied) {
		// One commit for the whole batch, so a single undo takes it all back.
		auto const& sel = c->selectionController->GetSelectedSet();
		c->ass->Commit(to_wx(spec.undo_message), AssFile::COMMIT_DIAG_TEXT, -1,
		               sel.size() == 1 ? *sel.begin() : nullptr);
	}

	EndModal(wxID_OK);
}

}

void ShowAiDialog(agi::Context *c, AiActionSpec spec) {
	if (c->selectionController->GetSelectedSet().empty()) {
		wxMessageBox(_("Select some lines first."), _("AI"), wxOK | wxICON_INFORMATION);
		return;
	}

	DialogAi dlg(c, std::move(spec));
	dlg.ShowModal();
}
