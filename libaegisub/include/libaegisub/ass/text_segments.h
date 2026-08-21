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

/// @file text_segments.h
/// @brief Split an ASS line body into the runs of text that are safe to
///        rewrite, keeping override tags and drawings intact.
/// @ingroup libaegisub ass

#include <map>
#include <string>
#include <vector>

namespace agi { namespace ass {

/// A run of plain, human-readable text within an ASS line body.
struct TextSegment {
	size_t begin;  ///< Byte offset into the body.
	size_t length; ///< Byte length.
};

/// Find the runs of an ASS line body that carry human-readable text.
///
/// Override blocks ({\pos(1,2)}, {\i1}), vector drawings (\p1 ... \p0) and
/// line breaks (\N, \h) are all excluded, so the returned segments are what
/// can be handed to a translator, a spell checker or a language model without
/// destroying the typesetting.
///
/// Segments are returned in order and never overlap.
std::vector<TextSegment> TextSegments(std::string const& body);

/// Rebuild a line body with each segment replaced by the matching string.
///
/// Everything outside the segments -- every override block, drawing and line
/// break -- is copied through byte for byte. @p replacements must have exactly
/// one entry per segment; a mismatch means the caller is working from a stale
/// or malformed response and would corrupt the line, so nothing is replaced
/// and false is returned.
bool ReplaceTextSegments(std::string const& body,
                         std::vector<TextSegment> const& segments,
                         std::vector<std::string> const& replacements,
                         std::string &out);

/// Render texts as a numbered list, one per line, for a language model.
///
/// Numbering is what makes the reply matchable: models reorder, merge and drop
/// lines, and without an explicit index there is no way to tell which output
/// belongs to which input. Newlines inside an item are escaped so one item
/// always occupies exactly one line.
std::string BuildNumberedList(std::vector<std::string> const& texts);

/// Parse a numbered list produced in reply to BuildNumberedList.
///
/// Returns index -> text for every "N. text" or "N<tab>text" line recognised.
/// Anything else -- preamble, code fences, commentary the model added -- is
/// ignored rather than treated as content. Indices are 1-based, matching what
/// BuildNumberedList emits; the caller decides what a missing index means.
std::map<size_t, std::string> ParseNumberedList(std::string const& reply);

} }
