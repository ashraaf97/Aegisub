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

/// @file text_segments.cpp
/// @brief Split an ASS line body into the runs of text that are safe to
///        rewrite.
/// @ingroup libaegisub ass

#include "libaegisub/ass/text_segments.h"

#include "libaegisub/ass/dialogue_parser.h"

#include <cctype>
#include <exception>
#include <sstream>
#include <stdexcept>

namespace {
namespace dt = agi::ass::DialogueTokenType;
}

namespace agi { namespace ass {

std::vector<TextSegment> TextSegments(std::string const& body) {
	auto tokens = TokenizeDialogueBody(body);
	// Without this, the contents of a \p1 drawing are still typed TEXT and
	// would be handed out as if they were prose.
	MarkDrawings(body, tokens);

	std::vector<TextSegment> segments;
	size_t pos = 0;

	for (auto const& tok : tokens) {
		if (tok.type == dt::TEXT && tok.length > 0) {
			// Coalesce runs that touch, so a caller never sees a run split at
			// a point that means nothing to a reader.
			if (!segments.empty() && segments.back().begin + segments.back().length == pos)
				segments.back().length += tok.length;
			else
				segments.push_back({pos, tok.length});
		}
		pos += tok.length;
	}

	return segments;
}

bool ReplaceTextSegments(std::string const& body,
                         std::vector<TextSegment> const& segments,
                         std::vector<std::string> const& replacements,
                         std::string &out) {
	if (segments.size() != replacements.size())
		return false;

	std::string result;
	result.reserve(body.size());

	size_t pos = 0;
	for (size_t i = 0; i < segments.size(); ++i) {
		auto const& seg = segments[i];
		// Segments come from TextSegments() and are ordered and disjoint;
		// anything else means the caller built them by hand and we would be
		// writing garbage into the line.
		if (seg.begin < pos || seg.begin + seg.length > body.size())
			return false;

		result.append(body, pos, seg.begin - pos);
		result.append(replacements[i]);
		pos = seg.begin + seg.length;
	}
	result.append(body, pos, body.size() - pos);

	out = std::move(result);
	return true;
}

std::string BuildNumberedList(std::vector<std::string> const& texts) {
	std::string out;
	for (size_t i = 0; i < texts.size(); ++i) {
		out += std::to_string(i + 1);
		out += ". ";
		// An item has to stay on one line, or the numbering stops being a
		// reliable way to line the reply up with the input.
		for (char c : texts[i]) {
			if (c == '\n')
				out += "\\n";
			else if (c != '\r')
				out += c;
		}
		out += '\n';
	}
	return out;
}

std::map<size_t, std::string> ParseNumberedList(std::string const& reply) {
	std::map<size_t, std::string> out;

	std::istringstream ss(reply);
	std::string line;
	while (std::getline(ss, line)) {
		if (!line.empty() && line.back() == '\r')
			line.pop_back();

		size_t i = 0;
		while (i < line.size() && std::isspace(static_cast<unsigned char>(line[i])))
			++i;

		size_t const digits_begin = i;
		while (i < line.size() && std::isdigit(static_cast<unsigned char>(line[i])))
			++i;
		if (i == digits_begin) continue; // No leading number: commentary.

		size_t index = 0;
		try {
			index = static_cast<size_t>(std::stoul(line.substr(digits_begin, i - digits_begin)));
		}
		catch (std::exception const&) {
			continue; // Absurdly long digit run; not an index.
		}
		if (index == 0) continue; // Numbering is 1-based.

		// Accept "1. text", "1) text", "1: text" and "1<tab>text": which of
		// these a model picks is not something we can rely on.
		if (i < line.size() && (line[i] == '.' || line[i] == ')' || line[i] == ':'))
			++i;
		else if (i >= line.size() || (line[i] != ' ' && line[i] != '\t'))
			continue; // "12abc" is not a numbered item.

		while (i < line.size() && (line[i] == ' ' || line[i] == '\t'))
			++i;

		std::string const text = line.substr(i);

		// Undo the escaping BuildNumberedList applies.
		std::string unescaped;
		for (size_t j = 0; j < text.size(); ++j) {
			if (text[j] == '\\' && j + 1 < text.size() && text[j + 1] == 'n') {
				unescaped += '\n';
				++j;
			}
			else
				unescaped += text[j];
		}

		// First occurrence wins: a model that repeats an index is usually
		// restating its answer, and the first is what it committed to.
		out.emplace(index, unescaped);
	}

	return out;
}

} }
