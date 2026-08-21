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

/// @file ai.cpp
/// @brief ai/ commands -- language model actions over the selection.
/// @ingroup command ai

#include "command.h"

#include "../compat.h"
#include "../dialog_ai.h"
#include "../include/aegisub/context.h"
#include "../options.h"
#include "../selection_controller.h"

#include <memory>
#include <string>

namespace {
	using cmd::Command;

/// Rules every action shares.
///
/// The output contract is the load-bearing part: the reply is matched back to
/// the input by number, so a model that renumbers, merges or explains itself
/// costs the user those lines.
std::string CommonRules() {
	return
		"You are helping edit subtitle text.\n"
		"The user's message is a numbered list. Reply with the same numbered "
		"list, same count, same order, same numbering.\n"
		"Output nothing but the numbered list: no preamble, no commentary, no "
		"code fences.\n"
		"Never merge or split items. If an item needs no change, repeat it "
		"unchanged.\n"
		"Each item is one run of subtitle text. Formatting tags have already "
		"been removed, so do not add any.\n";
}

struct ai_proofread final : public Command {
	CMD_NAME("ai/proofread")
	STR_MENU("Proofread with AI...")
	STR_DISP("Proofread with AI")
	STR_HELP("Fix spelling, grammar and punctuation in the selected lines")
	CMD_TYPE(cmd::COMMAND_VALIDATE)

	bool Validate(const agi::Context *c) override {
		return !c->selectionController->GetSelectedSet().empty();
	}

	void operator()(agi::Context *c) override {
		AiActionSpec spec;
		spec.undo_message = "ai proofread";
		spec.title = _("Proofread with AI");
		spec.system_prompt = [](std::string const&) {
			return CommonRules() +
				"Fix spelling, grammar and punctuation only. Preserve the "
				"original wording, tone and meaning. Do not translate, "
				"rephrase or shorten.\n";
		};
		ShowAiDialog(c, std::move(spec));
	}
};

struct ai_rephrase final : public Command {
	CMD_NAME("ai/rephrase")
	STR_MENU("Shorten with AI...")
	STR_DISP("Shorten with AI")
	STR_HELP("Shorten the selected lines so they read comfortably in their duration")
	CMD_TYPE(cmd::COMMAND_VALIDATE)

	bool Validate(const agi::Context *c) override {
		return !c->selectionController->GetSelectedSet().empty();
	}

	void operator()(agi::Context *c) override {
		AiActionSpec spec;
		spec.undo_message = "ai shorten";
		spec.title = _("Shorten with AI");
		spec.param_label = _("Target characters per second");
		// Matches the CPS warning threshold Aegisub already ships with, so
		// the two features do not disagree about what "too fast" means.
		spec.param_default = to_wx(std::to_string(OPT_GET("Subtitle/Character Counter/CPS Warning Threshold")->GetInt()));
		spec.system_prompt = [](std::string const& cps) {
			return CommonRules() +
				"Shorten each item so it can be read comfortably at about " +
				(cps.empty() ? std::string("15") : cps) +
				" characters per second. Keep the meaning and the tone. "
				"Prefer cutting filler over cutting information. If an item "
				"is already short enough, repeat it unchanged.\n";
		};
		ShowAiDialog(c, std::move(spec));
	}
};

struct ai_translate final : public Command {
	CMD_NAME("ai/translate")
	STR_MENU("Translate with AI...")
	STR_DISP("Translate with AI")
	STR_HELP("Translate the selected lines into another language")
	CMD_TYPE(cmd::COMMAND_VALIDATE)

	bool Validate(const agi::Context *c) override {
		return !c->selectionController->GetSelectedSet().empty();
	}

	void operator()(agi::Context *c) override {
		AiActionSpec spec;
		spec.undo_message = "ai translate";
		spec.title = _("Translate with AI");
		spec.param_label = _("Target language");
		spec.param_required = true;
		spec.system_prompt = [](std::string const& lang) {
			return CommonRules() +
				"Translate each item into " + lang + ". Translate only; do "
				"not explain, annotate or transliterate. Keep the register of "
				"the original -- casual stays casual. Preserve speaker "
				"punctuation such as dashes and ellipses.\n";
		};
		ShowAiDialog(c, std::move(spec));
	}
};

struct ai_prompt final : public Command {
	CMD_NAME("ai/prompt")
	STR_MENU("Custom AI prompt...")
	STR_DISP("Custom AI prompt")
	STR_HELP("Apply your own instruction to the selected lines")
	CMD_TYPE(cmd::COMMAND_VALIDATE)

	bool Validate(const agi::Context *c) override {
		return !c->selectionController->GetSelectedSet().empty();
	}

	void operator()(agi::Context *c) override {
		AiActionSpec spec;
		spec.undo_message = "ai prompt";
		spec.title = _("Custom AI prompt");
		spec.param_label = _("Instruction");
		spec.param_required = true;
		spec.system_prompt = [](std::string const& instruction) {
			return CommonRules() +
				"Apply this instruction to every item:\n" + instruction + "\n";
		};
		ShowAiDialog(c, std::move(spec));
	}
};

}

namespace cmd {
	void init_ai() {
		reg(std::make_unique<ai_proofread>());
		reg(std::make_unique<ai_prompt>());
		reg(std::make_unique<ai_rephrase>());
		reg(std::make_unique<ai_translate>());
	}
}
