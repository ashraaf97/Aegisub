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

#include <libaegisub/ass/text_segments.h>

#include <main.h>

using namespace agi::ass;

namespace {
/// The substrings the segments actually cover, which is what a caller sends
/// off to be rewritten.
std::vector<std::string> texts(std::string const& body) {
	std::vector<std::string> out;
	for (auto const& seg : TextSegments(body))
		out.push_back(body.substr(seg.begin, seg.length));
	return out;
}

/// Replace every segment with the same marker, to show what survives.
std::string replace_all_with(std::string const& body, std::string const& with) {
	auto segs = TextSegments(body);
	std::vector<std::string> reps(segs.size(), with);
	std::string out;
	EXPECT_TRUE(ReplaceTextSegments(body, segs, reps, out));
	return out;
}
}

TEST(lagi_text_segments, plain_line_is_one_segment) {
	EXPECT_EQ(std::vector<std::string>({"Hello there"}), texts("Hello there"));
}

TEST(lagi_text_segments, empty_line_has_no_segments) {
	EXPECT_TRUE(TextSegments("").empty());
}

TEST(lagi_text_segments, override_block_is_not_text) {
	EXPECT_EQ(std::vector<std::string>({"Hello"}), texts("{\\i1}Hello"));
}

TEST(lagi_text_segments, line_with_only_tags_has_no_segments) {
	EXPECT_TRUE(TextSegments("{\\pos(100,200)}").empty());
}

TEST(lagi_text_segments, text_around_and_between_tags) {
	EXPECT_EQ(std::vector<std::string>({"a", "b", "c"}),
	          texts("a{\\i1}b{\\i0}c"));
}

TEST(lagi_text_segments, drawings_are_not_text) {
	// The numbers are vector drawing commands, not prose -- handing them to a
	// translator or a model would destroy the shape.
	auto segs = texts("{\\p1}m 0 0 l 100 0 100 100{\\p0}");
	for (auto const& s : segs)
		EXPECT_EQ(std::string::npos, s.find("m 0 0"));
}

TEST(lagi_text_segments, replace_preserves_override_tags) {
	EXPECT_EQ("{\\pos(100,200)\\an8}X",
	          replace_all_with("{\\pos(100,200)\\an8}Hello", "X"));
}

TEST(lagi_text_segments, replace_preserves_tags_between_text) {
	EXPECT_EQ("X{\\i1}X{\\i0}X", replace_all_with("a{\\i1}b{\\i0}c", "X"));
}

TEST(lagi_text_segments, replace_preserves_drawings) {
	std::string const body = "{\\p1}m 0 0 l 100 0{\\p0}";
	// Whatever the segmentation, the drawing commands must come out intact.
	EXPECT_NE(std::string::npos, replace_all_with(body, "X").find("m 0 0 l 100 0"));
}

TEST(lagi_text_segments, identity_replacement_round_trips) {
	for (std::string const body : {
		"Hello there",
		"{\\i1}Hello{\\i0} there",
		"a{\\i1}b{\\i0}c",
		"{\\pos(1,2)}Line one\\NLine two",
		"{\\p1}m 0 0 l 5 5{\\p0}",
		"",
	}) {
		auto segs = TextSegments(body);
		std::vector<std::string> same;
		for (auto const& seg : segs)
			same.push_back(body.substr(seg.begin, seg.length));

		std::string out;
		ASSERT_TRUE(ReplaceTextSegments(body, segs, same, out));
		EXPECT_EQ(body, out) << "round trip changed: " << body;
	}
}

TEST(lagi_text_segments, count_mismatch_is_rejected) {
	std::string const body = "a{\\i1}b";
	auto segs = TextSegments(body);
	ASSERT_EQ(2u, segs.size());

	std::string out = "untouched";
	// A short response must not be applied -- that is how lines get corrupted.
	EXPECT_FALSE(ReplaceTextSegments(body, segs, {"x"}, out));
	EXPECT_EQ("untouched", out);

	EXPECT_FALSE(ReplaceTextSegments(body, segs, {"x", "y", "z"}, out));
	EXPECT_EQ("untouched", out);
}

TEST(lagi_text_segments, out_of_range_segment_is_rejected) {
	std::string out;
	EXPECT_FALSE(ReplaceTextSegments("abc", {{1, 99}}, {"x"}, out));
}

TEST(lagi_text_segments, segments_are_ordered_and_disjoint) {
	auto segs = TextSegments("a{\\i1}bb{\\i0}ccc{\\b1}d");
	size_t prev_end = 0;
	for (auto const& seg : segs) {
		EXPECT_GE(seg.begin, prev_end);
		prev_end = seg.begin + seg.length;
	}
}
