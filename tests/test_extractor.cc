#include "extractor.hh"
#include "pipeline/document_handler.hh"
#include "pipeline/image_handler.hh"
#include "pipeline/media_handler.hh"
#include "pipeline/registry.hh"

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
      std::filesystem::temp_directory_path() / ("lazylook_test_" + std::to_string(std::rand()));
  std::filesystem::create_directories(dir);
  return dir;
}

void write_file(const std::filesystem::path &path, std::string_view content)
{
  std::ofstream out(path, std::ios::binary);
  out.write(content.data(), static_cast<std::streamsize>(content.size()));
}

std::string shell_escape(std::string_view arg)
{
  std::string escaped = "'";
  for (char c : arg) {
    if (c == '\'') {
      escaped += "'\\''";
    }
    else {
      escaped.push_back(c);
    }
  }
  escaped += "'";
  return escaped;
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
      "'<office:document-content><office:body><office:text><text:p>OpenDocument "
      "Text Body</text:p></office:text></office:body></office:document-content>')\n"
      "\n"
      "with zipfile.ZipFile(os.path.join(d, 'sample.ods'), 'w') as z:\n"
      "    z.writestr('content.xml', "
      "'<office:document-content><office:body><office:spreadsheet><table:table-cell><text:p>"
      "Spreadsheet "
      "Data "
      "Cell</text:p></table:table-cell></office:spreadsheet></office:body></"
      "office:document-content>')\n"
      "\n"
      "with zipfile.ZipFile(os.path.join(d, 'sample.odp'), 'w') as z:\n"
      "    z.writestr('content.xml', "
      "'<office:document-content><office:body><office:presentation><draw:page><text:p>Presentation "
      "Slide "
      "1</text:p></draw:page></office:presentation></office:body></office:document-content>')\n"
      "\n"
      "with zipfile.ZipFile(os.path.join(d, 'sample.epub'), 'w') as z:\n"
      "    z.writestr('OEBPS/chapter1.xhtml', '<?xml version=\\\"1.0\\\" "
      "encoding=\\\"utf-8\\\"?><html><body><h1>Chapter "
      "One</h1><p>The story begins in an epub.</p></body></html>')\n"
      "    z.writestr('OEBPS/chapter2.html', '<html><body><h2>Chapter Two</h2><p>Continuation of "
      "epub.</p></body></html>')\n"
      "\n"
      "with zipfile.ZipFile(os.path.join(d, 'special [name] & spaces.docx'), 'w') as z:\n"
      "    z.writestr('word/document.xml', '<w:document><w:body><w:p><w:r><w:t>Content in escaped "
      "filename.</w:t></w:r></w:p></w:body></w:document>')\n"
      "\" 2>/dev/null";

  int ret = std::system(cmd.c_str());
  (void)ret;
}

void test_is_supported()
{
  std::cout << "[Test] is_supported format coverage..." << std::endl;

  // Text & Markdown formats
  TEST_ASSERT(lazylook::Extractor::is_supported("notes.txt"), ".txt supported");
  TEST_ASSERT(lazylook::Extractor::is_supported("README.MD"), ".MD uppercase supported");
  TEST_ASSERT(lazylook::Extractor::is_supported("doc.markdown"), ".markdown supported");
  TEST_ASSERT(lazylook::Extractor::is_supported("index.rst"), ".rst supported");
  TEST_ASSERT(lazylook::Extractor::is_supported("notes.org"), ".org supported");
  TEST_ASSERT(lazylook::Extractor::is_supported("data.csv"), ".csv supported");
  TEST_ASSERT(lazylook::Extractor::is_supported("data.tsv"), ".tsv supported");
  TEST_ASSERT(lazylook::Extractor::is_supported("server.log"), ".log supported");

  // Code formats
  TEST_ASSERT(lazylook::Extractor::is_supported("main.cc"), ".cc supported");
  TEST_ASSERT(lazylook::Extractor::is_supported("main.cpp"), ".cpp supported");
  TEST_ASSERT(lazylook::Extractor::is_supported("header.hh"), ".hh supported");
  TEST_ASSERT(lazylook::Extractor::is_supported("app.rs"), ".rs supported");
  TEST_ASSERT(lazylook::Extractor::is_supported("script.py"), ".py supported");
  TEST_ASSERT(lazylook::Extractor::is_supported("app.js"), ".js supported");
  TEST_ASSERT(lazylook::Extractor::is_supported("app.ts"), ".ts supported");
  TEST_ASSERT(lazylook::Extractor::is_supported("page.jsx"), ".jsx supported");
  TEST_ASSERT(lazylook::Extractor::is_supported("page.tsx"), ".tsx supported");
  TEST_ASSERT(lazylook::Extractor::is_supported("main.go"), ".go supported");
  TEST_ASSERT(lazylook::Extractor::is_supported("App.java"), ".java supported");
  TEST_ASSERT(lazylook::Extractor::is_supported("build.sh"), ".sh supported");
  TEST_ASSERT(lazylook::Extractor::is_supported("config.json"), ".json supported");
  TEST_ASSERT(lazylook::Extractor::is_supported("deploy.yaml"), ".yaml supported");
  TEST_ASSERT(lazylook::Extractor::is_supported("deploy.yml"), ".yml supported");
  TEST_ASSERT(lazylook::Extractor::is_supported("Cargo.toml"), ".toml supported");
  TEST_ASSERT(lazylook::Extractor::is_supported("schema.sql"), ".sql supported");
  TEST_ASSERT(lazylook::Extractor::is_supported("CMakeLists.cmake"), ".cmake supported");

  // Office & Rich Document formats
  TEST_ASSERT(lazylook::Extractor::is_supported("document.pdf"), ".pdf supported");
  TEST_ASSERT(lazylook::Extractor::is_supported("REPORT.PDF"), ".PDF uppercase supported");
  TEST_ASSERT(lazylook::Extractor::is_supported("memo.docx"), ".docx supported");
  TEST_ASSERT(lazylook::Extractor::is_supported("MEMO.DOCX"), ".DOCX uppercase supported");
  TEST_ASSERT(lazylook::Extractor::is_supported("budget.xlsx"), ".xlsx supported");
  TEST_ASSERT(lazylook::Extractor::is_supported("deck.pptx"), ".pptx supported");
  TEST_ASSERT(lazylook::Extractor::is_supported("document.odt"), ".odt supported");
  TEST_ASSERT(lazylook::Extractor::is_supported("calc.ods"), ".ods supported");
  TEST_ASSERT(lazylook::Extractor::is_supported("slides.odp"), ".odp supported");
  TEST_ASSERT(lazylook::Extractor::is_supported("book.epub"), ".epub supported");
  TEST_ASSERT(lazylook::Extractor::is_supported("legacy.rtf"), ".rtf supported");
  TEST_ASSERT(lazylook::Extractor::is_supported("index.html"), ".html supported");
  TEST_ASSERT(lazylook::Extractor::is_supported("index.htm"), ".htm supported");

  // Subtitles & Media formats
  TEST_ASSERT(lazylook::Extractor::is_supported("captions.srt"), ".srt supported");
  TEST_ASSERT(lazylook::Extractor::is_supported("CAPTIONS.SRT"), ".SRT uppercase supported");
  TEST_ASSERT(lazylook::Extractor::is_supported("subtitles.vtt"), ".vtt supported");
  TEST_ASSERT(lazylook::Extractor::is_supported("video.mp4"), ".mp4 supported");
  TEST_ASSERT(lazylook::Extractor::is_supported("movie.mkv"), ".mkv supported");
  TEST_ASSERT(lazylook::Extractor::is_supported("audio.mp3"), ".mp3 supported");

  // Images & Vision formats
  TEST_ASSERT(lazylook::Extractor::is_supported("image.png"), ".png supported");
  TEST_ASSERT(lazylook::Extractor::is_supported("IMAGE.PNG"), ".PNG uppercase supported");
  TEST_ASSERT(lazylook::Extractor::is_supported("photo.jpg"), ".jpg supported");
  TEST_ASSERT(lazylook::Extractor::is_supported("photo.jpeg"), ".jpeg supported");
  TEST_ASSERT(lazylook::Extractor::is_supported("graphic.webp"), ".webp supported");
  TEST_ASSERT(lazylook::Extractor::is_supported("diagram.svg"), ".svg supported");
  TEST_ASSERT(lazylook::Extractor::is_supported("bitmap.bmp"), ".bmp supported");

  // Unsupported files
  TEST_ASSERT(!lazylook::Extractor::is_supported("binary.exe"), ".exe rejected");
  TEST_ASSERT(!lazylook::Extractor::is_supported("firmware.bin"), ".bin rejected");
  TEST_ASSERT(!lazylook::Extractor::is_supported("archive.zip"), ".zip rejected");
  TEST_ASSERT(!lazylook::Extractor::is_supported("disk.iso"), ".iso rejected");

  std::cout << "  ✓ All format extensions verified." << std::endl;
}

void test_text_and_code(const std::filesystem::path &dir)
{
  std::cout << "[Test] Plain text and code extraction..." << std::endl;

  const auto text_path = dir / "sample.txt";
  write_file(text_path, "First line of text.\nSecond line with info.\nThird line.");

  auto chunks = lazylook::Extractor::extract_chunks(text_path);
  TEST_ASSERT(!chunks.empty(), "Text chunks non-empty");
  TEST_ASSERT(chunks[0].find("sample.txt") != std::string::npos, "Chunk has filename header");
  TEST_ASSERT(chunks[0].find("Second line with info.") != std::string::npos,
              "Chunk has line content");

  const auto code_path = dir / "algo.rs";
  write_file(code_path,
             "pub fn binary_search(arr: &[i32], target: i32) -> Option<usize> {\n"
             "    let mut low = 0;\n"
             "    let mut high = arr.len();\n"
             "    while low < high {\n"
             "        let mid = (low + high) / 2;\n"
             "        if arr[mid] == target { return Some(mid); }\n"
             "        if arr[mid] < target { low = mid + 1; } else { high = mid; }\n"
             "    }\n"
             "    None\n"
             "}\n");

  auto code_chunks = lazylook::Extractor::extract_chunks(code_path);
  TEST_ASSERT(!code_chunks.empty(), "Code chunks non-empty");
  TEST_ASSERT(code_chunks[0].find("binary_search") != std::string::npos, "Code function extracted");

  std::cout << "  ✓ Plain text and code extraction passed." << std::endl;
}

void test_html_extraction(const std::filesystem::path &dir)
{
  std::cout << "[Test] HTML clean text extraction..." << std::endl;

  const auto html_path = dir / "page.html";
  write_file(html_path,
             "<!DOCTYPE html><html><head><title>Test Page</title><style>.hidden { display: none; "
             "}</style></head>"
             "<body><h1>Header Title</h1><script>console.log('ignore');</script>"
             "<p>This is <b>important</b> content &amp; entities like &quot;quotes&quot; and "
             "&lt;tag&gt;.</p>"
             "</body></html>");

  auto chunks = lazylook::Extractor::extract_chunks(html_path);
  TEST_ASSERT(!chunks.empty(), "HTML chunks non-empty");
  TEST_ASSERT(chunks[0].find("Header Title") != std::string::npos, "Header text extracted");
  TEST_ASSERT(chunks[0].find("important content & entities like \"quotes\" and <tag>.") !=
                  std::string::npos,
              "Entities decoded properly");
  TEST_ASSERT(chunks[0].find("console.log") == std::string::npos, "Script tag content stripped");
  TEST_ASSERT(chunks[0].find(".hidden") == std::string::npos, "Style tag content stripped");

  std::cout << "  ✓ HTML extraction passed." << std::endl;
}

void test_rtf_extraction(const std::filesystem::path &dir)
{
  std::cout << "[Test] RTF extraction..." << std::endl;

  const auto rtf_path = dir / "sample.rtf";
  write_file(
      rtf_path,
      "{\\rtf1\\ansi\\deff0{\\fonttbl{\\f0\\fnil\\fcharset0 Arial;}}\n"
      "{\\colortbl ;\\red0\\green0\\blue0;}\n"
      "\\viewkind4\\uc1\\pard\\lang1033\\f0\\fs20 Hello from \\b Rich Text \\b0 Format!\\par\n"
      "Second line of RTF document.\\par\n"
      "}");

  auto chunks = lazylook::Extractor::extract_chunks(rtf_path);
  TEST_ASSERT(!chunks.empty(), "RTF chunks non-empty");
  TEST_ASSERT(chunks[0].find("Hello from Rich Text Format!") != std::string::npos,
              "RTF formatted text extracted");
  TEST_ASSERT(chunks[0].find("Second line of RTF document.") != std::string::npos,
              "RTF second paragraph extracted");
  TEST_ASSERT(chunks[0].find("colortbl") == std::string::npos, "RTF headers stripped");

  std::cout << "  ✓ RTF extraction passed." << std::endl;
}

void test_office_formats(const std::filesystem::path &dir)
{
  std::cout << "[Test] Office formats extraction (DOCX, XLSX, PPTX, ODF, EPUB)..." << std::endl;

  auto docx_chunks = lazylook::Extractor::extract_chunks(dir / "sample.docx");
  TEST_ASSERT(!docx_chunks.empty(), "DOCX chunks non-empty");
  TEST_ASSERT(docx_chunks[0].find("Hello from Word Document!") != std::string::npos,
              "DOCX text extracted");

  auto xlsx_chunks = lazylook::Extractor::extract_chunks(dir / "sample.xlsx");
  TEST_ASSERT(!xlsx_chunks.empty(), "XLSX chunks non-empty");
  TEST_ASSERT(xlsx_chunks[0].find("Revenue") != std::string::npos, "XLSX cell text extracted");
  TEST_ASSERT(xlsx_chunks[0].find("125000 USD") != std::string::npos, "XLSX cell value extracted");

  auto pptx_chunks = lazylook::Extractor::extract_chunks(dir / "sample.pptx");
  TEST_ASSERT(!pptx_chunks.empty(), "PPTX chunks non-empty");
  TEST_ASSERT(pptx_chunks[0].find("Quarterly Presentation Slide 1") != std::string::npos,
              "PPTX slide text extracted");

  auto odt_chunks = lazylook::Extractor::extract_chunks(dir / "sample.odt");
  TEST_ASSERT(!odt_chunks.empty(), "ODT chunks non-empty");
  TEST_ASSERT(odt_chunks[0].find("OpenDocument Text Body") != std::string::npos,
              "ODT text extracted");

  auto ods_chunks = lazylook::Extractor::extract_chunks(dir / "sample.ods");
  TEST_ASSERT(!ods_chunks.empty(), "ODS chunks non-empty");
  TEST_ASSERT(ods_chunks[0].find("Spreadsheet Data Cell") != std::string::npos,
              "ODS text extracted");

  auto odp_chunks = lazylook::Extractor::extract_chunks(dir / "sample.odp");
  TEST_ASSERT(!odp_chunks.empty(), "ODP chunks non-empty");
  TEST_ASSERT(odp_chunks[0].find("Presentation Slide 1") != std::string::npos,
              "ODP text extracted");

  auto epub_chunks = lazylook::Extractor::extract_chunks(dir / "sample.epub");
  TEST_ASSERT(!epub_chunks.empty(), "EPUB chunks non-empty");
  TEST_ASSERT(epub_chunks[0].find("The story begins in an epub.") != std::string::npos,
              "EPUB chapter text extracted");

  std::cout << "  ✓ All Office formats extracted successfully." << std::endl;
}

void test_subtitles_and_media(const std::filesystem::path &dir)
{
  std::cout << "[Test] Subtitle and media companion extraction..." << std::endl;

  // Standalone SRT subtitle file
  const auto srt_path = dir / "lecture.srt";
  write_file(srt_path,
             "1\n"
             "00:01:23,456 --> 00:01:28,789\n"
             "In this section we discuss vector similarity and search.\n\n"
             "2\n"
             "00:02:10,000 --> 00:02:15,000\n"
             "Reciprocal rank fusion combines multiple ranking models.\n");

  auto srt_chunks = lazylook::Extractor::extract_chunks(srt_path);
  TEST_ASSERT(!srt_chunks.empty(), "SRT chunks non-empty");
  TEST_ASSERT(srt_chunks[0].find("[01:23]") != std::string::npos,
              "SRT timestamp badge generated (01:23)");
  TEST_ASSERT(srt_chunks[0].find("vector similarity and search") != std::string::npos,
              "SRT cue text extracted");
  TEST_ASSERT(srt_chunks[0].find("[02:10]") != std::string::npos,
              "SRT second cue timestamp present (02:10)");

  // Standalone WebVTT subtitle file
  const auto vtt_path = dir / "talk.vtt";
  write_file(vtt_path,
             "WEBVTT\n\n"
             "00:00:15.000 --> 00:00:20.000\n"
             "Welcome everyone to the deep dive on systems.\n");

  auto vtt_chunks = lazylook::Extractor::extract_chunks(vtt_path);
  TEST_ASSERT(!vtt_chunks.empty(), "VTT chunks non-empty");
  TEST_ASSERT(vtt_chunks[0].find("[00:15]") != std::string::npos,
              "VTT timestamp badge generated (00:15)");
  TEST_ASSERT(vtt_chunks[0].find("Welcome everyone to the deep dive") != std::string::npos,
              "VTT cue text extracted");

  // Media companion subtitle pairing
  const auto mp4_path = dir / "lecture.mp4";
  write_file(mp4_path, "dummy media content");

  auto mp4_chunks = lazylook::Extractor::extract_chunks(mp4_path);
  TEST_ASSERT(!mp4_chunks.empty(), "MP4 extracted via companion SRT");
  TEST_ASSERT(mp4_chunks[0].find("lecture.mp4") != std::string::npos,
              "Header contains MP4 filename");
  TEST_ASSERT(mp4_chunks[0].find("[01:23]") != std::string::npos,
              "MP4 chunk has companion timestamp");

  std::cout << "  ✓ Subtitle and media companion extraction passed." << std::endl;
}

void test_image_pipeline(const std::filesystem::path &dir)
{
  std::cout << "[Test] Image & Vision pipeline extraction..." << std::endl;

  // SVG image with embedded vector graphics, title and text
  const auto svg_path = dir / "architecture.svg";
  write_file(svg_path,
             "<svg width=\"1920\" height=\"1080\" viewBox=\"0 0 1920 1080\" "
             "xmlns=\"http://www.w3.org/2000/svg\">\n"
             "  <title>System Architecture Overview</title>\n"
             "  <rect width=\"100%\" height=\"100%\" fill=\"#1e1e2e\"/>\n"
             "  <text x=\"100\" y=\"100\" fill=\"#ffffff\">Neural Search Engine</text>\n"
             "  <text x=\"100\" y=\"150\" fill=\"#89b4fa\">Vector Database Pipeline</text>\n"
             "</svg>\n");

  auto svg_chunks = lazylook::Extractor::extract_chunks(svg_path);
  TEST_ASSERT(!svg_chunks.empty(), "SVG chunks non-empty");
  TEST_ASSERT(svg_chunks[0].find("architecture.svg") != std::string::npos,
              "Chunk has SVG filename");
  TEST_ASSERT(svg_chunks[0].find("1920x1080") != std::string::npos, "SVG dimensions parsed");
  TEST_ASSERT(svg_chunks[0].find("Neural Search Engine") != std::string::npos,
              "SVG text content extracted");
  TEST_ASSERT(svg_chunks[0].find("Vector Database Pipeline") != std::string::npos,
              "SVG vector text extracted");

  // BMP image header creation
  const auto bmp_path = dir / "sample.bmp";
  // Create minimal BMP file
  const std::vector<unsigned char> bmp_bytes = {
      // BMP Header
      'B',
      'M',
      70,
      0,
      0,
      0, // File size header
      0,
      0,
      0,
      0, // reserved
      54,
      0,
      0,
      0, // pixel offset
      // DIB Header BITMAPINFOHEADER
      40,
      0,
      0,
      0, // header size
      2,
      0,
      0,
      0, // Width specification
      2,
      0,
      0,
      0, // Height specification
      1,
      0, // Color planes specification
      24,
      0, // Bits per pixel specification
      0,
      0,
      0,
      0, // compression = none
      16,
      0,
      0,
      0, // Image size specification
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      // Pixel buffer with row padding
      255,
      0,
      0,
      0,
      255,
      0,
      0,
      0, // First row
      0,
      0,
      255,
      255,
      255,
      255,
      0,
      0 // Second row
  };
  std::ofstream bmp_out(bmp_path, std::ios::binary);
  bmp_out.write(reinterpret_cast<const char *>(bmp_bytes.data()),
                static_cast<std::streamsize>(bmp_bytes.size()));
  bmp_out.close();

  auto bmp_chunks = lazylook::Extractor::extract_chunks(bmp_path);
  TEST_ASSERT(!bmp_chunks.empty(), "BMP chunks non-empty");
  TEST_ASSERT(bmp_chunks[0].find("sample.bmp") != std::string::npos, "BMP filename in chunk");
  TEST_ASSERT(bmp_chunks[0].find("BMP") != std::string::npos, "BMP format detected");
  TEST_ASSERT(bmp_chunks[0].find("2x2") != std::string::npos, "BMP dimensions 2x2 extracted");

  // SVG with single quotes in attributes
  const auto single_quote_svg = dir / "single_quote.svg";
  write_file(single_quote_svg,
             "<svg xmlns='http://www.w3.org/2000/svg' width='640' height='480'>\n"
             "  <title>Architecture Overview</title>\n"
             "</svg>\n");
  auto sq_meta = lazylook::ImageHandler::extract_metadata(single_quote_svg);
  TEST_ASSERT(sq_meta.width == 640 && sq_meta.height == 480,
              "Extracted SVG dimensions with single quotes");
  TEST_ASSERT(sq_meta.text_content.find("Architecture Overview") != std::string::npos,
              "Extracted title text from single quote SVG");

  std::cout << "  ✓ Image and vision pipeline extraction passed." << std::endl;
}

void test_pipeline_architecture()
{
  std::cout << "[Test] OOP Pipeline Architecture and Extensibility..." << std::endl;

  auto &registry = lazylook::HandlerRegistry::instance();

  // Verify categories
  TEST_ASSERT(registry.get_category("doc.pdf") == lazylook::FileCategory::Document,
              "PDF is Document");
  TEST_ASSERT(registry.get_category("photo.jpg") == lazylook::FileCategory::Image, "JPG is Image");
  TEST_ASSERT(registry.get_category("movie.mp4") == lazylook::FileCategory::Media, "MP4 is Media");

  // Verify Open/Closed Principle: Add a custom handler dynamically
  class CustomShaderHandler : public lazylook::IFileHandler
  {
  public:
    lazylook::FileCategory category() const override { return lazylook::FileCategory::Document; }
    std::string_view name() const override { return "CustomShaderHandler"; }
    bool supports(const std::filesystem::path &path) const override
    {
      return path.extension() == ".shader";
    }
    lazylook::ProcessedFile process(const std::filesystem::path &path) override
    {
      lazylook::ProcessedFile pf;
      pf.path = path;
      pf.category = category();
      pf.chunks.push_back(lazylook::ProcessedChunk{
          path.filename().string() + "\n[Custom Shader] vec4 fragment main()", 0, {}});
      return pf;
    }
  };

  TEST_ASSERT(!registry.is_supported("lighting.shader"),
              "Before registration, .shader unsupported");
  registry.register_handler(std::make_unique<CustomShaderHandler>());
  TEST_ASSERT(registry.is_supported("lighting.shader"), "After registration, .shader supported");

  auto *handler = registry.find_handler("lighting.shader");
  TEST_ASSERT(handler != nullptr, "Handler found for .shader");
  TEST_ASSERT(handler->name() == "CustomShaderHandler", "Custom handler dispatched correctly");

  auto res = handler->process("lighting.shader");
  TEST_ASSERT(!res.chunks.empty() && res.chunks[0].text.find("fragment main") != std::string::npos,
              "Custom handler processed successfully");

  std::cout << "  ✓ OOP Pipeline Registry and polymorphic extension passed." << std::endl;
}

void test_video_pipeline(const std::filesystem::path &dir)
{
  std::cout << "[Test] Video Pipeline: Chapters, Timecodes, and Thumbnails..." << std::endl;

  // Timecode formatting
  TEST_ASSERT(lazylook::MediaHandler::format_timecode(0) == "00:00", "0s -> 00:00");
  TEST_ASSERT(lazylook::MediaHandler::format_timecode(59) == "00:59", "59s -> 00:59");
  TEST_ASSERT(lazylook::MediaHandler::format_timecode(83) == "01:23", "83s -> 01:23");
  TEST_ASSERT(lazylook::MediaHandler::format_timecode(3665) == "01:01:05", "3665s -> 01:01:05");

  // Chapter parsing from ffmetadata
  std::string metadata_sample = ";FFMETADATA1\n"
                                "title=Engineering Presentation\n"
                                "[CHAPTER]\n"
                                "TIMEBASE=1/1000\n"
                                "START=0\n"
                                "END=30000\n"
                                "title=Overview and Goals\n"
                                "[CHAPTER]\n"
                                "TIMEBASE=1/1000\n"
                                "START=30000\n"
                                "END=95000\n"
                                "title=Architecture Deep Dive\n"
                                "[CHAPTER]\n"
                                "TIMEBASE=1/1\n"
                                "START=120\n"
                                "END=180\n"
                                "title=Q&A and Next Steps\n";

  auto chapters = lazylook::MediaHandler::parse_ffmetadata_chapters(metadata_sample);
  TEST_ASSERT(chapters.size() == 3, "Parsed 3 chapters from metadata");
  TEST_ASSERT(chapters[0].start_seconds == 0, "Chapter 0 starts at 0s");
  TEST_ASSERT(chapters[0].title == "Overview and Goals", "Chapter 0 title matches");
  TEST_ASSERT(chapters[1].start_seconds == 30, "Chapter 1 starts at 30s");
  TEST_ASSERT(chapters[1].title == "Architecture Deep Dive", "Chapter 1 title matches");
  TEST_ASSERT(chapters[2].start_seconds == 120, "Chapter 2 starts at 120s");
  TEST_ASSERT(chapters[2].title == "Q&A and Next Steps", "Chapter 2 title matches");

  // Thumbnails directory resolution
  auto thumb_dir = lazylook::MediaHandler::thumbnails_directory();
  TEST_ASSERT(!thumb_dir.empty(), "Thumbnails directory path non-empty");
  std::error_code ec;
  TEST_ASSERT(std::filesystem::exists(thumb_dir, ec), "Thumbnails directory exists on disk");

  // Video processing with companion subtitles
  const auto test_vid = dir / "clip.mp4";
  write_file(test_vid, "dummy mp4 header");
  const auto test_sub = dir / "clip.srt";
  write_file(test_sub,
             "1\n00:00:10,000 --> 00:00:15,000\nFirst video segment.\n\n"
             "2\n00:01:45,000 --> 00:01:50,000\nSecond video segment.\n");

  lazylook::MediaHandler handler;
  auto processed = handler.process(test_vid);
  TEST_ASSERT(!processed.chunks.empty(), "Processed video with companion subtitles has chunks");
  TEST_ASSERT(processed.chunks[0].text.find("clip.mp4") != std::string::npos,
              "Chunk has video filename");
  TEST_ASSERT(processed.chunks[0].page_or_timestamp == 10, "First chunk timestamp is 10s");

  // Robust subtitle parsing edge cases: markup tags and missing blank line
  std::string srt_edge_case = "1\n"
                              "00:00:05,500 --> 00:00:09,000\n"
                              "<i>Welcome to</i> <b>LazyLook</b>!\n"
                              "2\n"
                              "00:00:10,000 --> 00:00:15,000\n"
                              "<v Speaker>Direct speech without blank line.</v>\n";
  auto edge_cues = lazylook::MediaHandler::parse_subtitles(srt_edge_case);
  TEST_ASSERT(edge_cues.size() == 2, "Parsed 2 cues despite missing blank line");
  TEST_ASSERT(edge_cues[0].text == "Welcome to LazyLook!", "Stripped HTML tags from cue 0");
  TEST_ASSERT(edge_cues[1].text == "Direct speech without blank line.",
              "Stripped VTT tags from cue 1");
  TEST_ASSERT(edge_cues[1].seconds == 10, "Cue 1 timestamp is 10s");

  // Untitled video chapters
  std::string untitled_metadata = ";FFMETADATA1\n"
                                  "[CHAPTER]\n"
                                  "TIMEBASE=1/1000\n"
                                  "START=5000\n"
                                  "END=15000\n";
  auto untitled_chaps = lazylook::MediaHandler::parse_ffmetadata_chapters(untitled_metadata);
  TEST_ASSERT(untitled_chaps.size() == 1, "Parsed untitled chapter");
  TEST_ASSERT(untitled_chaps[0].title == "Chapter 1", "Untitled chapter assigned default name");
  TEST_ASSERT(untitled_chaps[0].start_seconds == 5, "Untitled chapter starts at 5s");

  // WebVTT subtitle parsing with dot timecodes and mm:ss format
  std::string vtt_sample = "WEBVTT\n\n"
                           "NOTE This is an editorial note\n\n"
                           "00:01.250 --> 00:03.500\n"
                           "First WebVTT cue line.\n\n"
                           "01:15.000 --> 01:20.000\n"
                           "Second WebVTT cue line.\n";
  auto vtt_cues = lazylook::MediaHandler::parse_subtitles(vtt_sample);
  TEST_ASSERT(vtt_cues.size() == 2, "Parsed 2 WebVTT cues");
  TEST_ASSERT(vtt_cues[0].seconds == 1, "WebVTT cue 0 at 1s");
  TEST_ASSERT(vtt_cues[0].text == "First WebVTT cue line.", "WebVTT cue 0 text");
  TEST_ASSERT(vtt_cues[1].seconds == 75, "WebVTT cue 1 at 75s (01:15)");
  TEST_ASSERT(vtt_cues[1].badge == "01:15", "WebVTT cue 1 badge matches");

  // Real synthetic video generation and thumbnail extraction
  const auto synth_vid = dir / "synthetic.mp4";
  const std::string gen_cmd =
      "ffmpeg -f lavfi -i testsrc=duration=4:size=320x240:rate=10 -f lavfi -i "
      "sine=frequency=1000:duration=4 -c:v libx264 -c:a aac -pix_fmt yuv420p -y " +
      shell_escape(synth_vid.string()) + " 2>/dev/null";
  int gen_ret = std::system(gen_cmd.c_str());
  if (gen_ret == 0 && std::filesystem::exists(synth_vid)) {
    // Probe real video metadata
    auto meta = lazylook::MediaHandler::probe_metadata(synth_vid);
    TEST_ASSERT(meta.duration_seconds >= 3, "Synthetic video duration probed >= 3s");
    TEST_ASSERT(meta.width == 320 && meta.height == 240, "Synthetic video resolution is 320x240");
    TEST_ASSERT(meta.codec.find("h264") != std::string::npos, "Synthetic video codec is h264");

    // Generate real keyframe thumbnail
    std::string thumb = lazylook::MediaHandler::generate_keyframe_thumbnail(synth_vid, 1);
    TEST_ASSERT(!thumb.empty(), "Generated thumbnail path non-empty");
    std::error_code ec;
    TEST_ASSERT(std::filesystem::exists(thumb, ec), "Thumbnail JPEG exists on disk");
    TEST_ASSERT(std::filesystem::file_size(thumb, ec) > 100, "Thumbnail file size > 100 bytes");

    // Verify thumbnail caching (second call returns existing file immediately)
    std::string cached_thumb = lazylook::MediaHandler::generate_keyframe_thumbnail(synth_vid, 1);
    TEST_ASSERT(cached_thumb == thumb, "Thumbnail cache hit returned identical path");

    // Process synthetic video without subtitles -> verifies scene keyframe chunks
    auto vid_processed = handler.process(synth_vid);
    TEST_ASSERT(!vid_processed.chunks.empty(),
                "Video without subtitles produces scene keyframe chunks");
    TEST_ASSERT(vid_processed.chunks[0].text.find("Video Scene") != std::string::npos,
                "Scene description present in chunk");
    TEST_ASSERT(!vid_processed.chunks[0].thumbnail_path.empty(),
                "Scene chunk has generated thumbnail path");

    // Test embedded subtitle stream extraction via ffmpeg
    const auto synth_sub = dir / "embedded.srt";
    write_file(synth_sub, "1\n00:00:01,000 --> 00:00:03,000\nEmbedded subtitle stream verified.\n");
    const auto synth_with_sub = dir / "synth_with_sub.mp4";
    const std::string mux_cmd = "ffmpeg -i " + shell_escape(synth_vid.string()) + " -i " +
                                shell_escape(synth_sub.string()) +
                                " -c:v copy -c:a copy -c:s mov_text -y " +
                                shell_escape(synth_with_sub.string()) + " 2>/dev/null";
    if (std::system(mux_cmd.c_str()) == 0 && std::filesystem::exists(synth_with_sub)) {
      auto mux_processed = handler.process(synth_with_sub);
      TEST_ASSERT(!mux_processed.chunks.empty(), "Muxed video has extracted chunks");
      bool found_embedded = false;
      for (const auto &chunk : mux_processed.chunks) {
        if (chunk.text.find("Embedded subtitle stream verified") != std::string::npos) {
          found_embedded = true;
          TEST_ASSERT(chunk.page_or_timestamp == 1, "Embedded cue timestamp is 1s");
          break;
        }
      }
      TEST_ASSERT(found_embedded, "Embedded subtitle stream successfully extracted via ffmpeg");
    }
  }

  std::cout << "  ✓ Video pipeline chapters, timecodes, and metadata passed." << std::endl;
}

void test_audio_pipeline(const std::filesystem::path &dir)
{
  std::cout << "[Test] Audio Pipeline: Slicing, Windowing, and Speech Hooks..." << std::endl;

  // Audio window slicing with overlapping hop
  auto slices = lazylook::MediaHandler::slice_audio_windows("fake.mp3", 95);
  TEST_ASSERT(slices.size() == 6, "Sliced 95s audio into 6 overlapping windows");
  TEST_ASSERT(slices[0].start_seconds == 0 && slices[0].end_seconds == 30, "Window 0: 0-30s");
  TEST_ASSERT(slices[0].badge == "00:00", "Window 0 badge is 00:00");
  TEST_ASSERT(slices[1].start_seconds == 15 && slices[1].end_seconds == 45, "Window 1: 15-45s");
  TEST_ASSERT(slices[1].badge == "00:15", "Window 1 badge is 00:15");
  TEST_ASSERT(slices[5].start_seconds == 75 && slices[5].end_seconds == 95, "Window 5: 75-95s");

  // Synthetic audio file processing with ffmpeg
  const auto synth_audio = dir / "podcast.mp3";
  const std::string gen_audio =
      "ffmpeg -f lavfi -i sine=frequency=440:duration=5 -c:a libmp3lame -y " +
      shell_escape(synth_audio.string()) + " 2>/dev/null";
  if (std::system(gen_audio.c_str()) == 0 && std::filesystem::exists(synth_audio)) {
    lazylook::MediaHandler handler;
    auto processed = handler.process(synth_audio);
    TEST_ASSERT(!processed.chunks.empty(), "Processed audio has chunks");
    TEST_ASSERT(processed.chunks[0].text.find("podcast.mp3") != std::string::npos,
                "Chunk has audio filename");
    TEST_ASSERT(processed.category == lazylook::FileCategory::Media, "Category is Media");
  }

  std::cout << "  ✓ Audio pipeline slicing and windowing passed." << std::endl;
}

void test_edge_cases(const std::filesystem::path &dir)
{
  std::cout << "[Test] Edge cases and safety..." << std::endl;

  // Non-existent file handling
  auto missing_chunks = lazylook::Extractor::extract_chunks(dir / "non_existent.txt");
  TEST_ASSERT(missing_chunks.empty(), "Non-existent file returns empty safely");

  // Zero-byte file handling
  const auto empty_path = dir / "empty.txt";
  write_file(empty_path, "");
  auto empty_chunks = lazylook::Extractor::extract_chunks(empty_path);
  TEST_ASSERT(empty_chunks.empty(), "0-byte file returns empty safely");

  // File path with spaces, symbols, and brackets
  auto special_chunks = lazylook::Extractor::extract_chunks(dir / "special [name] & spaces.docx");
  TEST_ASSERT(!special_chunks.empty(), "Path with spaces and symbols extracts properly");
  TEST_ASSERT(special_chunks[0].find("Content in escaped filename.") != std::string::npos,
              "Escaped path content extracted");

  // Binary unreadable file fallback
  const auto bin_path = dir / "mystery.txt";
  std::string binary_data = "header\0\1\2\3binary_data"s;
  write_file(bin_path, binary_data);
  auto bin_chunks = lazylook::Extractor::extract_chunks(bin_path);
  TEST_ASSERT(!bin_chunks.empty(), "Binary file with supported ext has fallback");
  TEST_ASSERT(bin_chunks[0] == "mystery.txt", "Binary file falls back to filename for indexing");

  // Oversized file handling
  const auto oversized_path = dir / "large.txt";
  {
    std::ofstream out(oversized_path, std::ios::binary);
    out.seekp(21 * 1024 * 1024);
    out.write("X", 1);
  }
  auto oversized_chunks = lazylook::Extractor::extract_chunks(oversized_path);
  TEST_ASSERT(oversized_chunks.empty(), "Oversized file rejected before reading");

  std::cout << "  ✓ Edge cases and safety passed." << std::endl;
}

} // namespace

int main()
{
  std::cout << "========================================" << std::endl;
  std::cout << "LazyLook Extractor & Multi-Format Test  " << std::endl;
  std::cout << "========================================" << std::endl;

  test_is_supported();

  const auto temp_dir = create_test_directory();
  setup_test_archives(temp_dir);

  test_text_and_code(temp_dir);
  test_html_extraction(temp_dir);
  test_rtf_extraction(temp_dir);
  test_office_formats(temp_dir);
  test_subtitles_and_media(temp_dir);
  test_video_pipeline(temp_dir);
  test_audio_pipeline(temp_dir);
  test_image_pipeline(temp_dir);
  test_pipeline_architecture();
  test_edge_cases(temp_dir);

  // Clean up
  std::error_code ec;
  std::filesystem::remove_all(temp_dir, ec);

  std::cout << "========================================" << std::endl;
  std::cout << "ALL EXTRACTOR TESTS PASSED SUCCESSFULLY!" << std::endl;
  std::cout << "========================================" << std::endl;

  return 0;
}
