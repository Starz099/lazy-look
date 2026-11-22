#include "temporal_parser.hh"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <chrono>
#include <optional>
#include <string>
#include <string_view>

namespace lazylook
{

namespace
{

constexpr int64_t kSecsPerDay = 86400;
constexpr int64_t kSecsPerWeek = 7 * kSecsPerDay;

// Returns the current Unix epoch in seconds.
int64_t now_secs()
{
  return std::chrono::duration_cast<std::chrono::seconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

// Approximate seconds in a calendar month (30 days).
constexpr int64_t kSecsPerMonth = 30 * kSecsPerDay;

// Approximate seconds in a calendar year (365 days).
constexpr int64_t kSecsPerYear = 365 * kSecsPerDay;

std::string to_lower(std::string_view sv)
{
  std::string s(sv);
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return s;
}

bool is_word_boundary(char c)
{
  return std::isspace(static_cast<unsigned char>(c)) || std::ispunct(static_cast<unsigned char>(c));
}

// Collapses repeated whitespace and trims ends.
std::string collapse_spaces(std::string_view s)
{
  std::string res;
  res.reserve(s.size());
  bool last_space = true;
  for (char c : s) {
    if (std::isspace(static_cast<unsigned char>(c))) {
      if (!last_space && !res.empty()) {
        res.push_back(' ');
        last_space = true;
      }
    }
    else {
      res.push_back(c);
      last_space = false;
    }
  }
  while (!res.empty() && std::isspace(static_cast<unsigned char>(res.back()))) {
    res.pop_back();
  }
  return res;
}

// Removes `phrase` starting at `pos` from `source` and collapses whitespace.
std::string remove_phrase_at(std::string_view source, size_t pos, size_t len)
{
  std::string result;
  result.reserve(source.size() > len ? source.size() - len : 0);
  if (pos > 0) {
    result.append(source.substr(0, pos));
  }
  if (pos + len < source.size()) {
    result.append(source.substr(pos + len));
  }
  return collapse_spaces(result);
}

bool match_unit(std::string_view lower,
                size_t start,
                std::string_view singular,
                std::string_view plural,
                size_t &end)
{
  if (start + plural.size() <= lower.size() && lower.substr(start, plural.size()) == plural &&
      (start + plural.size() == lower.size() || is_word_boundary(lower[start + plural.size()]))) {
    end = start + plural.size();
    return true;
  }
  if (start + singular.size() <= lower.size() && lower.substr(start, singular.size()) == singular &&
      (start + singular.size() == lower.size() ||
       is_word_boundary(lower[start + singular.size()]))) {
    end = start + singular.size();
    return true;
  }
  return false;
}

// Attempts to parse "last N <unit>", "past N <unit>" where unit is
// days/weeks/months/years.  Returns the offset in seconds on success.
std::optional<std::pair<int64_t, std::string>> try_parse_n_units(std::string_view lower,
                                                                 std::string_view original)
{
  // Find "last " or "past " with word boundaries
  for (std::string_view trigger : {"last ", "past "}) {
    size_t pos = lower.find(trigger);
    while (pos != std::string_view::npos) {
      if (pos > 0 && !is_word_boundary(lower[pos - 1])) {
        pos = lower.find(trigger, pos + 1);
        continue;
      }

      size_t num_start = pos + trigger.size();
      while (num_start < lower.size() &&
             std::isspace(static_cast<unsigned char>(lower[num_start]))) {
        ++num_start;
      }
      if (num_start >= lower.size() ||
          !std::isdigit(static_cast<unsigned char>(lower[num_start]))) {
        pos = lower.find(trigger, pos + 1);
        continue;
      }
      size_t num_end = num_start;
      while (num_end < lower.size() && std::isdigit(static_cast<unsigned char>(lower[num_end]))) {
        ++num_end;
      }

      int n = 0;
      auto [ptr, ec] = std::from_chars(lower.data() + num_start, lower.data() + num_end, n);
      if (ec != std::errc() || n <= 0 || n > 9999) {
        pos = lower.find(trigger, pos + 1);
        continue;
      }

      size_t unit_start = num_end;
      while (unit_start < lower.size() &&
             std::isspace(static_cast<unsigned char>(lower[unit_start]))) {
        ++unit_start;
      }

      int64_t offset = 0;
      size_t unit_end = unit_start;

      if (match_unit(lower, unit_start, "day", "days", unit_end)) {
        offset = static_cast<int64_t>(n) * kSecsPerDay;
      }
      else if (match_unit(lower, unit_start, "week", "weeks", unit_end)) {
        offset = static_cast<int64_t>(n) * kSecsPerWeek;
      }
      else if (match_unit(lower, unit_start, "month", "months", unit_end)) {
        offset = static_cast<int64_t>(n) * kSecsPerMonth;
      }
      else if (match_unit(lower, unit_start, "year", "years", unit_end)) {
        offset = static_cast<int64_t>(n) * kSecsPerYear;
      }

      if (offset > 0) {
        const std::string cleaned = remove_phrase_at(original, pos, unit_end - pos);
        return std::make_pair(offset, cleaned);
      }
      pos = lower.find(trigger, pos + 1);
    }
  }
  return std::nullopt;
}

} // namespace

std::optional<TemporalMatch> parse_temporal_hint(std::string_view query)
{
  if (query.empty()) {
    return std::nullopt;
  }

  const std::string lower = to_lower(query);
  const int64_t now = now_secs();

  // --- Fixed phrases (longest-match first) ---

  struct FixedPhrase
  {
    std::string_view phrase;
    int64_t offset_secs;
  };

  static constexpr FixedPhrase kFixedPhrases[] = {
      {"this year", kSecsPerYear},
      {"last year", kSecsPerYear},
      {"past year", kSecsPerYear},
      {"this month", kSecsPerMonth},
      {"last month", kSecsPerMonth},
      {"past month", kSecsPerMonth},
      {"this week", kSecsPerWeek},
      {"last week", kSecsPerWeek},
      {"past week", kSecsPerWeek},
      {"yesterday", 2 * kSecsPerDay},
      {"today", kSecsPerDay},
  };

  for (const auto &fp : kFixedPhrases) {
    size_t pos = lower.find(fp.phrase);
    while (pos != std::string_view::npos) {
      const bool start_ok = (pos == 0 || is_word_boundary(lower[pos - 1]));
      const size_t end_pos = pos + fp.phrase.size();
      const bool end_ok = (end_pos == lower.size() || is_word_boundary(lower[end_pos]));

      if (start_ok && end_ok) {
        TemporalMatch m;
        m.cutoff_secs = now - fp.offset_secs;
        m.cleaned_query = remove_phrase_at(query, pos, fp.phrase.size());
        return m;
      }
      pos = lower.find(fp.phrase, pos + 1);
    }
  }

  // --- Variable phrases: "last N days/weeks/months/years" ---
  if (auto res = try_parse_n_units(lower, query)) {
    TemporalMatch m;
    m.cutoff_secs = now - res->first;
    m.cleaned_query = std::move(res->second);
    return m;
  }

  return std::nullopt;
}

} // namespace lazylook
