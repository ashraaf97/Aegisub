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
			// The tokenizer emits one TEXT token per run, but coalesce
			// adjacent ones anyway so callers never see a run split in a
			// place that has no meaning to a reader.
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

} }
