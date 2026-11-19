#include "extractor.hh"

#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using namespace std::string_literals;

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

std::filesystem::path create_test_directory()
{
  std::filesystem::path dir =
      std::filesystem::temp_directory_path() / ("lazyfinder_test_" + std::to_string(std::rand()));
  std::filesystem::create_directories(dir);
  return dir;
}

void write_file(const std::filesystem::path &path, std::string_view content)
{
  std::ofstream out(path, std::ios::binary);
  out.write(content.data(), static_cast<std::streamsize>(content.size()));
}

void setup_test_archives(const std::filesystem::path &dir)
{
  std::string cmd =
      "python3 -c \"\n"
      "import zipfile, os\n"
      "d = '" +
      dir.string() +
      "'\n"
      "with zipfile.ZipFile(os.path.join(d, 'sample.docx'), 'w') as z:\n"
      "    z.writestr('word/document.xml', '<w:document><w:body><w:p><w:r><w:t>Hello "
      "from Word Document!</w:t></w:r></w:p></w:body></w:document>')\n"
      "\n"
      "with zipfile.ZipFile(os.path.join(d, 'sample.xlsx'), 'w') as z:\n"
      "    z.writestr('xl/sharedStrings.xml', "
      "'<sst><si><t>Revenue</t></si><si><t>125000 USD</t></si></sst>')\n"
      "    z.writestr('xl/worksheets/sheet1.xml', "
      "'<worksheet><sheetData><row><c r=\\\"A1\\\" "
      "t=\\\"s\\\"><v>0</v></c></row></sheetData></worksheet>')\n"
      "\n"
      "with zipfile.ZipFile(os.path.join(d, 'sample.pptx'), 'w') as z:\n"
      "    z.writestr('ppt/slides/slide1.xml', "
      "'<p:spTree><a:p><a:r><a:t>Quarterly Presentation Slide "
      "1</a:t></a:r></a:p></p:spTree>')\n"
      "\n"
      "with zipfile.ZipFile(os.path.join(d, 'sample.odt'), 'w') as z:\n"
      "    z.writestr('content.xml', "
      "'<office:document-content><office:body><office:text><text:p>OpenDocument Text "
      "Sample</text:p></office:text></office:body></office:document-content>')\n"
      "\n"
      "with zipfile.ZipFile(os.path.join(d, 'sample.ods'), 'w') as z:\n"
      "    z.writestr('content.xml', "
      "'<office:document-content><office:body><office:spreadsheet><table:table-cell><"
      "text:p>Spreadsheet Cell Value</text:p></table:table-cell></office:spreadsheet>"
      "</office:body></office:document-content>')\n"
      "\n"
      "with zipfile.ZipFile(os.path.join(d, 'sample.odp'), 'w') as z:\n"
      "    z.writestr('content.xml', "
      "'<office:document-content><office:body><office:presentation><text:p>Presentati"
      "on Slide Deck</text:p></office:presentation></office:body></office:document-"
      "content>')\n"
      "\n"
      "with zipfile.ZipFile(os.path.join(d, 'sample.epub'), 'w') as z:\n"
      "    z.writestr('EPUB/ch01.xhtml', '<html><body><h1>Chapter 1</h1><p>Electronic "
      "publication chapter text.</p></body></html>')\n"
      "\n"
      "with zipfile.ZipFile(os.path.join(d, 'special [name] & spaces.docx'), 'w') as "
      "z:\n"
      "    z.writestr('word/document.xml', "
      "'<w:document><w:body><w:p><w:r><w:t>Content in escaped "
      "filename.</w:t></w:r></w:p></w:body></w:document>')\n"
      "\"";
  int ret = std::system(cmd.c_str());
  (void)ret;
}

} // namespace

void test_is_supported()
{
  std::cout << "[Test] is_supported format coverage..." << std::endl;

  // Text & Markdown
  TEST_ASSERT(lazyfinder::Extractor::is_supported("notes.txt"), ".txt supported");
  TEST_ASSERT(lazyfinder::Extractor::is_supported("README.MD"), ".MD uppercase supported");
  TEST_ASSERT(lazyfinder::Extractor::is_supported("doc.markdown"), ".markdown supported");
  TEST_ASSERT(lazyfinder::Extractor::is_supported("index.rst"), ".rst supported");
  TEST_ASSERT(lazyfinder::Extractor::is_supported("notes.org"), ".org supported");
  TEST_ASSERT(lazyfinder::Extractor::is_supported("data.csv"), ".csv supported");
  TEST_ASSERT(lazyfinder::Extractor::is_supported("data.tsv"), ".tsv supported");
  TEST_ASSERT(lazyfinder::Extractor::is_supported("server.log"), ".log supported");

  // Code formats
  TEST_ASSERT(lazyfinder::Extractor::is_supported("main.cc"), ".cc supported");
  TEST_ASSERT(lazyfinder::Extractor::is_supported("main.cpp"), ".cpp supported");
  TEST_ASSERT(lazyfinder::Extractor::is_supported("header.hh"), ".hh supported");
  TEST_ASSERT(lazyfinder::Extractor::is_supported("app.rs"), ".rs supported");
  TEST_ASSERT(lazyfinder::Extractor::is_supported("script.py"), ".py supported");
  TEST_ASSERT(lazyfinder::Extractor::is_supported("app.js"), ".js supported");
  TEST_ASSERT(lazyfinder::Extractor::is_supported("app.ts"), ".ts supported");
  TEST_ASSERT(lazyfinder::Extractor::is_supported("page.jsx"), ".jsx supported");
  TEST_ASSERT(lazyfinder::Extractor::is_supported("page.tsx"), ".tsx supported");
  TEST_ASSERT(lazyfinder::Extractor::is_supported("main.go"), ".go supported");
  TEST_ASSERT(lazyfinder::Extractor::is_supported("App.java"), ".java supported");
  TEST_ASSERT(lazyfinder::Extractor::is_supported("build.sh"), ".sh supported");
  TEST_ASSERT(lazyfinder::Extractor::is_supported("config.json"), ".json supported");
  TEST_ASSERT(lazyfinder::Extractor::is_supported("deploy.yaml"), ".yaml supported");
  TEST_ASSERT(lazyfinder::Extractor::is_supported("deploy.yml"), ".yml supported");
  TEST_ASSERT(lazyfinder::Extractor::is_supported("Cargo.toml"), ".toml supported");
  TEST_ASSERT(lazyfinder::Extractor::is_supported("schema.sql"), ".sql supported");
  TEST_ASSERT(lazyfinder::Extractor::is_supported("CMakeLists.cmake"), ".cmake supported");

  // Document & Office formats
  TEST_ASSERT(lazyfinder::Extractor::is_supported("document.pdf"), ".pdf supported");
  TEST_ASSERT(lazyfinder::Extractor::is_supported("REPORT.PDF"), ".PDF uppercase supported");
  TEST_ASSERT(lazyfinder::Extractor::is_supported("memo.docx"), ".docx supported");
  TEST_ASSERT(lazyfinder::Extractor::is_supported("MEMO.DOCX"), ".DOCX uppercase supported");
  TEST_ASSERT(lazyfinder::Extractor::is_supported("budget.xlsx"), ".xlsx supported");
  TEST_ASSERT(lazyfinder::Extractor::is_supported("deck.pptx"), ".pptx supported");
  TEST_ASSERT(lazyfinder::Extractor::is_supported("document.odt"), ".odt supported");
  TEST_ASSERT(lazyfinder::Extractor::is_supported("calc.ods"), ".ods supported");
  TEST_ASSERT(lazyfinder::Extractor::is_supported("slides.odp"), ".odp supported");
  TEST_ASSERT(lazyfinder::Extractor::is_supported("book.epub"), ".epub supported");
  TEST_ASSERT(lazyfinder::Extractor::is_supported("legacy.rtf"), ".rtf supported");
  TEST_ASSERT(lazyfinder::Extractor::is_supported("index.html"), ".html supported");
  TEST_ASSERT(lazyfinder::Extractor::is_supported("index.htm"), ".htm supported");

  // Subtitles & Media formats
  TEST_ASSERT(lazyfinder::Extractor::is_supported("captions.srt"), ".srt supported");
  TEST_ASSERT(lazyfinder::Extractor::is_supported("CAPTIONS.SRT"), ".SRT uppercase supported");
  TEST_ASSERT(lazyfinder::Extractor::is_supported("subtitles.vtt"), ".vtt supported");
  TEST_ASSERT(lazyfinder::Extractor::is_supported("video.mp4"), ".mp4 supported");
  TEST_ASSERT(lazyfinder::Extractor::is_supported("movie.mkv"), ".mkv supported");
  TEST_ASSERT(lazyfinder::Extractor::is_supported("audio.mp3"), ".mp3 supported");

  // Unsupported files
  TEST_ASSERT(!lazyfinder::Extractor::is_supported("binary.exe"), ".exe rejected");
  TEST_ASSERT(!lazyfinder::Extractor::is_supported("firmware.bin"), ".bin rejected");
  TEST_ASSERT(!lazyfinder::Extractor::is_supported("archive.zip"), ".zip rejected");
  TEST_ASSERT(!lazyfinder::Extractor::is_supported("disk.iso"), ".iso rejected");
  TEST_ASSERT(!lazyfinder::Extractor::is_supported("image.png"), ".png rejected");
  TEST_ASSERT(!lazyfinder::Extractor::is_supported("photo.jpg"), ".jpg rejected");

  std::cout << "  ✓ All format extensions verified." << std::endl;
}

void test_text_and_code(const std::filesystem::path &dir)
{
  std::cout << "[Test] Plain text and code extraction..." << std::endl;

  const auto text_path = dir / "sample.txt";
  write_file(text_path, "First line of text.\nSecond line with info.\nThird line.");

  auto chunks = lazyfinder::Extractor::extract_chunks(text_path);
  TEST_ASSERT(!chunks.empty(), "Text file extracted chunks");
  TEST_ASSERT(chunks[0].find("sample.txt") != std::string::npos, "Chunk has filename header");
  TEST_ASSERT(chunks[0].find("First line of text.") != std::string::npos, "Chunk has content");

  const auto code_path = dir / "sample.rs";
  write_file(code_path,
             "fn calculate_hash(data: &[u8]) -> u64 {\n    let mut hash = 0;\n    hash\n}");
  auto code_chunks = lazyfinder::Extractor::extract_chunks(code_path);
  TEST_ASSERT(!code_chunks.empty(), "Code file extracted chunks");
  TEST_ASSERT(code_chunks[0].find("calculate_hash") != std::string::npos, "Code content preserved");

  std::cout << "  ✓ Plain text and code extraction passed." << std::endl;
}

void test_html_extraction(const std::filesystem::path &dir)
{
  std::cout << "[Test] HTML parsing and entity decoding..." << std::endl;

  const auto html_path = dir / "page.html";
  write_file(html_path,
             "<!DOCTYPE html><html><head><title>Web Page</title>"
             "<style>body { color: red; }</style>"
             "<script>function bad() { alert('xss'); }</script>"
             "</head><body>"
             "<h1>Article Title</h1>"
             "<p>Paragraph with &amp; &lt;brackets&gt; and &quot;quotes&quot; and &nbsp; "
             "spaces.</p>"
             "</body></html>");

  auto chunks = lazyfinder::Extractor::extract_chunks(html_path);
  TEST_ASSERT(!chunks.empty(), "HTML extracted chunks");
  const auto &content = chunks[0];

  TEST_ASSERT(content.find("alert('xss')") == std::string::npos, "Script tag stripped");
  TEST_ASSERT(content.find("color: red") == std::string::npos, "Style tag stripped");
  TEST_ASSERT(content.find("Article Title") != std::string::npos, "Title text extracted");
  TEST_ASSERT(content.find("&") != std::string::npos, "&amp; entity decoded");
  TEST_ASSERT(content.find("<brackets>") != std::string::npos, "<brackets> decoded");
  TEST_ASSERT(content.find("\"quotes\"") != std::string::npos, "\"quotes\" decoded");

  std::cout << "  ✓ HTML extraction and sanitization passed." << std::endl;
}

void test_rtf_extraction(const std::filesystem::path &dir)
{
  std::cout << "[Test] RTF parsing..." << std::endl;

  const auto rtf_path = dir / "sample.rtf";
  write_file(rtf_path,
             "{\\rtf1\\ansi\\deff0 {\\fonttbl {\\f0 Arial;}} "
             "\\f0\\fs24 \\b Executive Summary \\b0 \\par "
             "This report describes the \\i quarterly \\i0 results for 2026.}");

  auto chunks = lazyfinder::Extractor::extract_chunks(rtf_path);
  TEST_ASSERT(!chunks.empty(), "RTF extracted chunks");
  const auto &content = chunks[0];

  TEST_ASSERT(content.find("Executive Summary") != std::string::npos, "Bold header extracted");
  TEST_ASSERT(content.find("quarterly results for 2026") != std::string::npos,
              "Body text extracted");
  TEST_ASSERT(content.find("\\fonttbl") == std::string::npos, "Font table metadata stripped");
  TEST_ASSERT(content.find("\\b0") == std::string::npos, "Control tags stripped");

  std::cout << "  ✓ RTF extraction passed." << std::endl;
}

void test_office_formats(const std::filesystem::path &dir)
{
  std::cout << "[Test] OpenXML and OpenDocument formats..." << std::endl;

  // DOCX
  auto docx_chunks = lazyfinder::Extractor::extract_chunks(dir / "sample.docx");
  TEST_ASSERT(!docx_chunks.empty(), "DOCX extracted chunks");
  TEST_ASSERT(docx_chunks[0].find("Hello from Word Document!") != std::string::npos,
              "DOCX text matched");

  // XLSX
  auto xlsx_chunks = lazyfinder::Extractor::extract_chunks(dir / "sample.xlsx");
  TEST_ASSERT(!xlsx_chunks.empty(), "XLSX extracted chunks");
  TEST_ASSERT(xlsx_chunks[0].find("Revenue") != std::string::npos, "XLSX text matched");
  TEST_ASSERT(xlsx_chunks[0].find("125000 USD") != std::string::npos, "XLSX cell value matched");

  // PPTX
  auto pptx_chunks = lazyfinder::Extractor::extract_chunks(dir / "sample.pptx");
  TEST_ASSERT(!pptx_chunks.empty(), "PPTX extracted chunks");
  TEST_ASSERT(pptx_chunks[0].find("Quarterly Presentation Slide 1") != std::string::npos,
              "PPTX slide matched");

  // ODT
  auto odt_chunks = lazyfinder::Extractor::extract_chunks(dir / "sample.odt");
  TEST_ASSERT(!odt_chunks.empty(), "ODT extracted chunks");
  TEST_ASSERT(odt_chunks[0].find("OpenDocument Text Sample") != std::string::npos,
              "ODT text matched");

  // ODS
  auto ods_chunks = lazyfinder::Extractor::extract_chunks(dir / "sample.ods");
  TEST_ASSERT(!ods_chunks.empty(), "ODS extracted chunks");
  TEST_ASSERT(ods_chunks[0].find("Spreadsheet Cell Value") != std::string::npos,
              "ODS text matched");

  // ODP
  auto odp_chunks = lazyfinder::Extractor::extract_chunks(dir / "sample.odp");
  TEST_ASSERT(!odp_chunks.empty(), "ODP extracted chunks");
  TEST_ASSERT(odp_chunks[0].find("Presentation Slide Deck") != std::string::npos,
              "ODP text matched");

  // EPUB
  auto epub_chunks = lazyfinder::Extractor::extract_chunks(dir / "sample.epub");
  TEST_ASSERT(!epub_chunks.empty(), "EPUB extracted chunks");
  TEST_ASSERT(epub_chunks[0].find("Electronic publication chapter text.") != std::string::npos,
              "EPUB chapter text matched");

  std::cout << "  ✓ All office and archive formats passed." << std::endl;
}

void test_subtitles_and_media(const std::filesystem::path &dir)
{
  std::cout << "[Test] Subtitle and media companion extraction..." << std::endl;

  // 1. SRT subtitle file
  const std::string srt_text = "1\n"
                               "00:01:23,500 --> 00:01:28,000\n"
                               "Discussion on database indexing and retrieval.\n"
                               "\n"
                               "2\n"
                               "00:02:15,000 --> 00:02:20,000\n"
                               "PostgreSQL configuration and vector performance.\n";
  const auto srt_path = dir / "lecture.srt";
  write_file(srt_path, srt_text);

  auto srt_chunks = lazyfinder::Extractor::extract_chunks(srt_path);
  TEST_ASSERT(!srt_chunks.empty(), "SRT chunks extracted");
  TEST_ASSERT(srt_chunks[0].find("[01:23]") != std::string::npos, "SRT contains [01:23] timestamp");
  TEST_ASSERT(srt_chunks[0].find("database indexing") != std::string::npos,
              "SRT contains cue text");

  // 2. WebVTT subtitle file
  const std::string vtt_text = "WEBVTT\n"
                               "\n"
                               "00:00:45.000 --> 00:00:50.000\n"
                               "Introduction to machine learning pipelines.\n";
  const auto vtt_path = dir / "intro.vtt";
  write_file(vtt_path, vtt_text);

  auto vtt_chunks = lazyfinder::Extractor::extract_chunks(vtt_path);
  TEST_ASSERT(!vtt_chunks.empty(), "VTT chunks extracted");
  TEST_ASSERT(vtt_chunks[0].find("[00:45]") != std::string::npos, "VTT contains [00:45] timestamp");

  // 3. Media companion: lecture.mp4 alongside lecture.srt
  const auto mp4_path = dir / "lecture.mp4";
  write_file(mp4_path, "dummy media content");

  auto mp4_chunks = lazyfinder::Extractor::extract_chunks(mp4_path);
  TEST_ASSERT(!mp4_chunks.empty(), "MP4 extracted via companion SRT");
  TEST_ASSERT(mp4_chunks[0].find("lecture.mp4") != std::string::npos,
              "Header contains MP4 filename");
  TEST_ASSERT(mp4_chunks[0].find("[01:23]") != std::string::npos,
              "MP4 chunk has companion timestamp");

  std::cout << "  ✓ Subtitle and media companion extraction passed." << std::endl;
}

void test_edge_cases(const std::filesystem::path &dir)
{
  std::cout << "[Test] Edge cases and safety..." << std::endl;

  // 1. Non-existent file
  auto missing_chunks = lazyfinder::Extractor::extract_chunks(dir / "non_existent.txt");
  TEST_ASSERT(missing_chunks.empty(), "Non-existent file returns empty safely");

  // 2. 0-byte file
  const auto empty_path = dir / "empty.txt";
  write_file(empty_path, "");
  auto empty_chunks = lazyfinder::Extractor::extract_chunks(empty_path);
  TEST_ASSERT(empty_chunks.empty(), "0-byte file returns empty safely");

  // 3. File path with spaces, symbols, and brackets
  auto special_chunks = lazyfinder::Extractor::extract_chunks(dir / "special [name] & spaces.docx");
  TEST_ASSERT(!special_chunks.empty(), "Path with spaces and symbols extracts properly");
  TEST_ASSERT(special_chunks[0].find("Content in escaped filename.") != std::string::npos,
              "Escaped path content extracted");

  // 4. Binary unreadable file fallback
  const auto bin_path = dir / "mystery.txt";
  std::string binary_data = "header\0\1\2\3binary_data"s;
  write_file(bin_path, binary_data);
  auto bin_chunks = lazyfinder::Extractor::extract_chunks(bin_path);
  TEST_ASSERT(!bin_chunks.empty(), "Binary file with supported ext has fallback");
  TEST_ASSERT(bin_chunks[0] == "mystery.txt", "Binary file falls back to filename for indexing");

  // 5. Oversized file (> 20 MB)
  const auto oversized_path = dir / "large.txt";
  {
    std::ofstream out(oversized_path, std::ios::binary);
    // Seek to 21 MB and write one byte to create a sparse file
    out.seekp(21 * 1024 * 1024);
    out.write("X", 1);
  }
  auto oversized_chunks = lazyfinder::Extractor::extract_chunks(oversized_path);
  TEST_ASSERT(oversized_chunks.empty(), "Oversized file rejected before reading");

  std::cout << "  ✓ Edge cases and safety passed." << std::endl;
}

int main()
{
  std::cout << "========================================" << std::endl;
  std::cout << "LazyFinder Extractor & Multi-Format Test" << std::endl;
  std::cout << "========================================" << std::endl;

  test_is_supported();

  const auto temp_dir = create_test_directory();
  setup_test_archives(temp_dir);

  test_text_and_code(temp_dir);
  test_html_extraction(temp_dir);
  test_rtf_extraction(temp_dir);
  test_office_formats(temp_dir);
  test_subtitles_and_media(temp_dir);
  test_edge_cases(temp_dir);

  // Clean up
  std::error_code ec;
  std::filesystem::remove_all(temp_dir, ec);

  std::cout << "========================================" << std::endl;
  std::cout << "ALL EXTRACTOR TESTS PASSED SUCCESSFULLY!" << std::endl;
  std::cout << "========================================" << std::endl;

  return 0;
}
