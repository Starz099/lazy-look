#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace lazylook
{

// Parses temporal hints from a natural language query string.
//
// Recognised patterns (case-insensitive):
//   "today", "yesterday"
//   "this week", "last week", "past week"
//   "this month", "last month", "past month"
//   "this year", "last year", "past year"
//   "past N days", "last N days"  (N = 1..999)
//   "past N weeks", "last N weeks"
//   "past N months", "last N months"
//
// Returns the earliest Unix epoch second that should be included, or
// std::nullopt if no temporal hint was found.  The caller should rewrite the
// query to the returned `cleaned_query` (temporal tokens stripped) and add
//   WHERE f.mtime >= <cutoff>
// to the database query.
struct TemporalMatch
{
  int64_t cutoff_secs = 0;   // inclusive lower bound for file mtime
  std::string cleaned_query; // original query with the matched phrase removed
};

std::optional<TemporalMatch> parse_temporal_hint(std::string_view query);

} // namespace lazylook
