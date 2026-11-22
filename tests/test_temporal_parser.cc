#include "temporal_parser.hh"

#include <cassert>
#include <chrono>
#include <iostream>
#include <string>

namespace
{

#define TEST_ASSERT(cond, msg)                                                                     \
  do {                                                                                             \
    if (!(cond)) {                                                                                 \
      std::cerr << "Assertion failed: " << (msg) << " at " << __FILE__ << ":" << __LINE__          \
                << std::endl;                                                                      \
      std::exit(1);                                                                                \
    }                                                                                              \
  } while (false)

int64_t now_secs()
{
  return std::chrono::duration_cast<std::chrono::seconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

constexpr int64_t kSecsPerDay = 86400;
constexpr int64_t kSecsPerWeek = 7 * kSecsPerDay;
constexpr int64_t kSecsPerMonth = 30 * kSecsPerDay;
constexpr int64_t kSecsPerYear = 365 * kSecsPerDay;

// Tolerance: allow a 5-second window between expected and actual cutoff (the
// test and production code call now_secs() independently, milliseconds apart).
constexpr int64_t kToleranceSecs = 5;

bool approx_eq(int64_t a, int64_t b, int64_t tol = kToleranceSecs)
{
  const int64_t diff = a - b;
  return diff >= -tol && diff <= tol;
}

} // namespace

void test_no_temporal()
{
  std::cout << "[Test] No temporal hint in query..." << std::endl;
  const auto res = lazylook::parse_temporal_hint("project report");
  TEST_ASSERT(!res.has_value(), "Plain query should return nullopt");

  const auto res2 = lazylook::parse_temporal_hint("");
  TEST_ASSERT(!res2.has_value(), "Empty query should return nullopt");

  const auto res3 = lazylook::parse_temporal_hint("code: calculate_hash");
  TEST_ASSERT(!res3.has_value(), "Type prefix only should return nullopt");
  std::cout << "  ✓ No temporal hint tests passed." << std::endl;
}

void test_today()
{
  std::cout << "[Test] 'today' hint..." << std::endl;
  const auto res = lazylook::parse_temporal_hint("budget PDF today");
  TEST_ASSERT(res.has_value(), "Should detect 'today'");
  const int64_t expected = now_secs() - kSecsPerDay;
  TEST_ASSERT(approx_eq(res->cutoff_secs, expected), "Cutoff should be ~24h ago for 'today'");
  TEST_ASSERT(res->cleaned_query == "budget PDF", "Should strip 'today' from query");
  std::cout << "  ✓ Today tests passed." << std::endl;
}

void test_yesterday()
{
  std::cout << "[Test] 'yesterday' hint..." << std::endl;
  const auto res = lazylook::parse_temporal_hint("invoice yesterday");
  TEST_ASSERT(res.has_value(), "Should detect 'yesterday'");
  const int64_t expected = now_secs() - 2 * kSecsPerDay;
  TEST_ASSERT(approx_eq(res->cutoff_secs, expected), "Cutoff should be ~48h ago");
  TEST_ASSERT(res->cleaned_query == "invoice", "Should strip 'yesterday' from query");
  std::cout << "  ✓ Yesterday tests passed." << std::endl;
}

void test_last_week()
{
  std::cout << "[Test] 'last week' hint..." << std::endl;
  const auto res = lazylook::parse_temporal_hint("meeting notes last week");
  TEST_ASSERT(res.has_value(), "Should detect 'last week'");
  const int64_t expected = now_secs() - kSecsPerWeek;
  TEST_ASSERT(approx_eq(res->cutoff_secs, expected), "Cutoff should be ~1 week ago");
  TEST_ASSERT(res->cleaned_query == "meeting notes", "Should strip 'last week'");
  std::cout << "  ✓ Last week tests passed." << std::endl;
}

void test_this_month()
{
  std::cout << "[Test] 'this month' hint..." << std::endl;
  const auto res = lazylook::parse_temporal_hint("receipts this month");
  TEST_ASSERT(res.has_value(), "Should detect 'this month'");
  const int64_t expected = now_secs() - kSecsPerMonth;
  TEST_ASSERT(approx_eq(res->cutoff_secs, expected), "Cutoff should be ~30 days ago");
  TEST_ASSERT(res->cleaned_query == "receipts", "Should strip 'this month'");
  std::cout << "  ✓ This month tests passed." << std::endl;
}

void test_last_year()
{
  std::cout << "[Test] 'last year' hint..." << std::endl;
  const auto res = lazylook::parse_temporal_hint("annual report last year");
  TEST_ASSERT(res.has_value(), "Should detect 'last year'");
  const int64_t expected = now_secs() - kSecsPerYear;
  TEST_ASSERT(approx_eq(res->cutoff_secs, expected), "Cutoff should be ~365 days ago");
  TEST_ASSERT(res->cleaned_query == "annual report", "Should strip 'last year'");
  std::cout << "  ✓ Last year tests passed." << std::endl;
}

void test_past_n_days()
{
  std::cout << "[Test] 'past N days' hint..." << std::endl;
  {
    const auto res = lazylook::parse_temporal_hint("expenses past 7 days");
    TEST_ASSERT(res.has_value(), "Should detect 'past 7 days'");
    const int64_t expected = now_secs() - 7 * kSecsPerDay;
    TEST_ASSERT(approx_eq(res->cutoff_secs, expected), "Cutoff should be ~7 days ago");
    TEST_ASSERT(res->cleaned_query == "expenses", "Should strip 'past 7 days'");
  }
  {
    const auto res = lazylook::parse_temporal_hint("last 30 days budget");
    TEST_ASSERT(res.has_value(), "Should detect 'last 30 days'");
    const int64_t expected = now_secs() - 30 * kSecsPerDay;
    TEST_ASSERT(approx_eq(res->cutoff_secs, expected), "Cutoff should be ~30 days ago");
    TEST_ASSERT(res->cleaned_query == "budget", "Should strip 'last 30 days'");
  }
  std::cout << "  ✓ Past N days tests passed." << std::endl;
}

void test_past_n_weeks()
{
  std::cout << "[Test] 'past N weeks' hint..." << std::endl;
  const auto res = lazylook::parse_temporal_hint("project docs past 2 weeks");
  TEST_ASSERT(res.has_value(), "Should detect 'past 2 weeks'");
  const int64_t expected = now_secs() - 2 * kSecsPerWeek;
  TEST_ASSERT(approx_eq(res->cutoff_secs, expected), "Cutoff should be ~2 weeks ago");
  TEST_ASSERT(res->cleaned_query == "project docs", "Should strip 'past 2 weeks'");
  std::cout << "  ✓ Past N weeks tests passed." << std::endl;
}

void test_case_insensitive()
{
  std::cout << "[Test] Case-insensitive temporal parsing..." << std::endl;
  const auto res1 = lazylook::parse_temporal_hint("invoice LAST WEEK");
  TEST_ASSERT(res1.has_value(), "Should be case-insensitive for 'LAST WEEK'");

  const auto res2 = lazylook::parse_temporal_hint("report TODAY");
  TEST_ASSERT(res2.has_value(), "Should be case-insensitive for 'TODAY'");
  std::cout << "  ✓ Case-insensitive tests passed." << std::endl;
}

void test_cleaned_query_trimmed()
{
  std::cout << "[Test] Cleaned query is trimmed..." << std::endl;
  const auto res = lazylook::parse_temporal_hint("last week");
  TEST_ASSERT(res.has_value(), "Should detect 'last week' alone");
  TEST_ASSERT(res->cleaned_query.empty() || res->cleaned_query == "",
              "Cleaned query should be empty when only temporal phrase was present");
  std::cout << "  ✓ Cleaned query trimming passed." << std::endl;
}

void test_word_boundaries_and_subwords()
{
  std::cout << "[Test] Word boundaries and subword false positives..." << std::endl;
  // Subwords should NOT trigger temporal matches
  TEST_ASSERT(!lazylook::parse_temporal_hint("todays invoices").has_value(),
              "'todays' must not match 'today'");
  TEST_ASSERT(!lazylook::parse_temporal_hint("ballast 5 days").has_value(),
              "'ballast' must not match 'last'");
  TEST_ASSERT(!lazylook::parse_temporal_hint("last 3 daylight").has_value(),
              "'daylight' must not match 'day'");
  TEST_ASSERT(!lazylook::parse_temporal_hint("last 2 weekends").has_value(),
              "'weekends' must not match 'week'");
  TEST_ASSERT(!lazylook::parse_temporal_hint("past 3 monthly").has_value(),
              "'monthly' must not match 'month'");

  // Valid word boundaries with punctuation
  auto res1 = lazylook::parse_temporal_hint("receipts (today)");
  TEST_ASSERT(res1.has_value(), "Parenthesized 'today' should match");
  TEST_ASSERT(res1->cleaned_query == "receipts ()", "Parentheses preserved");

  auto res2 = lazylook::parse_temporal_hint("expenses, last week, finance");
  TEST_ASSERT(res2.has_value(), "Comma-separated 'last week' should match");
  TEST_ASSERT(res2->cleaned_query == "expenses, , finance", "Surrounding tokens preserved");
  std::cout << "  ✓ Word boundaries and subword tests passed." << std::endl;
}

int main()
{
  std::cout << "=== TemporalParserTests ===" << std::endl;
  test_no_temporal();
  test_today();
  test_yesterday();
  test_last_week();
  test_this_month();
  test_last_year();
  test_past_n_days();
  test_past_n_weeks();
  test_case_insensitive();
  test_cleaned_query_trimmed();
  test_word_boundaries_and_subwords();
  std::cout << "=== All TemporalParserTests passed. ===" << std::endl;
  return 0;
}
